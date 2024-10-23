#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

// Function to parse a single shell command from the command line input
void parse_shell_command(char *command_line, ShellCommand *cmd);

// Function to split the command line into multiple piped commands
int split_piped_commands(char *command_line, char *piped_commands[], int max_pipes);

// Function to skip leading whitespace characters in the given string
void skip_leading_whitespace(char **str);

// Function to extract the filename for input/output redirection from the command line
char *parse_redirection_filename(char **str);

// Function to parse a single argument from the command line and stores it in the buffer
void parse_single_argument(char **str, char *buffer);

#endif
