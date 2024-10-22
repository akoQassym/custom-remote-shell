#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "commands.h"
#include "utils.h"

#define PORT 8080
#define BUFFER_SIZE 1024

int main(void) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create and configure server socket
    server_socket = create_server_socket(PORT, &server_addr);

    // Listen for client connections
    listen_for_connections(server_socket);

    // Accept client connections
    while ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len)) > 0) {
        printf("Client connected\n");
        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            int read_size = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (read_size <= 0) break;

            buffer[read_size] = '\0';
            printf("Received: \"%s\"\n", buffer); // Print received command

            // Check for exit command
            if (strcmp(buffer, "exit") == 0) {
                printf("Client requested to end the connection.\n");
                break;
            }

            // Execute the command and send the output back
            execute_command(buffer, client_socket);
        }
        printf("Client disconnected\n");
        close(client_socket);
    }

    close(server_socket);
    return 0;
}
