#ifndef COMMANDS_H
#define COMMANDS_H

#include "shell.h"

void execute_single_command(ShellCommand *cmd);
void execute_piped_commands(ShellCommand **commands, int command_count);
int is_built_in_command(ShellCommand *cmd);

#endif
