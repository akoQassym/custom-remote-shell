#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utilities.h"

#define PORT 8080          // Port number to connect to
#define BUFFER_SIZE 1024   // Buffer size for sending and receiving data

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create and connect the client socket
    client_socket = create_client_socket(PORT, "127.0.0.1", &server_addr);
    if (client_socket < 0) {
        fprintf(stderr, "Failed to connect to the server.\n");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Enter commands (type 'exit' to quit):\n");

    while (1) {
        printf("client_shell> ");  // Display prompt
        fflush(stdout); // Ensure prompt is displayed immediately

        // Read user input from stdin
        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            // Handle EOF (e.g., Ctrl+D)
            printf("\nEOF detected. Exiting.\n");
            break;
        }

        // Remove the trailing newline character
        buffer[strcspn(buffer, "\n")] = '\0';

        // Send the command to the server
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            break;
        }

        // If the user types 'exit', close the connection
        if (strcmp(buffer, "exit") == 0) {
            printf("Closing connection.\n");
            break;
        }

        // Receive and display the server's response
        while (1) {
            memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received < 0) {
                perror("Receive failed");
                close(client_socket);
                exit(EXIT_FAILURE);
            } else if (bytes_received == 0) {
                // Connection closed by the server
                printf("Server closed the connection.\n");
                close(client_socket);
                exit(EXIT_SUCCESS);
            }

            buffer[bytes_received] = '\0';  // Null-terminate the received data

            // Check for the end-of-output signal
            if (strcmp(buffer, "__END__") == 0) {
                break;
            }

            // Print the received data
            printf("%s", buffer);
        }
    }

    // Close the client socket
    close(client_socket);
    return 0;
}
