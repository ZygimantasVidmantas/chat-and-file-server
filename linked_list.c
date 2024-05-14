#include <unistd.h>

struct client {
    int descriptor;
    struct client *next, *previous;
} typedef client_node;

client_node *add_client(int client_socket, client_node *catalogue_back) {
    struct client *client = (struct client*)malloc(sizeof(struct client));
    client->descriptor = client_socket;

    client->next = catalogue_back;
    client->previous = catalogue_back->previous;
    catalogue_back->previous->next = client;
    catalogue_back->previous = client;

    return client;
}

void remove_client(client_node *client) {
    client->previous->next = client->next;
    client->next->previous = client->previous;

    if (client->descriptor != 0)
        close(client->descriptor);

    free(client);
}