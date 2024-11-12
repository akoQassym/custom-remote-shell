#ifndef UTILITIES_H
#define UTILITIES_H

#include <netinet/in.h>

// Function to display the shell prompt
void display_shell_prompt();

// Function to read user input from stdin
int read_user_input(char *command_line);

// Function to create and connect a client socket
int create_client_socket(int port, const char *ip, struct sockaddr_in *server_addr);

// Function to create a server socket
int create_server_socket(int port, struct sockaddr_in *server_addr);

// Function to bind and listen on the server socket
int setup_server_socket(int server_socket, struct sockaddr_in *server_addr, int backlog);

#endif
