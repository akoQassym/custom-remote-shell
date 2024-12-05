#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define BUFFER_SIZE 1024

// Global task queue
TaskQueue task_queue = {NULL, NULL};
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t task_cond = PTHREAD_COND_INITIALIZER;

// Initialize the scheduler
void init_scheduler() {
    pthread_mutex_lock(&queue_mutex);
    task_queue.front = task_queue.rear = NULL;
    pthread_mutex_unlock(&queue_mutex);
}

// Add a task to the queue
void add_task(Task *task) {
    pthread_mutex_lock(&queue_mutex);

    if (task_queue.rear == NULL) {
        task_queue.front = task_queue.rear = task;
    } else {
        task_queue.rear->next = task;
        task_queue.rear = task;
    }

    task->next = NULL; // Ensure the requeued task points to NULL
    pthread_cond_signal(&task_cond); // Signal that a task is available
    pthread_mutex_unlock(&queue_mutex);
}

// Select the next task to execute
Task *get_next_task() {
    pthread_mutex_lock(&queue_mutex);

    while (task_queue.front == NULL) {
        pthread_cond_wait(&task_cond, &queue_mutex);
    }

    Task *prev = NULL;
    Task *current = task_queue.front;
    Task *selected_task = NULL;
    Task *selected_prev = NULL;

    // Apply RR for the first round
    while (current != NULL) {
        if (selected_task == NULL || current->round_count == 0) {
            selected_task = current;
            selected_prev = prev;
            break; // Prioritize first round tasks
        }
        prev = current;
        current = current->next;
    }

    // Apply SJF after the first round
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

    // Remove selected task from queue
    if (selected_task != NULL) {
        if (selected_prev == NULL) {
            task_queue.front = selected_task->next;
        } else {
            selected_prev->next = selected_task->next;
        }
        if (selected_task == task_queue.rear) {
            task_queue.rear = selected_prev;
        }
    }

    pthread_mutex_unlock(&queue_mutex);
    return selected_task;
}


// Execute a task
void execute_task(Task *task) {
    // Determine the quantum based on the task's round count
    int quantum = (task->round_count == 0) ? 3 : 7;
    int elapsed_time = 0;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // If this is the first time executing the task, create the child process
    if (task->pid == 0) {
        if (pipe(task->pipe_fd) == -1) {
            perror("Pipe creation failed");
            return;
        }

        task->pid = fork();
        if (task->pid == 0) {
            // Child process
            close(task->pipe_fd[0]); // Close read end in child
            dup2(task->pipe_fd[1], STDOUT_FILENO); // Redirect stdout to pipe
            dup2(task->pipe_fd[1], STDERR_FILENO); // Redirect stderr to pipe
            close(task->pipe_fd[1]);

            execlp("/bin/sh", "sh", "-c", task->command, NULL);
            perror("Execution failed");
            exit(EXIT_FAILURE);
        } else if (task->pid < 0) {
            perror("Fork failed");
            return;
        }
        close(task->pipe_fd[1]); // Close write end in parent
        printf("(%d)--- started (%d)\n", task->client_id, task->burst_time);
    } else {
        kill(task->pid, SIGCONT); // Send SIGCONT to child
    }

    printf("(%d)--- running (%d)\n", task->client_id, task->remaining_time);

    // Loop for the duration of the quantum or until task is completed
    while (elapsed_time < quantum && task->remaining_time > 0) {
        // Non-blocking read from the pipe
        bytes_read = read(task->pipe_fd[0], buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            send(task->client_socket, buffer, bytes_read, 0);
            task->total_bytes_sent += bytes_read; // Accumulate total bytes sent
        }

        task->remaining_time--;
        elapsed_time++;
        sleep(1);
    }

    if (task->remaining_time > 0) {
        // Task is not completed, pause it
        printf("(%d)--- waiting (%d)\n", task->client_id, task->remaining_time);
        kill(task->pid, SIGSTOP); // Send SIGSTOP to child
        task->round_count++;      // Increment the round count
        add_task(task);           // Re-add to queue with updated state
    } else {
        // Task completed

        // Read any remaining output
        while ((bytes_read = read(task->pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\n';
            bytes_read++;
            buffer[bytes_read] = '\0';
            send(task->client_socket, buffer, bytes_read, 0);
            task->total_bytes_sent += bytes_read; // Accumulate total bytes sent
        }

        printf("[%d]<<< %ld bytes sent\n", task->client_id, (long)task->total_bytes_sent);
        printf("(%d)--- ended (0)\n", task->client_id);

        // Send __END__ to indicate task completion
        send(task->client_socket, "__END__", strlen("__END__"), 0);

        close(task->pipe_fd[0]);  // Close read end
        waitpid(task->pid, NULL, 0); // Wait for child to exit
        task->pid = 0;            // Reset pid
    }
}

// Scheduler thread function
void *scheduler_thread(void *arg) {
    while (1) {
        Task *task = get_next_task();
        if (task) {
            execute_task(task);
        }
    }
}
