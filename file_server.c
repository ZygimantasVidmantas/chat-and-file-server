#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "utilities.c"

#define MAX_THREADS 5
#define CLIENT_COUNT 5

pthread_mutex_t file_lock, count_lock;

struct sockaddr_in6 server_address;
int server_socket, addrlen;

int thread_count;

void handle_put(int client_socket, char *message, char *server_name) {
    remove_new_line(message);

    char *file_name = message + 5, user_name[BUFFER_SIZE] = {0};
    void *file_buffer = malloc(FILE_SIZE);

    if (recv(client_socket, user_name, BUFFER_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    int file_size = 0;
    if ((file_size = recv(client_socket, file_buffer, FILE_SIZE, 0)) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    char *directory_path = malloc(BUFFER_SIZE), *file_path = malloc(BUFFER_SIZE);

    sprintf(directory_path, "%s@%s", user_name, server_name);

    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        mkdir(directory_path, S_IRWXU | S_IRWXG | S_IRWXO);
    }
    closedir(dir);

    sprintf(file_path, "%s/%s", directory_path, file_name);

    pthread_mutex_lock(&file_lock);
    FILE *file = fopen(file_path, "wb");

    if (file == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    
    fwrite(file_buffer, 1, file_size, file);

    fclose(file);
    pthread_mutex_unlock(&file_lock);

    printf("File %s uploaded.\n", file_name);

    send_success(client_socket);

    free(file_buffer);
}

void handle_get(int client_socket, char *message) {
    char *file_name = message + 5;
    remove_new_line(file_name);

    pthread_mutex_lock(&file_lock);
    FILE *file = fopen(file_name, "rb");

    if (file == NULL) {
        send_failure(client_socket);
        pthread_mutex_unlock(&file_lock);
        return;
    }
    send_success(client_socket);
    
    void *file_buffer = calloc(FILE_SIZE, 1);
    
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    rewind(file);
    
    int size = fread(file_buffer, 1, file_size, file);
    fclose(file);

    pthread_mutex_unlock(&file_lock);
    
    if (send(client_socket, file_buffer, size, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    return;
}

void handle_request(int client_socket, char *message, char *server_name) {
    printf("%s: %s", server_name, message);

    if (strncmp(message, "@put ", 5) == 0)
        handle_put(client_socket, message, server_name);

    if (strncmp(message, "@get ", 5) == 0)
        handle_get(client_socket, message);
}

void *handle_connection() {
    int client_socket;

    if ((client_socket = accept(server_socket, (struct sockaddr *)&server_address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char name[BUFFER_SIZE] = {0};

    if (recv(client_socket, name, BUFFER_SIZE, 0) < 0) {
        perror("recv");
        exit(EXIT_FAILURE);
    }

    printf("Server %s connected.\n", name);

    while(1) {
        char buffer[BUFFER_SIZE] = {0};

        if (recv(client_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        handle_request(client_socket, buffer, name);
    }

    printf("Server %s disconnected.\n", name);

    close(client_socket);
        
    pthread_mutex_lock(&count_lock);
    --thread_count;
    pthread_mutex_unlock(&count_lock);
}

int main(int argc, char *argv[]) {
    addrlen = sizeof(struct sockaddr_in6);

    printf("Server is working.\n");

    int port = atoi(argv[1]);
    if (strcmp(argv[1], "0") != 0 && port == 0)
        return 0;

    if ((server_socket = socket(AF_INET6, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int enable_reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any;
    server_address.sin6_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, addrlen) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 2) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&file_lock, 0);
    pthread_mutex_init(&count_lock, 0);

    thread_count = 0;
    while (1) {
        if (thread_count >= MAX_THREADS)
            continue;

        pthread_mutex_lock(&count_lock);
        ++thread_count;
        pthread_mutex_unlock(&count_lock);

        pthread_t thread;
        pthread_create(&thread, NULL, handle_connection, 0);
    }

    pthread_mutex_destroy(&file_lock);
    pthread_mutex_destroy(&count_lock);
    
    close(server_socket);
}