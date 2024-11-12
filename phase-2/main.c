#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "parser.h"
#include "commands.h"
#include "utilities.h"

// MAIN is used for local shell (can be started with ./shell)
void display_shell_prompt();
int read_user_input(char *command_line);

int main(void) {
    char command_line[MAX_COMMAND_LENGTH];

    while (1) {
        // Display the shell prompt
        display_shell_prompt();  

        if (!read_user_input(command_line)) {
            break; // Exit on EOF (e.g., Ctrl+D)
        }

        if (strlen(command_line) == 0) {
            continue; // Ignore empty commands
        }

        char *piped_commands[MAX_PIPED_COMMANDS + 1];
        int piped_command_count = split_piped_commands(command_line, piped_commands, MAX_PIPED_COMMANDS + 1);

        // Skip execution if there was an error in parsing
        if (piped_command_count == -1) {
            continue; 
        }

        if (piped_command_count == 1) {
            ShellCommand cmd;
            parse_shell_command(piped_commands[0], &cmd);

            if (cmd.arguments[0] == NULL) {
                continue; // No command to execute
            }

            if (!is_built_in_command(&cmd)) {
                execute_single_command(&cmd);
            }
        } else {
            ShellCommand *commands[MAX_PIPED_COMMANDS + 1];
            for (int i = 0; i < piped_command_count; i++) {
                commands[i] = malloc(sizeof(ShellCommand));
                if (commands[i] == NULL) {
                    perror("Malloc failed");
                    // Free previously allocated commands
                    for (int j = 0; j < i; j++) {
                        for (int k = 0; commands[j]->arguments[k] != NULL; k++) {
                            free(commands[j]->arguments[k]);
                        }
                        free(commands[j]);
                    }
                    break;
                }
                parse_shell_command(piped_commands[i], commands[i]);
            }
            execute_piped_commands(commands, piped_command_count);

            // Free allocated ShellCommand structures
            for (int i = 0; i < piped_command_count; i++) {
                if (commands[i] != NULL) {
                    for (int j = 0; commands[i]->arguments[j] != NULL; j++) {
                        free(commands[i]->arguments[j]);
                    }
                    free(commands[i]);
                }
            }
        }
    }
    return 0;
}
