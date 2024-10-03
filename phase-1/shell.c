#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

// Define constants for maximum sizes and limits
#define MAX_COMMAND_LENGTH 1024   // Maximum length of a single command line
#define MAX_ARGUMENTS 100         // Maximum number of arguments for a single command
#define MAX_PIPED_COMMANDS 3      // Maximum number of commands that can be piped together

// Structure to represent a single shell command
typedef struct {
    char *arguments[MAX_ARGUMENTS];  // Array to store command arguments
    char *input_file;                // Input redirection file (optional)
    char *output_file;               // Output redirection file (optional)
    int append_output;               // Flag for output append mode (1 for append, 0 for overwrite)
} ShellCommand;

// Function prototypes
void parse_shell_command(char *command_line, ShellCommand *cmd);
int is_built_in_command(ShellCommand *cmd);
void execute_single_command(ShellCommand *cmd);
void execute_piped_commands(ShellCommand **commands, int command_count);
void display_shell_prompt();
int read_user_input(char *command_line);
int split_piped_commands(char *command_line, char *piped_commands[], int max_pipes);

int main() {
    char command_line[MAX_COMMAND_LENGTH];

    while (1) {
        // Display the shell prompt and read user input
        display_shell_prompt();
        
        if (!read_user_input(command_line)) {
            break; // Exit loop if EOF (Ctrl+D) is encountered
        }

        if (strlen(command_line) == 0) {
            continue; // Skip processing for empty lines
        }

        // Split the command line into individual piped commands
        char *piped_commands[MAX_PIPED_COMMANDS + 1];
        int piped_command_count = split_piped_commands(command_line, piped_commands, MAX_PIPED_COMMANDS + 1);

        if (piped_command_count == 1) {
            // No pipes, execute as a single command
            ShellCommand cmd;
            parse_shell_command(piped_commands[0], &cmd);

            if (cmd.arguments[0] == NULL) {
                continue; // Skip if the command is empty after parsing
            }

            if (!is_built_in_command(&cmd)) {
                // If not a built-in command, execute it as an external command
                execute_single_command(&cmd);
            }
        } else {
            // Handle multiple piped commands
            ShellCommand *commands[MAX_PIPED_COMMANDS + 1];
            for (int i = 0; i < piped_command_count; i++) {
                commands[i] = malloc(sizeof(ShellCommand));
                parse_shell_command(piped_commands[i], commands[i]);
            }
            execute_piped_commands(commands, piped_command_count);
            // Free allocated memory for commands
            for (int i = 0; i < piped_command_count; i++) {
                free(commands[i]);
            }
        }
    }
    return 0;
}

// Skip leading whitespace characters in a string
void skip_leading_whitespace(char **str) {
    while (isspace((unsigned char)**str)) (*str)++;
}

// Parse redirection symbols (< or >) and return the associated filename
char *parse_redirection_filename(char **str) {
    skip_leading_whitespace(str);
    char *start = *str;
    // Find the end of the filename (next whitespace or end of string)
    while (**str != '\0' && !isspace((unsigned char)**str)) (*str)++;
    // Create a new string containing just the filename
    return strndup(start, *str - start);
}

// Parse a single argument, handling quoted strings
void parse_single_argument(char **str, char *buffer) {
    int in_quotes = 0;
    char quote_char = '\0';
    int buffer_index = 0;

    while (**str != '\0' && (in_quotes || !isspace((unsigned char)**str))) {
        if (in_quotes) {
            if (**str == quote_char) {
                // End of quoted string
                in_quotes = 0;
            } else {
                // Add character to buffer
                buffer[buffer_index++] = **str;
            }
        } else {
            if (**str == '\'' || **str == '\"') {
                // Start of quoted string
                in_quotes = 1;
                quote_char = **str;
            } else {
                // Add character to buffer
                buffer[buffer_index++] = **str;
            }
        }
        (*str)++;
    }
    buffer[buffer_index] = '\0';  // Null-terminate the argument string
}

// Parse a shell command into the ShellCommand structure
void parse_shell_command(char *command_line, ShellCommand *cmd) {
    int arg_count = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = 0;

    char *ptr = command_line;
    char buffer[MAX_COMMAND_LENGTH];

    while (*ptr != '\0') {
        skip_leading_whitespace(&ptr);
        if (*ptr == '\0') break;

        switch (*ptr) {
            case '<':
                // Input redirection
                ptr++;
                cmd->input_file = parse_redirection_filename(&ptr);
                break;
            case '>':
                // Output redirection
                ptr++;
                if (*ptr == '>') {
                    // Append mode
                    cmd->append_output = 1;
                    ptr++;
                } else {
                    // Overwrite mode
                    cmd->append_output = 0;
                }
                cmd->output_file = parse_redirection_filename(&ptr);
                break;
            default:
                // Regular argument
                parse_single_argument(&ptr, buffer);
                cmd->arguments[arg_count++] = strdup(buffer);
                break;
        }
    }
    cmd->arguments[arg_count] = NULL;  // Null-terminate the arguments array
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

// Execute a single shell command
void execute_single_command(ShellCommand *cmd) {
    pid_t child_pid = fork();
    if (child_pid == 0) {
        // Child process

        // Set up input redirection
        if (cmd->input_file) {
            int input_fd = open(cmd->input_file, O_RDONLY);
            if (input_fd < 0) {
                perror("open input file");
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
                perror("open output file");
                exit(EXIT_FAILURE);
            }
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        // Execute the command
        if (execvp(cmd->arguments[0], cmd->arguments) < 0) {
            perror("execvp");
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
            }

            // Close all pipe file descriptors
            for (int j = 0; j < 2 * (command_count - 1); j++) {
                close(pipes[j]);
            }

            // Set up input redirection for the first command (if specified)
            if (commands[i]->input_file && i == 0) {
                int input_fd = open(commands[i]->input_file, O_RDONLY);
                if (input_fd < 0) {
                    perror("open input file");
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
                    perror("open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }

            // Execute the command
            if (execvp(commands[i]->arguments[0], commands[i]->arguments) < 0) {
                perror("execvp");
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

// Display the shell prompt
void display_shell_prompt() {
    printf("my_shell> ");
    fflush(stdout);  // Ensure the prompt is immediately displayed
}

// Read user input from the command line
int read_user_input(char *command_line) {
    if (!fgets(command_line, MAX_COMMAND_LENGTH, stdin)) {
        return 0; // Return 0 on EOF (end of file)
    }
    // Remove trailing newline character
    command_line[strcspn(command_line, "\n")] = '\0';
    return 1;
}

// Split a command line into multiple piped commands
int split_piped_commands(char *command_line, char *piped_commands[], int max_pipes) {
    int pipe_count = 0;
    char *token = strtok(command_line, "|");
    while (token != NULL && pipe_count < max_pipes) {
        piped_commands[pipe_count++] = token;
        token = strtok(NULL, "|");
    }
    return pipe_count;
}