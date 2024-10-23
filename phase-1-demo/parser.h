#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

void parse_shell_command(char *command_line, ShellCommand *cmd);
int split_piped_commands(char *command_line, char *piped_commands[], int max_pipes);
void skip_leading_whitespace(char **str);
char *parse_redirection_filename(char **str);
void parse_single_argument(char **str, char *buffer);

#endif
