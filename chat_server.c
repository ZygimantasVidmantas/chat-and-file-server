#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "linked_list.c"
#include "file_sharing.c"

#define MAX_THREADS 50
#define CLIENT_COUNT 50

pthread_mutex_t catalogue_lock, count_lock, login_lock;

char names[CLIENT_COUNT][BUFFER_SIZE] = {0}, *server_name;
int online[CLIENT_COUNT] = {0}, name_count;

char *server_name;
int server_socket, file_server_socket, addrlen;
struct sockaddr_in6 server_address;

client_node *catalogue_front, *catalogue_back;
int user_count;

int thread_count;

void handle_request(int client_socket, char *message, char *user_name) {
    printf("%s: %s", user_name, message);

    if (strncmp(message, "@get ", 5) == 0)
        handle_get(client_socket, file_server_socket, message);

    else if (strncmp(message, "@put ", 5) == 0)
        handle_put(client_socket, file_server_socket, message, user_name);

    else {
        char updated_message[BUFFER_SIZE] = {0};
        sprintf(updated_message, "%s: %s", user_name, message);
	
        pthread_mutex_lock(&catalogue_lock);

        int length = strlen(updated_message);
        for (client_node *i = catalogue_front->next; i != catalogue_back; i = i->next) {
            if (i->descriptor == client_socket)
                continue;

            if (send(i->descriptor, updated_message, length, 0) < 0) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }

        pthread_mutex_unlock(&catalogue_lock);
    }
}

int handle_login(int client_socket, int id, int *position) {
    char buffer[BUFFER_SIZE] = {0};
    int contains;

	do {
		if (send(client_socket, "Input your username.\n", 22, 0) < 0) {
            perror("send");
            exit(EXIT_FAILURE);
        }

    	if (recv(client_socket, buffer, BUFFER_SIZE, 0) < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

    	if (strlen(buffer) == 0)
        	return 0;

		remove_new_line(buffer);
    	
        contains = 0;

    	pthread_mutex_lock(&login_lock);

		for (int i = 0; i < name_count; ++i)
		 	if (strcmp(buffer, names[i]) == 0) {
                *position = i;

				if (online[*position] == 0) {
                    online[*position] = 1;

                    printf("Client %i logged in as %s.\n", id, names[*position]);

                    if (send(client_socket, "Logged in.\n", 12, 0) < 0) {
                        perror("send");
                        exit(EXIT_FAILURE);
                    }

                    return 1;
                }

                send(client_socket, "A user with this name is already online.\n", 42, 0);
		 		
    	 		memset(buffer, 0, BUFFER_SIZE);
				contains = 1;
				break;
		 	}

	    pthread_mutex_unlock(&login_lock);
    } while(contains);
    
    if (name_count >= 50) {
        send(client_socket, "Maximum user limit reached.\n", 29, 0);
        
        return 0;
    }

    pthread_mutex_lock(&login_lock);

    *position = name_count++;
    strcpy(names[*position], buffer);
    online[*position] = 1;

    pthread_mutex_unlock(&login_lock);

    printf("Client %i logged in as %s.\n", id, names[*position]);
    
    if (send(client_socket, "Logged in.\n", 12, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    return 1;
}

void *handle_connection(void *args) {
    int client_socket, id = *(int *)args;

    if ((client_socket = accept(server_socket, (struct sockaddr *)&server_address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    
    pthread_mutex_lock(&catalogue_lock);
    client_node *client = add_client(client_socket, catalogue_back);
    pthread_mutex_unlock(&catalogue_lock);

    printf("Client %i connected.\n", id);
    
    char buffer[BUFFER_SIZE] = {0};
    int position = -1;

    if (!handle_login(client->descriptor, id, &position)) {
        pthread_mutex_lock(&catalogue_lock);
        printf("Client %i disconnected.\n", id);
        remove_client(client);
        pthread_mutex_unlock(&catalogue_lock);

        pthread_mutex_lock(&login_lock);
        if (position > 0)
            online[position] = 0;
        pthread_mutex_unlock(&login_lock);
        
        return 0;
    }

    memset(buffer, 0, BUFFER_SIZE);
    
    while(1) {
        int size;

        if ((size = recv(client_socket, buffer, BUFFER_SIZE, 0)) < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        if (strlen(buffer) == 1 || size == 0) {
            pthread_mutex_lock(&catalogue_lock);
            remove_client(client);
            pthread_mutex_unlock(&catalogue_lock);

            pthread_mutex_lock(&login_lock);
            online[position] = 0;
            pthread_mutex_unlock(&login_lock);
            
            printf("Client %i disconnected.\n", id);

            return 0;
        }

        handle_request(client_socket, buffer, names[position]);

        memset(buffer, 0, BUFFER_SIZE);
    }

    close(client_socket);

    pthread_mutex_lock(&login_lock);
    online[position] = 0;
    pthread_mutex_unlock(&login_lock);

    pthread_mutex_lock(&count_lock);
    --thread_count;
    pthread_mutex_unlock(&count_lock);
}

int main(int argc, char *argv[])
{
    //intialization

    addrlen = sizeof(struct sockaddr_in6);

    catalogue_front = malloc(sizeof(client_node));
    catalogue_back = malloc(sizeof(client_node));
    
    catalogue_front->next = catalogue_back;
    catalogue_back->previous = catalogue_front;

    pthread_mutex_init(&catalogue_lock, 0);
    pthread_mutex_init(&count_lock, 0);

    //server booting up

    int port = atoi(argv[1]);
    if (strcmp(argv[1], "0") != 0 && port == 0)
        return 0;

    server_name = argv[2];

    struct sockaddr_in6 file_server_address;
    connect_to_file_server(&file_server_socket, &file_server_address, argv[3], argv[2]);

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

    if (listen(server_socket, CLIENT_COUNT) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
	
    //main process

    printf("Server is running.\n");

    thread_count = 0;
    while(1) {
		if (thread_count >= MAX_THREADS)
			continue;
		
    	pthread_mutex_lock(&count_lock);
    	int id = ++thread_count;    	  
    	pthread_mutex_unlock(&count_lock);
    
	 	pthread_t thread;
	 	pthread_create(&thread, 0, handle_connection, (void *)&id);
    }

    //clean up

    pthread_mutex_destroy(&catalogue_lock);
    pthread_mutex_destroy(&count_lock);

    free(catalogue_front);
    free(catalogue_back);

    close(server_socket);
}