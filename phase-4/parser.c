#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "parser.h"

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
    int in_quotes = 0, buffer_index = 0;
    char quote_char = '\0';

    while (**str != '\0' && (in_quotes || !isspace((unsigned char)**str))) {
        if (in_quotes) {
            // End of quoted string
            if (**str == quote_char) in_quotes = 0;
            // Add character to buffer
            else buffer[buffer_index++] = **str;
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
    buffer[buffer_index] = '\0';
}

// Parse a shell command into the ShellCommand structure
void parse_shell_command(char *command_line, ShellCommand *cmd) {
    int arg_count = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->append_output = 0;
    cmd->error_file = NULL;

    char *ptr = command_line;
    char buffer[MAX_COMMAND_LENGTH];
    int first_arg = 1;  // Track if the command starts with an argument
    int empty_command_error = 0;  // Track empty command errors

    while (*ptr != '\0') {
        skip_leading_whitespace(&ptr);
        if (*ptr == '\0') break;

        if (strncmp(ptr, "2>", 2) == 0) {
            // Error redirection
            ptr += 2;
            cmd->error_file = parse_redirection_filename(&ptr);
            if (cmd->error_file == NULL || strcmp(cmd->error_file, "") == 0) {
                fprintf(stderr, "Error: Missing error redirection file.\n");
                empty_command_error = 1;
                break;  // Stop parsing
            }
        } else if (*ptr == '>') {
            // Output redirection
            ptr++;
            if (*ptr == '>') {
                // Append mode
                cmd->append_output = 1;
                ptr++;
            } else {
                cmd->append_output = 0;
            }

            skip_leading_whitespace(&ptr);

            // Check if there is an argument after '>'
            if (*ptr == '\0' || isspace((unsigned char)*ptr)) {
                fprintf(stderr, "Error: Missing output file for redirection.\n");
                empty_command_error = 1;
                break;  // Stop parsing
            }

            cmd->output_file = parse_redirection_filename(&ptr);
            if (cmd->output_file == NULL || strcmp(cmd->output_file, "") == 0) {
                fprintf(stderr, "Error: Missing output file for redirection.\n");
                empty_command_error = 1;
                break;  // Stop parsing
            }
        } else if (*ptr == '<') {
            // Input redirection
            if (first_arg && arg_count == 0) {
                // No arguments before '<'
                fprintf(stderr, "Error: Empty argument before input redirection.\n");
                empty_command_error = 1;
                break;  // Stop parsing
            }
            ptr++;
            cmd->input_file = parse_redirection_filename(&ptr);
            if (cmd->input_file == NULL || strcmp(cmd->input_file, "") == 0) {
                fprintf(stderr, "Error: Missing input file for redirection.\n");
                empty_command_error = 1;
                break;  // Stop parsing
            }
        } else {
            // Regular argument
            parse_single_argument(&ptr, buffer);
            cmd->arguments[arg_count++] = strdup(buffer);
            first_arg = 0;  // Reset the first argument flag
        }
    }

    // If there was an error during parsing, clear the command and return
    if (empty_command_error) {
        cmd->arguments[0] = NULL;  // Ensure no command is executed
        return;
    }

    // Null-terminate the arguments array
    cmd->arguments[arg_count] = NULL;
}

// Split a command line into multiple piped commands
int split_piped_commands(char *command_line, char *piped_commands[], int max_pipes) {
    int pipe_count = 0;
    char *token;
    
    // Temporary copy of command_line to check for trailing pipe
    char *temp = strdup(command_line);
    if (!temp) {
        perror("strdup");
        return -1;
    }

    // Trim trailing whitespace
    char *end = temp + strlen(temp) - 1;
    while (end >= temp && isspace((unsigned char)*end)) {
        end--;
    }

    // Check if the last non-whitespace character is a pipe
    if (end >= temp && *end == '|') {
        fprintf(stderr, "Error: Missing command after pipe.\n");
        free(temp);
        return -1;
    }
    free(temp);

    // Tokenize the command_line using strtok
    token = strtok(command_line, "|");
    while (token != NULL && pipe_count < max_pipes) {
        // Skip leading whitespace in the token
        char *cmd = token;
        skip_leading_whitespace(&cmd);

        // Check if the token is empty or contains only whitespace
        if (*cmd == '\0') {
            fprintf(stderr, "Error: Empty command between pipes.\n");
            return -1;
        }

        piped_commands[pipe_count++] = cmd;
        token = strtok(NULL, "|");
    }

    // Null-terminate the piped_commands array
    piped_commands[pipe_count] = NULL;

    return pipe_count;
}