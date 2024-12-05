#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

typedef struct Task {
    int client_id;               // Client ID
    int client_socket;           // Client's socket
    char command[1024];          // Command to execute
    int burst_time;              // Total burst time
    int remaining_time;          // Remaining burst time
    int round_count;             // Number of rounds the task has been executed
    pid_t pid;                   // PID of the child process
    int pipe_fd[2];              // Pipe file descriptors [0]: read end, [1]: write end
    ssize_t total_bytes_sent;    // Total bytes sent across all rounds
    struct Task *next;           // Pointer to the next task
} Task;

// Task queue structure
typedef struct {
    Task *front;  // Front of the queue
    Task *rear;   // Rear of the queue
} TaskQueue;

// Declare global variables as extern
extern TaskQueue task_queue;
extern pthread_mutex_t queue_mutex;

void init_scheduler();
void add_task(Task *task);
void *scheduler_thread(void *arg);
void execute_task(Task *task);

#endif
