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

// Split screen constants
#define SPLIT_WIDTH (SCREEN_WIDTH / 2)
#define P1_OFFSET 0
#define P2_OFFSET SPLIT_WIDTH

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
    Rectangle playArea;
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
    
    float xOffset = player->isPlayer1 ? P1_OFFSET : P2_OFFSET;
    float xPos = xOffset + SPLIT_WIDTH * 0.5f;
    
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
        
        if (strcmp(buffer, "START") == 0) {
            *data->gameStarted = true;
        }
        else if (strncmp(buffer, "STATE", 5) == 0) {
            int id;
            float health;
            int score;
            sscanf(buffer, "STATE %d %f %d", &id, &health, &score);
            data->remotePlayer->health = health;
            data->remotePlayer->score = score;
        }
        
        pthread_mutex_unlock(data->mutex);
    }
    return NULL;
}

void HandleInput(Player* player, Player* opponent, int socket) {
    int pressedDir = -1;
    
    // Both players can now play simultaneously on the same keyboard
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

void DrawPlayerArea(Player* player, Texture2D* arrows, Texture2D background) {
    float xOffset = player->isPlayer1 ? P1_OFFSET : P2_OFFSET;
    
    // Draw background for this player's half
    Rectangle srcRec = { 0, 0, background.width/2, background.height };
    Rectangle destRec = { xOffset, 0, SPLIT_WIDTH, SCREEN_HEIGHT };
    DrawTexturePro(background, srcRec, destRec, (Vector2){0, 0}, 0, WHITE);
    
    // Draw dividing line
    DrawLineV(
        (Vector2){SPLIT_WIDTH, 0},
        (Vector2){SPLIT_WIDTH, SCREEN_HEIGHT},
        (Color){255, 255, 255, 128}
    );
    
    // Draw target zone line for this player
    DrawLine(xOffset, TARGET_ZONE_Y, xOffset + SPLIT_WIDTH, TARGET_ZONE_Y, RED);
    
    // Draw arrows
    for (int i = 0; i < player->arrowCount; i++) {
        Arrow* arrow = &player->arrows[i];
        Texture2D* texture = NULL;
        switch (arrow->direction) {
            case 0: texture = &arrows[0]; break;
            case 1: texture = &arrows[1]; break;
            case 2: texture = &arrows[2]; break;
            case 3: texture = &arrows[3]; break;
        }
        if (texture) {
            DrawTexture(*texture, arrow->position.x, arrow->position.y, WHITE);
        }
    }
    
    // Draw player stats
    float textX = xOffset + 10;
    DrawText(TextFormat("Score: %d", player->score), textX, 10, 20, BLACK);
    DrawText(TextFormat("Health: %.0f", player->health), textX, 40, 20, BLACK);
    DrawText(TextFormat("Combo: %s", player->combo), textX, 70, 20, BLACK);
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
    
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Rhythm Game - Split Screen");
    SetTargetFPS(60);
    
    Player player1 = {0};
    Player player2 = {0};
    player1.isPlayer1 = true;
    player2.isPlayer1 = false;
    player1.health = 100.0f;
    player2.health = 100.0f;
    
    // Set up play areas
    player1.playArea = (Rectangle){P1_OFFSET, 0, SPLIT_WIDTH, SCREEN_HEIGHT};
    player2.playArea = (Rectangle){P2_OFFSET, 0, SPLIT_WIDTH, SCREEN_HEIGHT};
    
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
    
    send(sock, "READY", 5, 0);
    
    Texture2D arrows[4] = {
        LoadTexture("darrow.png"),
        LoadTexture("uarrow.png"),
        LoadTexture("larrow.png"),
        LoadTexture("rarrow.png")
    };
    Texture2D background = LoadTexture("backd.png");
    
    float spawnTimer = 0.0f;
    float spawnInterval = 2.0f;
    
    while (!WindowShouldClose()) {
        pthread_mutex_lock(&mutex);
        
        if (gameStarted) {
            spawnTimer += GetFrameTime();
            if (spawnTimer >= spawnInterval) {
                SpawnArrow(&player1);
                SpawnArrow(&player2);
                spawnTimer = 0.0f;
            }
            
            HandleInput(&player1, &player2, sock);
            HandleInput(&player2, &player1, sock);
            
            // Update arrows
            for (int i = 0; i < player1.arrowCount; i++) {
                player1.arrows[i].position.y += ARROW_SPEED * GetFrameTime();
                if (player1.arrows[i].position.y > SCREEN_HEIGHT) {
                    RemoveArrow(&player1, i);
                    i--;
                }
            }
            
            for (int i = 0; i < player2.arrowCount; i++) {
                player2.arrows[i].position.y += ARROW_SPEED * GetFrameTime();
                if (player2.arrows[i].position.y > SCREEN_HEIGHT) {
                    RemoveArrow(&player2, i);
                    i--;
                }
            }
        }
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        if (gameStarted) {
            DrawPlayerArea(&player1, arrows, background);
            DrawPlayerArea(&player2, arrows, background);
        } else {
            DrawText("Waiting for other player...", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 20, BLACK);
        }
        
        EndDrawing();
        
        pthread_mutex_unlock(&mutex);
    }
    
    // Cleanup
    for (int i = 0; i < 4; i++) {
        UnloadTexture(arrows[i]);
    }
    UnloadTexture(background);
    
    close(sock);
    CloseWindow();
    
    return 0;
}