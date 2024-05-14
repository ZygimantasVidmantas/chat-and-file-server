#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Utilities.c"

void connect_to_file_server(int *file_server_socket, struct sockaddr_in6 *file_server_address, char *file_server_port, char *server_name) {
    if ((*file_server_socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int port = atoi(file_server_port);
    if (strcmp(file_server_port, "0") != 0 && port == 0) {
        perror("atoi");
        exit(EXIT_FAILURE);
    }

    file_server_address->sin6_family = AF_INET6;
    file_server_address->sin6_port = htons(port);

    if (inet_pton(AF_INET6, LOOPBACK, &(*file_server_address).sin6_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    if (connect(*file_server_socket, (struct sockaddr *)file_server_address, sizeof(struct sockaddr_in6)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    if (send(*file_server_socket, server_name, strlen(server_name), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void handle_put(int client_socket, int file_server_socket, char *message, char *user_name) {
    void *file_buffer = malloc(FILE_SIZE);
    int file_size = 0;

    if ((file_size = recv(client_socket, file_buffer, FILE_SIZE, 0)) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    char display_name[BUFFER_SIZE] = {0};
    memcpy(display_name, message + 5, strlen(message + 5) - 1);

    printf("File %s received.\n", display_name);

    if (send(file_server_socket, message, strlen(message), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    usleep(100000);

    if (send(file_server_socket, user_name, strlen(user_name), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    usleep(100000);

    if (send(file_server_socket, file_buffer, file_size, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
    
    printf("File %s sent to server.\n", display_name);

    char buffer[BUFFER_SIZE]= {0};

    if (recv(file_server_socket, buffer, BUFFER_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    printf("Response received from server.\n");

    if (send(client_socket, buffer, strlen(file_buffer), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    free(file_buffer);
}

void handle_get(int client_socket, int file_server_socket, char *message) {
    if (send(file_server_socket, message, strlen(message), 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE] = {0};

    if (recv(file_server_socket, buffer, BUFFER_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    if (strcmp(buffer, "FAILURE") == 0) {
        send_failure(client_socket);
        return;
    }

    send_success(client_socket);

    void *file_buffer = malloc(FILE_SIZE);

    int file_size = 0;
    if ((file_size = recv(file_server_socket, file_buffer, FILE_SIZE, 0)) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    if (send(client_socket, file_buffer, file_size, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    free(file_buffer);
}