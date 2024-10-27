#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <raylib.h>
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
#define PERFECT_DAMAGE 10.0f

typedef enum {
    GAME_STATE_CONNECTING,
    GAME_STATE_WAITING,
    GAME_STATE_PLAYING,
    GAME_STATE_GAMEOVER
} GameState;

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
    bool ready;
} Player;

typedef struct {
    int socket;
    Player* localPlayer;
    Player* remotePlayer;
    bool* gameStarted;
    pthread_mutex_t* mutex;
    GameState* gameState;
} NetworkData;

void SpawnArrow(Player* player) {
    if (player->arrowCount >= MAX_ARROWS) return;
    
    Arrow* arrow = &player->arrows[player->arrowCount];
    arrow->direction = GetRandomValue(0, 3);
    arrow->active = true;
    
    float xPos = player->isPlayer1 ? SCREEN_WIDTH * 0.25f : SCREEN_WIDTH * 0.75f;
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
        if (bytes_received <= 0) {
            *data->gameState = GAME_STATE_GAMEOVER;
            break;
        }
        buffer[bytes_received] = '\0';
        
        pthread_mutex_lock(data->mutex);
        
        if (strcmp(buffer, "START") == 0) {
            *data->gameStarted = true;
            *data->gameState = GAME_STATE_PLAYING;
            printf("Game starting!\n");
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

void HandleInput(Player* player, Player* opponent, int socket, GameState* gameState) {
    if (*gameState == GAME_STATE_WAITING && !player->ready && IsKeyPressed(KEY_SPACE)) {
        player->ready = true;
        send(socket, "READY", 5, 0);
        printf("Player ready, waiting for other player...\n");
        return;
    }

    if (*gameState != GAME_STATE_PLAYING) return;

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
                opponent->health -= PERFECT_DAMAGE;
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

int main(void) {
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

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Rhythm Battle");
    SetTargetFPS(60);
    InitAudioDevice();

    // Load resources
    Texture2D upArrow = LoadTexture("darrow.png");
    Texture2D downArrow = LoadTexture("uarrow.png");
    Texture2D leftArrow = LoadTexture("larrow.png");
    Texture2D rightArrow = LoadTexture("rarrow.png");
    Texture2D background = LoadTexture("backd.png");
    Music gameMusic = LoadMusicStream("bloodymary.mp3");
    
    SetMusicVolume(gameMusic, 0.5f);
    
    GameState gameState = GAME_STATE_CONNECTING;
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        printf("Connection failed\n");
        return 1;
    }
    
    gameState = GAME_STATE_WAITING;
    
    Player player1 = {0};
    Player player2 = {0};
    player1.health = 100.0f;
    player2.health = 100.0f;
    player1.ready = false;
    player2.ready = false;
    
    // Receive player ID from server
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        int player_id;
        if (sscanf(buffer, "ID %d", &player_id) == 1) {
            player1.isPlayer1 = (player_id == 1);
            printf("You are Player %d\n", player_id);
        }
    }
    
    bool gameStarted = false;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    
    NetworkData netData = {
        .socket = sock,
        .localPlayer = &player1,
        .remotePlayer = &player2,
        .gameStarted = &gameStarted,
        .mutex = &mutex,
        .gameState = &gameState
    };
    
    pthread_t net_thread;
    pthread_create(&net_thread, NULL, network_thread, &netData);
    
    float spawnTimer = 0.0f;
    
    while (!WindowShouldClose()) {
        UpdateMusicStream(gameMusic);
        
        pthread_mutex_lock(&mutex);
        
        HandleInput(&player1, &player2, sock, &gameState);
        
        if (gameState == GAME_STATE_PLAYING) {
            if (!gameStarted) {
                PlayMusicStream(gameMusic);
                gameStarted = true;
            }
            
            spawnTimer += GetFrameTime();
            if (spawnTimer >= 2.0f) {
                SpawnArrow(&player1);
                spawnTimer = 0.0f;
            }
            
            // Update arrows
            for (int i = 0; i < player1.arrowCount; i++) {
                player1.arrows[i].position.y += ARROW_SPEED * GetFrameTime();
                if (player1.arrows[i].position.y > SCREEN_HEIGHT) {
                    RemoveArrow(&player1, i);
                    i--;
                }
            }
        }
        
        BeginDrawing();
        ClearBackground(BLACK);
        
        switch (gameState) {
            case GAME_STATE_CONNECTING:
                DrawText("Connecting to server...", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 20, WHITE);
                break;
                
            case GAME_STATE_WAITING:
                if (!player1.ready) {
                    DrawText("Press SPACE when ready!", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2, 20, WHITE);
                } else {
                    DrawText("Waiting for other player...", SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2, 20, WHITE);
                }
                DrawText(TextFormat("You are Player %d", player1.isPlayer1 ? 1 : 2), 
                        SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 - 50, 20, WHITE);
                break;
                
            case GAME_STATE_PLAYING:
                DrawTexture(background, 0, 0, WHITE);
                
                // Draw UI elements
                DrawText(TextFormat("Score: %d", player1.score), 20, 20, 20, WHITE);
                DrawText(TextFormat("Health: %.0f%%", player1.health), 20, 50, 20, WHITE);
                DrawText(player1.combo, 20, 80, 20, YELLOW);
                
                DrawText(TextFormat("Opponent Score: %d", player2.score), SCREEN_WIDTH - 200, 20, 20, WHITE);
                DrawText(TextFormat("Opponent Health: %.0f%%", player2.health), SCREEN_WIDTH - 200, 50, 20, WHITE);
                
                // Draw arrows
                for (int i = 0; i < player1.arrowCount; i++) {
                    Arrow* arrow = &player1.arrows[i];
                    Texture2D* arrowTexture;
                    switch (arrow->direction) {
                        case 0: arrowTexture = &upArrow; break;
                        case 1: arrowTexture = &downArrow; break;
                        case 2: arrowTexture = &leftArrow; break;
                        case 3: arrowTexture = &rightArrow; break;
                        default: continue;
                    }
                    DrawTexture(*arrowTexture, arrow->position.x, arrow->position.y, WHITE);
                }
                
                // Draw target zones
                DrawLine(0, TARGET_ZONE_Y, SCREEN_WIDTH, TARGET_ZONE_Y, RED);
                DrawLine(0, TARGET_ZONE_Y - PERFECT_THRESHOLD, SCREEN_WIDTH, TARGET_ZONE_Y - PERFECT_THRESHOLD, GREEN);
                DrawLine(0, TARGET_ZONE_Y + PERFECT_THRESHOLD, SCREEN_WIDTH, TARGET_ZONE_Y + PERFECT_THRESHOLD, GREEN);
                break;
                
            case GAME_STATE_GAMEOVER:
                DrawText("Game Over!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 40, WHITE);
                DrawText(TextFormat("Final Score: %d", player1.score), SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT/2 + 50, 20, WHITE);
                if (player1.health <= 0 && player2.health > 0) {
                    DrawText("You Lost!", SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 + 90, 30, RED);
                } else if (player1.health > 0 && player2.health <= 0) {
                    DrawText("You Won!", SCREEN_WIDTH/2 - 60, SCREEN_HEIGHT/2 + 90, 30, GREEN);
                } else {
                    DrawText("Draw!", SCREEN_WIDTH/2 - 40, SCREEN_HEIGHT/2 + 90, 30, YELLOW);
                }
                break;
        }
        
        EndDrawing();
        
        pthread_mutex_unlock(&mutex);
    }
    
    // Cleanup
    UnloadTexture(upArrow);
    UnloadTexture(downArrow);
    UnloadTexture(leftArrow);
    UnloadTexture(rightArrow);
    UnloadTexture(background);
    UnloadMusicStream(gameMusic);
    CloseAudioDevice();
    CloseWindow();
    
    close(sock);
    pthread_mutex_destroy(&mutex);
    
    return 0;
}
