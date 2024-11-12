#ifndef SHELL_H
#define SHELL_H

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGUMENTS 100
#define MAX_PIPED_COMMANDS 3

typedef struct {
    char *arguments[MAX_ARGUMENTS];
    char *input_file;
    char *output_file;
    char *error_file;
    int append_output;
} ShellCommand;

#endif
