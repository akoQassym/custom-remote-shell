#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>

int create_server_socket(int port, struct sockaddr_in *server_addr);
void listen_for_connections(int server_socket);
int create_client_socket(int port, const char *ip_address, struct sockaddr_in *server_addr);

#endif
