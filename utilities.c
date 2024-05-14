#include <string.h>

#define BUFFER_SIZE 1024
#define FILE_SIZE 4096
#define LOOPBACK "::1"

void remove_new_line(char string[BUFFER_SIZE]) {
	for (int i = 0, length = strlen(string); i < length; ++i)
	 	if (string[i] == '\r' || string[i] == '\n') {
			string[i] = '\0';	 	
			return;  
	 	}
}

void check_if_successful(int socket) {
	char buffer[BUFFER_SIZE] = {0};

    if (recv(socket, buffer, BUFFER_SIZE, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }

	if (strcmp(buffer, "SUCCESS") != 0) {
        perror("File server error");
        exit(EXIT_FAILURE);
    }
}

void send_success(int socket) {
	if (send(socket, "SUCCESS", 10, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}

void send_failure(int socket) {
	if (send(socket, "FAILURE", 8, 0) < 0) {
        perror("send");
        exit(EXIT_FAILURE);
    }
}