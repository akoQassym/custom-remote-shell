#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "utilities.h"
#include "shell.h"

// Shell Utilities

// Display the shell prompt
void display_shell_prompt() {
    printf("my_shell> ");
    fflush(stdout); // Ensure the prompt is displayed immediately
}

// Read user input from stdin into the provided buffer
int read_user_input(char *command_line) {
    if (!fgets(command_line, MAX_COMMAND_LENGTH, stdin)) {
        return 0; // Return 0 on EOF (end of file) or error
    }
    command_line[strcspn(command_line, "\n")] = '\0'; // Remove the trailing newline character
    return 1;
}


// Socket Utilities

// Create and connects a client socket to the specified IP and port
int create_client_socket(int port, const char *ip, struct sockaddr_in *server_addr) {
    int sock;

    // Create a TCP socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Configure server address structure
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;         // IPv4
    server_addr->sin_port = htons(port);       // Port number

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, ip, &server_addr->sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        close(sock);
        return -1;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Connection Failed");
        close(sock);
        return -1;
    }

    return sock;
}

// Create a server socket and configures the sockaddr_in structure
int create_server_socket(int port, struct sockaddr_in *server_addr) {
    int sock;

    // Create a TCP socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Server Socket creation failed");
        return -1;
    }

    // Configure server address structure
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;         // IPv4
    server_addr->sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
    server_addr->sin_port = htons(port);       // Port number

    return sock;
}

// Bind the server socket to the specified address and starts listening
int setup_server_socket(int server_socket, struct sockaddr_in *server_addr, int backlog) {
    // Allow reuse of address and port
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return -1;
    }

    // Bind the socket to the specified address and port
    if (bind(server_socket, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return -1;
    }

    // Start listening on the socket
    if (listen(server_socket, backlog) < 0) {
        perror("Listen failed");
        close(server_socket);
        return -1;
    }

    return 0;
}
