#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024 // Buffer size for reading and sending data

// Global task queue to manage the tasks for scheduling
TaskQueue task_queue = {NULL, NULL};
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex to protect queue operations
pthread_cond_t task_cond = PTHREAD_COND_INITIALIZER;     // Condition variable to signal task availability

// Initialize the scheduler by resetting the task queue
void init_scheduler() {
    pthread_mutex_lock(&queue_mutex); // Lock the queue to ensure thread safety
    task_queue.front = task_queue.rear = NULL; // Reset queue pointers
    pthread_mutex_unlock(&queue_mutex); // Unlock after initialization
}

// Add a task to the queue
void add_task(Task *task) {
    pthread_mutex_lock(&queue_mutex); // Lock the queue to modify it

    // If the queue is empty, the new task becomes both the front and rear
    if (task_queue.rear == NULL) {
        task_queue.front = task_queue.rear = task;
    } else {
        // Add the task to the rear of the queue
        task_queue.rear->next = task;
        task_queue.rear = task;
    }

    task->next = NULL;               // Ensure the task points to NULL
    pthread_cond_signal(&task_cond); // Signal the scheduler that a new task is available
    pthread_mutex_unlock(&queue_mutex); // Unlock the queue
}

// Select the next task to execute from the queue
Task *get_next_task() {
    pthread_mutex_lock(&queue_mutex); // Lock the queue to safely access it

    // Wait until a task is available in the queue
    while (task_queue.front == NULL) {
        pthread_cond_wait(&task_cond, &queue_mutex);
    }

    // Initialize pointers for traversal and selection
    Task *prev = NULL;
    Task *current = task_queue.front;
    Task *selected_task = NULL;
    Task *selected_prev = NULL;

    // First apply Round Robin (RR) to prioritize tasks in their first round
    while (current != NULL) {
        if (current->round_count == 0) { // First round task
            selected_task = current;
            selected_prev = prev;
            break;
        }
        prev = current;
        current = current->next;
    }

    // Apply Shortest Job First (SJF) prioritization for subsequent rounds
    if (selected_task == NULL) {
        current = task_queue.front;
        prev = NULL;
        while (current != NULL) {
            if (selected_task == NULL || 
                (current->remaining_time < selected_task->remaining_time)) {
                selected_task = current;
                selected_prev = prev;
            }
            prev = current;
            current = current->next;
        }
    }

    // Remove the selected task from the queue
    if (selected_task != NULL) {
        if (selected_prev == NULL) { // Task is at the front
            task_queue.front = selected_task->next;
        } else { // Task is in the middle or end
            selected_prev->next = selected_task->next;
        }
        if (selected_task == task_queue.rear) { // Task is at the rear
            task_queue.rear = selected_prev;
        }
    }

    pthread_mutex_unlock(&queue_mutex); // Unlock the queue
    return selected_task; // Return the selected task
}

// Execute a task on the CPU
void execute_task(Task *task) {
    // Determine the quantum based on the task's round count
    int quantum = (task->round_count == 0) ? 3 : 7; // First round: 3s, subsequent rounds: 7s
    int elapsed_time = 0; // Time spent executing this task
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // If this is the first execution of the task, create its child process
    if (task->pid == 0) {
        if (pipe(task->pipe_fd) == -1) { // Create a pipe for inter-process communication
            perror("Pipe creation failed");
            return;
        }

        task->pid = fork(); // Create a child process
        if (task->pid == 0) {
            // In the child process
            close(task->pipe_fd[0]);               // Close read end in child
            dup2(task->pipe_fd[1], STDOUT_FILENO); // Redirect stdout to pipe
            dup2(task->pipe_fd[1], STDERR_FILENO); // Redirect stderr to pipe
            close(task->pipe_fd[1]);

            // Execute the command using a shell
            execlp("/bin/sh", "sh", "-c", task->command, NULL);
            perror("Execution failed"); // Only reaches here if execlp fails
            exit(EXIT_FAILURE);
        } else if (task->pid < 0) {
            perror("Fork failed");
            return;
        }
        close(task->pipe_fd[1]); // Close write end in parent
        printf("(%d)--- started (%d)\n", task->client_id, task->burst_time);
    } else {
        // Resume a paused task by sending it a SIGCONT signal
        kill(task->pid, SIGCONT);
    }

    printf("(%d)--- running (%d)\n", task->client_id, task->remaining_time);

    // Execute the task for its quantum or until it is completed
    while (elapsed_time < quantum && task->remaining_time > 0) {
        // Read output from the task's pipe
        bytes_read = read(task->pipe_fd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
            send(task->client_socket, buffer, bytes_read, 0); // Send output to client
            task->total_bytes_sent += bytes_read; // Update the total bytes sent
        }

        task->remaining_time--; // Decrease the remaining time of the task
        elapsed_time++;         // Increment the elapsed time
        sleep(1);               // Simulate execution time
    }

    if (task->remaining_time > 0) {
        // Task is not completed, pause it
        printf("(%d)--- waiting (%d)\n", task->client_id, task->remaining_time);
        kill(task->pid, SIGSTOP); // Send SIGSTOP to pause the task
        task->round_count++;      // Increment the round count
        add_task(task);           // Re-add to the queue for future execution
    } else {
        // Task completed

        // Read any remaining output from the task
        while ((bytes_read = read(task->pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\n'; // Append a newline to the output
            bytes_read++;
            buffer[bytes_read] = '\0'; // Null-terminate the output
            send(task->client_socket, buffer, bytes_read, 0); // Send to client
            task->total_bytes_sent += bytes_read; // Update the total bytes sent
        }

        printf("[%d]<<< %ld bytes sent\n", task->client_id, (long)task->total_bytes_sent);
        printf("(%d)--- ended (0)\n", task->client_id);

        // Notify the client that the task has completed
        send(task->client_socket, "__END__", strlen("__END__"), 0);

        close(task->pipe_fd[0]);  // Close the pipe
        waitpid(task->pid, NULL, 0); // Wait for the child process to exit
        task->pid = 0;            // Reset the task's PID
    }
}

// Scheduler thread function to continuously execute tasks
void *scheduler_thread(void *arg) {
    while (1) {
        Task *task = get_next_task(); // Fetch the next task to execute
        if (task) {
            execute_task(task); // Execute the task
        }
    }
}
