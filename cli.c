#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <math.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define TARGET_ZONE_Y 600
#define ARROW_SPEED 300.0f
#define PERFECT_THRESHOLD 25.0f
#define GOOD_THRESHOLD 50.0f
#define MAX_ARROWS 100
#define BUFFER_SIZE 1024
#define PORT 8080

typedef struct {
    Vector2 position;
    int direction;
    bool active;
} Arrow;

typedef struct {
    int score;
    float health;
    char combo[20];
    Color laneColor;
    Arrow arrows[MAX_ARROWS];
    int arrowCount;
    int perfectPresses;
    bool isPlayer1;
} Player;

typedef struct {
    int socket;
    Player* localPlayer;
    Player* remotePlayer;
    bool* gameStarted;
    pthread_mutex_t* mutex;
} NetworkData;

void SpawnArrow(Player* player) {
    if (player->arrowCount >= MAX_ARROWS) return;
    
    Arrow* arrow = &player->arrows[player->arrowCount];
    arrow->direction = GetRandomValue(0, 3);
    arrow->active = true;
    
    float xPos = player->isPlayer1 ? SCREEN_WIDTH * 0.16f : SCREEN_WIDTH * 0.84f;
    switch (arrow->direction) {
        case 0: xPos -= 50; break;
        case 1: xPos += 50; break;
        case 2: xPos -= 150; break;
        case 3: xPos += 150; break;
    }
    arrow->position = (Vector2){ xPos, -50.0f };
    player->arrowCount++;
}

void RemoveArrow(Player* player, int index) {
    if (index < 0 || index >= player->arrowCount) return;
    for (int i = index; i < player->arrowCount - 1; i++) {
        player->arrows[i] = player->arrows[i + 1];
    }
    player->arrowCount--;
}

void* network_thread(void* arg) {
    NetworkData* data = (NetworkData*)arg;
    char buffer[BUFFER_SIZE];
    
    while (1) {
        int bytes_received = recv(data->socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;
        buffer[bytes_received] = '\0';
        
        pthread_mutex_lock(data->mutex);
        
        printf("Received: %s\n", buffer);
        
        if (strcmp(buffer, "START") == 0) {
            *data->gameStarted = true;
            printf("Both players connected! Game starting...\n");
        }
        else if (strncmp(buffer, "STATE", 5) == 0) {
            int id;
            float health;
            int score;
            sscanf(buffer, "STATE %d %f %d", &id, &health, &score);
            data->remotePlayer->health = health;
            data->remotePlayer->score = score;
        }
        else if (strcmp(buffer, "WAITING") == 0) {
            printf("Waiting for another player to join...\n");
        }
        else if (strncmp(buffer, "ID", 2) == 0) {
            int id;
            sscanf(buffer, "ID %d", &id);
            data->localPlayer->isPlayer1 = (id == 1);
            printf("Assigned as Player %d\n", id);
        }
        
        pthread_mutex_unlock(data->mutex);
    }
    return NULL;
}

void HandleInput(Player* player, Player* opponent, int socket) {
    int pressedDir = -1;
    
    if (player->isPlayer1) {
        if (IsKeyPressed(KEY_W)) pressedDir = 0;
        if (IsKeyPressed(KEY_S)) pressedDir = 1;
        if (IsKeyPressed(KEY_A)) pressedDir = 2;
        if (IsKeyPressed(KEY_D)) pressedDir = 3;
    } else {
        if (IsKeyPressed(KEY_UP)) pressedDir = 0;
        if (IsKeyPressed(KEY_DOWN)) pressedDir = 1;
        if (IsKeyPressed(KEY_LEFT)) pressedDir = 2;
        if (IsKeyPressed(KEY_RIGHT)) pressedDir = 3;
    }
    
    if (pressedDir != -1) {
        float closestDist = GOOD_THRESHOLD;
        int closestIdx = -1;
        
        for (int i = 0; i < player->arrowCount; i++) {
            if (player->arrows[i].direction == pressedDir) {
                float dist = fabsf(player->arrows[i].position.y - TARGET_ZONE_Y);
                if (dist < closestDist) {
                    closestDist = dist;
                    closestIdx = i;
                }
            }
        }
        
        if (closestIdx != -1) {
            float dist = fabsf(player->arrows[closestIdx].position.y - TARGET_ZONE_Y);
            if (dist < PERFECT_THRESHOLD) {
                player->score += 100;
                strcpy(player->combo, "PERFECT!");
                opponent->health -= 10.0f;
                player->perfectPresses++;
            } else if (dist < GOOD_THRESHOLD) {
                player->score += 50;
                strcpy(player->combo, "GOOD!");
            } else {
                strcpy(player->combo, "MISS!");
            }
            RemoveArrow(player, closestIdx);
            
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "UPDATE %.2f %d", player->health, player->score);
            send(socket, buffer, strlen(buffer), 0);
        }
    }
}

int main() {
    printf("Enter server IP: ");
    char server_ip[16];
    scanf("%s", server_ip);
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Socket creation failed\n");
        return 1;
    }
    
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        return 1;
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        printf("Connection failed\n");
        return 1;
    }
    
    printf("Connected to server! Waiting for another player...\n");
    
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Rhythm Game");
    SetTargetFPS(60);
    
    Player player1 = {0};
    Player player2 = {0};
    player1.isPlayer1 = true;
    player2.isPlayer1 = false;
    player1.health = 100.0f;
    player2.health = 100.0f;
    
    bool gameStarted = false;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    NetworkData netData = {
        .socket = sock,
        .localPlayer = &player1,
        .remotePlayer = &player2,
        .gameStarted = &gameStarted,
        .mutex = &mutex
    };
    
    pthread_t net_thread;
    pthread_create(&net_thread, NULL, network_thread, &netData);
    
    // Send READY signal after network thread is created
    printf("Sending READY signal...\n");
    send(sock, "READY", 5, 0);
    
    Texture2D upArrow = LoadTexture("darrow.png");
    Texture2D downArrow = LoadTexture("uarrow.png");
    Texture2D leftArrow = LoadTexture("larrow.png");
    Texture2D rightArrow = LoadTexture("rarrow.png");
    Texture2D background = LoadTexture("backd.png");
    
    float spawnTimer = 0.0f;
    float spawnInterval = 2.0f;
    
    while (!WindowShouldClose()) {
        pthread_mutex_lock(&mutex);
        
        if (gameStarted) {
            spawnTimer += GetFrameTime();
            if (spawnTimer >= spawnInterval) {
                SpawnArrow(&player1);
                spawnTimer = 0.0f;
            }
            
            HandleInput(&player1, &player2, sock);
            
            for (int i = 0; i < player1.arrowCount; i++) {
                player1.arrows[i].position.y += ARROW_SPEED * GetFrameTime();
                if (player1.arrows[i].position.y > SCREEN_HEIGHT) {
                    RemoveArrow(&player1, i);
                    i--;
                }
            }
        }
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        DrawTexture(background, 0, 0, WHITE);
        
        if (gameStarted) {
            for (int i = 0; i < player1.arrowCount; i++) {
                Arrow* arrow = &player1.arrows[i];
                Texture2D* texture = NULL;
                switch (arrow->direction) {
                    case 0: texture = &upArrow; break;
                    case 1: texture = &downArrow; break;
                    case 2: texture = &leftArrow; break;
                    case 3: texture = &rightArrow; break;
                }
                if (texture) {
                    DrawTexture(*texture, arrow->position.x, arrow->position.y, WHITE);
                }
            }
            
            DrawText(TextFormat("Score: %d", player1.score), 10, 10, 20, BLACK);
            DrawText(TextFormat("Health: %.0f", player1.health), 10, 40, 20, BLACK);
            DrawText(TextFormat("Combo: %s", player1.combo), 10, 70, 20, BLACK);
            
            DrawText(TextFormat("Opponent Score: %d", player2.score), SCREEN_WIDTH - 200, 10, 20, BLACK);
            DrawText(TextFormat("Opponent Health: %.0f", player2.health), SCREEN_WIDTH - 200, 40, 20, BLACK);
            
            DrawLine(0, TARGET_ZONE_Y, SCREEN_WIDTH, TARGET_ZONE_Y, RED);
        } else {
            const char* waitingText = "Waiting for another player to join...";
            int textWidth = MeasureText(waitingText, 30);
            DrawText(waitingText, SCREEN_WIDTH/2 - textWidth/2, SCREEN_HEIGHT/2, 30, BLACK);
            
            const char* controlsText = "Controls: WASD for Player 1 | Arrow Keys for Player 2";
            int controlsWidth = MeasureText(controlsText, 20);
            DrawText(controlsText, SCREEN_WIDTH/2 - controlsWidth/2, SCREEN_HEIGHT/2 + 50, 20, GRAY);
        }
        
        EndDrawing();
        
        pthread_mutex_unlock(&mutex);
    }
    
    UnloadTexture(upArrow);
    UnloadTexture(downArrow);
    UnloadTexture(leftArrow);
    UnloadTexture(rightArrow);
    UnloadTexture(background);
    
    close(sock);
    CloseWindow();
    
    return 0;
}
