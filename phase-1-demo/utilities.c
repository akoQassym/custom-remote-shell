#include <stdio.h>
#include <string.h>
#include "utilities.h"
#include "shell.h"

void display_shell_prompt() {
    printf("my_shell> ");
    fflush(stdout);
}

int read_user_input(char *command_line) {
    if (!fgets(command_line, MAX_COMMAND_LENGTH, stdin)) {
        return 0; // Return 0 on EOF (end of file)
    }
    command_line[strcspn(command_line, "\n")] = '\0'; // Remove newline
    return 1;
}
