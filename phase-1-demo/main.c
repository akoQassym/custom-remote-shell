#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shell.h"
#include "parser.h"
#include "commands.h"

void display_shell_prompt();
int read_user_input(char *command_line);

int main(void) {
    char command_line[MAX_COMMAND_LENGTH];

    while (1) {
        display_shell_prompt();

        if (!read_user_input(command_line)) {
            break;
        }

        if (strlen(command_line) == 0) {
            continue;
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
                continue;
            }

            if (!is_built_in_command(&cmd)) {
                execute_single_command(&cmd);
            }
        } else {
            ShellCommand *commands[MAX_PIPED_COMMANDS + 1];
            for (int i = 0; i < piped_command_count; i++) {
                commands[i] = malloc(sizeof(ShellCommand));
                parse_shell_command(piped_commands[i], commands[i]);
            }
            execute_piped_commands(commands, piped_command_count);

            for (int i = 0; i < piped_command_count; i++) {
                free(commands[i]);
            }
        }
    }
    return 0;
}
