#ifndef SHELL_H
#define SHELL_H


#define MAX_COMMAND_LENGTH 1024 // Maximum length of a single command line input
#define MAX_ARGUMENTS 100       // Maximum number of arguments a command can have
#define MAX_PIPED_COMMANDS 3    // Maximum number of piped commands that can be handled

typedef struct {
    char *arguments[MAX_ARGUMENTS];  // Command arguments
    char *input_file;                // Input redirection file
    char *output_file;               // Output redirection file
    char *error_file;                // Error redirection file
    int append_output;               // Flag for output append mode
} ShellCommand;

#endif
