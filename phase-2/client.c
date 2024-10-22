#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "utils.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main(void) {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Create and configure client socket
    client_socket = create_client_socket(PORT, "127.0.0.1", &server_addr);

    printf("Connected to server. Enter commands:\n");
    while (1) {
        printf("> ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

        // Send command to the server
        send(client_socket, buffer, strlen(buffer), 0);

        // Check for exit command to close the connection
        if (strcmp(buffer, "exit") == 0) {
            printf("Closing connection.\n");
            break;
        }

        // Receive result from the server
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            int read_size = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (read_size <= 0) break;
            
            buffer[read_size] = '\0';
            
            // Check for end-of-output signal
            if (strcmp(buffer, "__END__") == 0) {
                break;
            }

            printf("%s", buffer);
        }
    }

    close(client_socket);
    return 0;
}
