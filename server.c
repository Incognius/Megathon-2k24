#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 8080
#define MAX_CLIENTS 2
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    int id;
    float health;
    int score;
    bool ready;
} Client;

typedef struct {
    Client* clients[MAX_CLIENTS];  // Array of Client pointers
    int client_count;
    pthread_mutex_t mutex;
    bool game_started;
} GameState;

GameState game_state = {0};

void broadcast_message(const char* message) {
    printf("Broadcasting: %s\n", message);
    for (int i = 0; i < game_state.client_count; i++) {
        send(game_state.clients[i]->socket, message, strlen(message), 0);
    }
}

void broadcast_game_state() {
    char buffer[BUFFER_SIZE];
    for (int i = 0; i < game_state.client_count; i++) {
        for (int j = 0; j < game_state.client_count; j++) {
            if (i != j) {
                snprintf(buffer, BUFFER_SIZE, "STATE %d %.2f %d", 
                    game_state.clients[j]->id,
                    game_state.clients[j]->health,
                    game_state.clients[j]->score);
                send(game_state.clients[i]->socket, buffer, strlen(buffer), 0);
            }
        }
    }
}

void check_game_start() {
    printf("Checking game start. Clients: %d\n", game_state.client_count);

    if (game_state.client_count == MAX_CLIENTS) {
        bool all_ready = true;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!game_state.clients[i]->ready) {
                all_ready = false;
                printf("Client %d not ready\n", game_state.clients[i]->id);
                break;
            }
        }

        if (all_ready && !game_state.game_started) {
            game_state.game_started = true;
            printf("All players ready! Starting game...\n");
            broadcast_message("START");
        }
    } else if (!game_state.game_started) {
        printf("Waiting for more players...\n");
        broadcast_message("WAITING");
    }
}

void* handle_client(void* arg) {
    Client* client = (Client*)arg;
    char buffer[BUFFER_SIZE];
    
    printf("Client %d connected\n", client->id);
    snprintf(buffer, BUFFER_SIZE, "ID %d", client->id);
    send(client->socket, buffer, strlen(buffer), 0);

    while (1) {
        int bytes_received = recv(client->socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        
        pthread_mutex_lock(&game_state.mutex);
        
        printf("Received from client %d: %s\n", client->id, buffer);
        
        if (strncmp(buffer, "READY", 5) == 0) {
            client->ready = true;
            printf("Client %d is ready\n", client->id);
            check_game_start();
        }
        else if (strncmp(buffer, "UPDATE", 6) == 0) {
            float health;
            int score;
            sscanf(buffer, "UPDATE %f %d", &health, &score);
            client->health = health;
            client->score = score;
            broadcast_game_state();
        }
        
        pthread_mutex_unlock(&game_state.mutex);
    }
    
    pthread_mutex_lock(&game_state.mutex);
    printf("Client %d disconnected. Total clients: %d\n", client->id, game_state.client_count - 1);
    
    for (int i = 0; i < game_state.client_count; i++) {
        if (game_state.clients[i] == client) {
            for (int j = i; j < game_state.client_count - 1; j++) {
                game_state.clients[j] = game_state.clients[j + 1];
            }
            game_state.client_count--;
            if (game_state.client_count < MAX_CLIENTS) {
                game_state.game_started = false;
                broadcast_message("WAITING");
            }
            break;
        }
    }
    pthread_mutex_unlock(&game_state.mutex);
    
    close(client->socket);
    free(client);
    return NULL;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        return EXIT_FAILURE;
    }
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        return EXIT_FAILURE;
    }
    
    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Listen failed");
        return EXIT_FAILURE;
    }
    
    pthread_mutex_init(&game_state.mutex, NULL);
    printf("Server started on port %d\n", PORT);
    
    while (1) {
        struct sockaddr_in client_addr = {0};
        socklen_t addr_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket == -1) {
            perror("Accept failed");
            continue;
        }
        
        pthread_mutex_lock(&game_state.mutex);
        
        if (game_state.client_count >= MAX_CLIENTS) {
            const char* msg = "Server full";
            send(client_socket, msg, strlen(msg), 0);
            close(client_socket);
            pthread_mutex_unlock(&game_state.mutex);
            continue;
        }
        
        Client* new_client = malloc(sizeof(Client));
        new_client->socket = client_socket;
        new_client->id = game_state.client_count + 1;
        new_client->health = 100.0f;
        new_client->score = 0;
        new_client->ready = false;
        
        game_state.clients[game_state.client_count++] = new_client;
        printf("New client connected. Total clients: %d\n", game_state.client_count);
        
        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, new_client);
        pthread_detach(thread);
        
        check_game_start();
        pthread_mutex_unlock(&game_state.mutex);
    }
    
    pthread_mutex_destroy(&game_state.mutex);
    close(server_socket);
    return 0;
}