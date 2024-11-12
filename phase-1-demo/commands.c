#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "commands.h"

// Execute a single shell command
void execute_single_command(ShellCommand *cmd) {
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process

        // Set up input redirection
        if (cmd->input_file) {
            int input_fd = open(cmd->input_file, O_RDONLY);
            if (input_fd < 0) {
                perror("Open Input File Error");
                exit(EXIT_FAILURE);
            }
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        // Set up output redirection
        if (cmd->output_file) {
            int output_fd;
            if (cmd->append_output) {
                output_fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                output_fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (output_fd < 0) {
                perror("Open Output File Error");
                exit(EXIT_FAILURE);
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        // Set up error redirection
        if (cmd->error_file) {
            int error_fd = open(cmd->error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (error_fd < 0) {
                perror("Open Error File Failure");
                exit(EXIT_FAILURE);
            }
            dup2(error_fd, STDERR_FILENO);
            close(error_fd);
        }

        // Execute the command
        if (execvp(cmd->arguments[0], cmd->arguments) < 0) {
            fprintf(stderr, "Error: Invalid command '%s'\n", cmd->arguments[0]);
            exit(EXIT_FAILURE);
        }
    } else if (child_pid > 0) {
        // Parent process
        wait(NULL);  // Wait for the child process to complete
    } else {
        perror("fork");
    }
}

// Execute a series of piped commands
void execute_piped_commands(ShellCommand **commands, int command_count) {
    int pipes[2 * (command_count - 1)];
    pid_t child_pid;

    // Create pipes for inter-process communication
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipes + i * 2) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Fork and execute each command in the pipeline
    for (int i = 0; i < command_count; i++) {
        child_pid = fork();
        if (child_pid == 0) {
            // Child process

            // Redirect input from previous pipe if not the first command
            if (i != 0) {
                dup2(pipes[(i - 1) * 2], STDIN_FILENO);
            }
            // Redirect output to next pipe if not the last command
            if (i != command_count - 1) {
                dup2(pipes[i * 2 + 1], STDOUT_FILENO);
            } else {
                // Redirect error output (stderr) to a file for the last command
                if (commands[i]->error_file) {
                    int error_fd = open(commands[i]->error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (error_fd < 0) {
                        perror("Open Error File Failure");
                        exit(EXIT_FAILURE);
                    }
                    dup2(error_fd, STDERR_FILENO);
                    close(error_fd);
                }
            }

            // Close all pipe file descriptors
            for (int j = 0; j < 2 * (command_count - 1); j++) {
                close(pipes[j]);
            }

            // Set up input redirection for the first command (if specified)
            if (commands[i]->input_file && i == 0) {
                int input_fd = open(commands[i]->input_file, O_RDONLY);
                if (input_fd < 0) {
                    perror("Open Input File Error");
                    exit(EXIT_FAILURE);
                }
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            // Set up output redirection for the last command (if specified)
            if (commands[i]->output_file && i == command_count - 1) {
                int output_fd;
                if (commands[i]->append_output) {
                    output_fd = open(commands[i]->output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
                } else {
                    output_fd = open(commands[i]->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                }
                if (output_fd < 0) {
                    perror("Open Output File Error");
                    exit(EXIT_FAILURE);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            // Execute the command
            if (execvp(commands[i]->arguments[0], commands[i]->arguments) < 0) {
                fprintf(stderr, "Error: Invalid command '%s'\n", commands[i]->arguments[0]);
                exit(EXIT_FAILURE);
            }
        } else if (child_pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipe file descriptors in the parent process
    for (int i = 0; i < 2 * (command_count - 1); i++) {
        close(pipes[i]);
    }

    // Wait for all child processes to complete
    for (int i = 0; i < command_count; i++) {
        wait(NULL);
    }
}

// Check if the command is a built-in shell command
int is_built_in_command(ShellCommand *cmd) {
    if (strcmp(cmd->arguments[0], "exit") == 0) {
        // Exit the shell
        exit(0);
    }
    if (strcmp(cmd->arguments[0], "cd") == 0) {
        // Change directory
        if (cmd->arguments[1] == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else {
            if (chdir(cmd->arguments[1]) != 0) {
                perror("cd");
            }
        }
        return 1;
    }
    return 0; // Not a built-in command
}
