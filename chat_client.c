#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "Utilities.c"

int server_socket = 0;

void handle_login() {
    char buffer[BUFFER_SIZE] = {0};

    if (recv(server_socket, buffer, BUFFER_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    printf("%s", buffer);
    do {
        memset(buffer, 0, BUFFER_SIZE);

        fgets(buffer, BUFFER_SIZE, stdin);
        if (send(server_socket, buffer, strlen(buffer) + 1, 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }
        memset(buffer, 0, BUFFER_SIZE);

        if (recv(server_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
        printf("%s", buffer);
    } while (strcmp(buffer, "A user with this name is already online.\n") == 0);

    memset(buffer, 0, BUFFER_SIZE);
}

void handle_put(char *message) {
    char filename[BUFFER_SIZE] = {0};
    strcpy(filename, message + 5);

    remove_new_line(filename);
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        printf("No such file exists.\n");
        return;
    }
    
    if (send(server_socket, message, strlen(message) + 1, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);

    void *file_buffer = malloc(FILE_SIZE);

    int size = fread(file_buffer, 1, file_size, file);
    if (send(server_socket, file_buffer, size, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
    
    char buffer[BUFFER_SIZE] = {0};

    if (recv(server_socket, buffer, BUFFER_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    if (strncmp(buffer, "SUCCESS", 8) == 0)
        printf("File uploaded");
    else printf("Upload failed");

    fclose(file);
}

void handle_get(char *message) {
    if (send(server_socket, message, strlen(message) + 1, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE] = {0};

    if (recv(server_socket, buffer, FILE_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    if (strncmp(buffer, "FAILURE", 8) == 0) {
        printf("No such file exists.\n");
        return;
    }

    void *file_buffer = calloc(FILE_SIZE, 1);

    int file_size;
    if ((file_size = recv(server_socket, file_buffer, FILE_SIZE, 0)) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }
    
    FILE *file = fopen(strrchr(message, '/') + 1, "wb");

    fwrite(file_buffer, 1, file_size, file);

    fclose(file);
}

void *handle_output(void *arg) {
    char buffer[BUFFER_SIZE] = {0};

    while (1) {
        if (recv(server_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        printf("%s", buffer);
        memset(buffer, 0, BUFFER_SIZE);
    }
}

void *handle_input(void *arg) {
    char buffer[BUFFER_SIZE] = {0};
    
    pthread_t output_thread;
    pthread_create(&output_thread, 0, handle_output, 0);

    while(1) {
        fgets(buffer, BUFFER_SIZE, stdin);

        if (strlen(buffer) == 1 && buffer[0] == '\n')
            break;

        if (strncmp(buffer, "@", 1) == 0) {
            pthread_cancel(output_thread);

            if (strncmp(buffer, "@put ", 5) == 0)
                handle_put(buffer);

            else if (strncmp(buffer, "@get ", 5) == 0)
                handle_get(buffer);

            pthread_create(&output_thread, 0, handle_output, 0);
        }
        else if (send(server_socket, buffer, strlen(buffer) + 1, 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }

        memset(buffer, 0, BUFFER_SIZE);
    }

    pthread_cancel(output_thread);

    if (send(server_socket, "", 1, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    struct sockaddr_in6 server_address;

    if ((server_socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (strcmp(argv[1], "0") != 0 && port == 0)
        return 0;

    server_address.sin6_family = AF_INET6;
    server_address.sin6_port = htons(port);

    if (inet_pton(AF_INET6, LOOPBACK, &server_address.sin6_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("connect ");
        exit(EXIT_FAILURE);
    }

    handle_login();

    pthread_t input_thread;

    pthread_create(&input_thread, 0, handle_input, 0);

    pthread_join(input_thread, 0);

    close(server_socket);

    return 0;
}