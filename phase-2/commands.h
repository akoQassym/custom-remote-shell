#ifndef COMMANDS_H
#define COMMANDS_H

#include "shell.h"

// Funciton to execute a single shell command
void execute_single_command(ShellCommand *cmd);

// Funciton to execute a series of piped commands
void execute_piped_commands(ShellCommand **commands, int command_count);

// Funciton to check if the given command is a built-in shell command
int is_built_in_command(ShellCommand *cmd);

#endif
