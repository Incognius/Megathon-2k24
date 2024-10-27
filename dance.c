#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Constants
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define TARGET_ZONE_Y 600
#define ARROW_SPEED 300.0f
#define PERFECT_THRESHOLD 25.0f
#define GOOD_THRESHOLD 50.0f
#define MAX_ARROWS 100
#define INITIAL_SPAWN_INTERVAL 2.0f
#define DIFFICULTY_INCREASE_INTERVAL 15.0f
#define SPAWN_INTERVAL_DECREASE 0.5f
#define MIN_SPAWN_INTERVAL 0.5f
#define PERFECT_DAMAGE 2.5f
#define UP_DOWN_HITBOX 200.0f
#define TIMING_RANGE 50.0f

typedef enum { STATE_START_SCREEN, STATE_GAME, STATE_END_SCREEN } GameStateEnum;

// Arrow structure
typedef struct {
    Vector2 position;
    int direction; // 0: up, 1: down, 2: left, 3: right
    bool active;
} Arrow;

// Player structure
typedef struct {
    int score;
    float health;
    char combo[20];
    Color laneColor;
    Arrow arrows[MAX_ARROWS];
    int arrowCount;
    int perfectPresses;
} Player;

// Character structure
typedef struct {
    Vector2 position;
    float scale;
    Texture2D texture;
    int currentDirection;
} Character;

// Game state structure
typedef struct {
    float spawnTimer;
    float currentSpawnInterval;
    float difficultyTimer;
} GameState;

// Function to spawn a new arrow
void SpawnArrow(Player* player, bool isLeftSide) {
    if (player->arrowCount >= MAX_ARROWS) return;

    Arrow* arrow = &player->arrows[player->arrowCount];
    arrow->direction = GetRandomValue(0, 3);
    arrow->active = true;

    float xPos = isLeftSide ? SCREEN_WIDTH * 0.16f : SCREEN_WIDTH * 0.84f;

    switch (arrow->direction) {
        case 0: xPos -= 50; break;  // Up arrow
        case 1: xPos += 50; break;  // Down arrow
        case 2: xPos -= 150; break; // Left arrow
        case 3: xPos += 150; break; // Right arrow
    }

    arrow->position = (Vector2){ xPos, -50.0f };
    player->arrowCount++;
}

// Function to remove an arrow
void RemoveArrow(Player* player, int index) {
    if (index < 0 || index >= player->arrowCount) return; // Shift remaining arrows left
    for (int i = index; i < player->arrowCount - 1; i++) {
        player->arrows[i] = player->arrows[i + 1];
    }
    player->arrowCount--;
}

// Function to draw arrows
void DrawArrow(Vector2 pos, int direction, Color color, Texture2D upArrow, Texture2D downArrow, Texture2D leftArrow, Texture2D rightArrow) {
    float scale = 0.33f; // Arrow scale set to 0.33f

    switch (direction) {
        case 0: // Up
            DrawTextureEx(upArrow, (Vector2){ pos.x - (upArrow.width * scale) / 2, pos.y - (upArrow.height * scale) / 2 }, 0.0f, scale, color);
            break;
        case 1: // Down
            DrawTextureEx(downArrow, (Vector2){ pos.x - (downArrow.width * scale) / 2, pos.y - (downArrow.height * scale) / 2 }, 0.0f, scale, color);
            break;
        case 2: // Left
            DrawTextureEx(leftArrow, (Vector2){ pos.x - (leftArrow.width * scale) / 2, pos.y - (leftArrow.height * scale) / 2 }, 0.0f, scale, color);
            break;
        case 3: // Right
            DrawTextureEx(rightArrow, (Vector2){ pos.x - (rightArrow.width * scale) / 2, pos.y - (rightArrow.height * scale) / 2 }, 0.0f, scale, color);
            break;
    }
}

// Function to handle player input
void HandleInput(Player* attacker, Player * defender, Character* character, bool isLeftPlayer) {
    int pressedDir = -1;

    if (isLeftPlayer) {
        if (IsKeyPressed(KEY_S)) pressedDir = 0; // Up (swapped to 'S' for down)
        else if (IsKeyPressed(KEY_W)) pressedDir = 1; // Down (swapped to 'W' for up)
        else if (IsKeyPressed(KEY_A)) pressedDir = 2; // Left
        else if (IsKeyPressed(KEY_D)) pressedDir = 3; // Right
    } else {
        if (IsKeyPressed(KEY_DOWN)) pressedDir = 4; // Up (swapped to down arrow)
        else if (IsKeyPressed(KEY_UP)) pressedDir = 5; // Down (swapped to up arrow)
        else if (IsKeyPressed(KEY_LEFT)) pressedDir = 6; // Left
        else if (IsKeyPressed(KEY_RIGHT)) pressedDir = 7; // Right
    }

    if (pressedDir != -1) {
        // Update character texture based on direction
        character->currentDirection = pressedDir;

        switch (pressedDir) {
            case 0: character->texture = LoadTexture("downg.png"); break;
            case 1: character->texture = LoadTexture("upg.png"); break;
            case 2: character->texture = LoadTexture("leftg.png"); break;
            case 3: character->texture = LoadTexture("rightg.png"); break;
            case 4: character->texture = LoadTexture("downz.png"); break;
            case 5: character->texture = LoadTexture("upz.png"); break;
            case 6: character->texture = LoadTexture("leftz.png"); break;
            case 7: character->texture = LoadTexture("rightz.png"); break;
        }

        float closestDist = GOOD_THRESHOLD;
        int closestIdx = -1;

        for (int i = 0; i < attacker->arrowCount; i++) {
            int arrowDirection = attacker->arrows[i].direction + (isLeftPlayer ? 0 : 4); // Adjust arrow direction for right player

            // Check if the pressed direction matches the arrow direction
            if (arrowDirection == pressedDir) {
                float dist = fabsf(attacker->arrows[i].position.y - TARGET_ZONE_Y);

                if (dist < closestDist) {
                    closestDist = dist;
                    closestIdx = i;
                }
            }
        }

        // Apply scoring based on timing
        if (closestIdx != -1) {
            float dist = fabsf(attacker->arrows[closestIdx].position.y - TARGET_ZONE_Y);

            if (dist < PERFECT_THRESHOLD) {
                attacker->score += 100;
                strcpy(attacker->combo, "PERFECT!");
                defender->health -= PERFECT_DAMAGE;
                if (defender->health < 0) defender->health = 0;
                attacker->perfectPresses++;
            } else if ((pressedDir % 2 == 1 || pressedDir % 2 == 2) && dist < UP_DOWN_HITBOX) {
                attacker->score += 50;
                strcpy(attacker->combo, "GOOD!");
            } else if ((pressedDir % 2 == 3 || pressedDir % 2 == 4) && dist < TIMING_RANGE) {
                attacker->score += 50;
                strcpy(attacker->combo, "GOOD!");
            } else {
                strcpy(attacker->combo, "MISS!");
            }

            RemoveArrow(attacker, closestIdx);
        } else {
            strcpy(attacker->combo, "MISS!");
        }
    }
}

// Function to draw characters
void DrawCharacter(Character character) {
    DrawTextureEx(character.texture, (Vector2){ character.position.x - (character.texture.width * character.scale) / 2, character.position.y - (character.texture.height * character.scale) / 2 }, 0.0f, character.scale, WHITE);
}

void DrawStartScreen() {
    DrawText("Press SPACE to start", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2, 30, WHITE);
}

void DrawEndScreen(int leftPlayerHealth, int rightPlayerHealth) {
    ClearBackground(BLACK);

    const char* winnerText;
    if (leftPlayerHealth <= 0 && rightPlayerHealth <= 0) {
        winnerText = "It's a Draw!";
    } else if (leftPlayerHealth <= 0) {
        winnerText = "Right Player Wins!";
    } else {
        winnerText = "Left Player Wins!";
    }

    DrawText(winnerText, SCREEN_WIDTH / 2 - MeasureText(winnerText, 40) / 2, SCREEN_HEIGHT / 2 - 20, 40, WHITE);
    DrawText("Press SPACE to restart", SCREEN_WIDTH / 2 - MeasureText("Press SPACE to restart", 20) / 2, SCREEN_HEIGHT / 2 + 30,  20, WHITE);
}

int main(void) {
    // Initialize window
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Rhythm Game");
    SetTargetFPS(60);

    // Initialize audio device
    InitAudioDevice();

    // Load textures for arrows and background
    Texture2D upArrow = LoadTexture("darrow.png");
    Texture2D downArrow = LoadTexture("uarrow.png");
    Texture2D leftArrow = LoadTexture("larrow.png");
    Texture2D rightArrow = LoadTexture("rarrow.png");
    Texture2D background = LoadTexture("backd.png");  // Load background image

    Texture2D leftCharacterUp = LoadTexture("upg.png");
    Texture2D leftCharacterDown = LoadTexture ("downg.png");
    Texture2D leftCharacterLeft = LoadTexture("leftg.png");
    Texture2D leftCharacterRight = LoadTexture("rightg.png");

    Texture2D rightCharacterUp = LoadTexture("upz.png");
    Texture2D rightCharacterDown = LoadTexture("downz.png");
    Texture2D rightCharacterLeft = LoadTexture("leftz.png");
    Texture2D rightCharacterRight = LoadTexture("rightz.png");

    // Load character textures
    Texture2D leftCharacterTexture = LoadTexture("baseg.png");  // Load left character texture
    Texture2D rightCharacterTexture = LoadTexture("basez.png"); // Load right character texture

    // Load music
    Music music = LoadMusicStream("bloodymary.mp3"); // Load your music file
    SetMusicVolume(music, 0.5f); // Set volume (0.0f to 1.0f)

    // Check if background texture loaded successfully
    if (background.width == 0 || background.height == 0) {
        printf("Failed to load background texture!\n");
        CloseWindow();
        return -1;  // Exit if the background fails to load
    }

    // Calculate scaling factors to fit background to screen
    float scaleX = (float)SCREEN_WIDTH / background.width;
    float scaleY = (float)SCREEN_HEIGHT / background.height;
    float scale = scaleX > scaleY ? scaleX : scaleY;  // Choose the larger scale to cover the screen

    // Initialize players
    Player leftPlayer = {.score = 0, .health = 100.0f, .combo = "READY!", .laneColor = WHITE, .arrowCount = 0, .perfectPresses = 0};
    Player rightPlayer = {. score = 0, .health = 100.0f, .combo = "READY!", .laneColor = WHITE, .arrowCount = 0, .perfectPresses = 0};

    // Initialize characters
    Character leftCharacter = { 
        .position = (Vector2){ SCREEN_WIDTH * 0.25f, TARGET_ZONE_Y - 100 },  // Position above the perfection line
        .scale = 0.5f,  // Set scale for left character
        .texture = leftCharacterTexture
    };

    Character rightCharacter = { 
        .position = (Vector2){ SCREEN_WIDTH * 0.75f, TARGET_ZONE_Y - 100 },  // Position above the perfection line
        .scale = 0.5f,  // Set scale for right character
        .texture = rightCharacterTexture
    };

    // Initialize game state
    GameState gameState = {.spawnTimer = 0.0f, .currentSpawnInterval = INITIAL_SPAWN_INTERVAL, .difficultyTimer = 0.0f};

    GameStateEnum currentGameState = STATE_START_SCREEN; // Start in the start screen state

    // Main game loop
    while (!WindowShouldClose()) {
        // Update game state
        gameState.spawnTimer += GetFrameTime();
        gameState.difficultyTimer += GetFrameTime();

        // Check input
        if (IsKeyPressed(KEY_SPACE) && currentGameState == STATE_START_SCREEN) {
            currentGameState = STATE_GAME;
            PlayMusicStream(music); // Start playing music when game starts
            SetMusicVolume(music, 1.0f); // Volume range is 0.0 to 1.0
        }

        if (currentGameState == STATE_GAME) {
            HandleInput(&leftPlayer, &rightPlayer, &leftCharacter, true);
            HandleInput(&rightPlayer, &leftPlayer, &rightCharacter, false);

            // Spawn arrows based on the current spawn interval
            if (gameState.spawnTimer >= gameState.currentSpawnInterval) {
                SpawnArrow(&leftPlayer, true);   // Spawn arrow for left player
                SpawnArrow(&rightPlayer, false); // Spawn arrow for right player
                gameState.spawnTimer = 0.0f;
            }

            // Every 15 seconds, reduce the spawn interval by 0.5 seconds, if above the minimum
            if (gameState.difficultyTimer >= DIFFICULTY_INCREASE_INTERVAL) {
                if (gameState.currentSpawnInterval > MIN_SPAWN_INTERVAL) {
                    gameState.currentSpawnInterval -= SPAWN_INTERVAL_DECREASE;
                    if (gameState.currentSpawnInterval < MIN_SPAWN_INTERVAL) {
                        gameState.currentSpawnInterval = MIN_SPAWN_INTERVAL;
                    }
                }
                gameState.difficultyTimer = 0.0f; // Reset the difficulty timer
            }

            // Update arrows
            for (int i = 0; i < leftPlayer.arrowCount; i++) {
                leftPlayer.arrows[i].position.y += ARROW_SPEED * GetFrameTime();
                if (leftPlayer.arrows[i].position.y > SCREEN_HEIGHT) {
                    RemoveArrow(&leftPlayer, i);
                    i--;
                }
            }

            for (int i = 0; i < rightPlayer.arrowCount; i++) {
                rightPlayer.arrows[i].position.y += ARROW_SPEED * GetFrameTime();
                if (rightPlayer.arrows[i].position.y > SCREEN_HEIGHT) {
                    RemoveArrow(&rightPlayer, i);
                    i--;
                }
            }

            // Update music stream
            UpdateMusicStream(music);

            // Check for game over condition
            if (leftPlayer.health <= 0 || rightPlayer.health <= 0) {
                currentGameState = STATE_END_SCREEN;
            }
        }

        if (currentGameState == STATE_END_SCREEN) {
            if (IsKeyPressed(KEY_SPACE)) {
                // Reset player health and game state
                leftPlayer.health = 100.0f;
                rightPlayer.health = 100.0f;
                currentGameState = STATE_START_SCREEN;
            }
        }

        // Draw
        BeginDrawing();
        ClearBackground(BLACK);

        if (currentGameState == STATE_START_SCREEN) {
            DrawStartScreen();
        } else if (currentGameState == STATE_END_SCREEN) {
            DrawEndScreen(leftPlayer.health, rightPlayer.health);
        } else {
            // Draw the background image scaled to the screen
            DrawTextureEx(background, (Vector2){0, 0}, 0.0f, scale, WHITE);

            // Draw left and right characters
            DrawCharacter(leftCharacter);
            DrawCharacter(rightCharacter);

            // Draw score, health, and combo for each player
            DrawText("Score: ", 50, 50, 20, BLACK);
            DrawText(TextFormat("%d", leftPlayer.score), 150, 50, 20, BLACK);

            DrawRectangle(20, 80,  200, 20, RED);
            DrawRectangle(20, 80, 200 * (rightPlayer.health / 100.0f), 20, GREEN);

            DrawRectangle(SCREEN_WIDTH - 220, 80, 200, 20, RED);
            DrawRectangle(SCREEN_WIDTH - 220, 80, 200 * (leftPlayer.health / 100.0f), 20, GREEN);

            DrawText("Combo: ", 50, 110, 20, BLACK);
            DrawText(leftPlayer.combo, 150, 110, 20, BLACK);

            DrawText("Score: ", SCREEN_WIDTH - 200, 50, 20, BLACK);
            DrawText(TextFormat("%d", rightPlayer.score), SCREEN_WIDTH - 100, 50, 20, BLACK);

            DrawRectangle(20, 80, 200, 20, RED);
            DrawRectangle(20, 80, 200 * (leftPlayer.health / 100.0f ), 20, GREEN);

            DrawRectangle(SCREEN_WIDTH - 220, 80, 200, 20, RED);
            DrawRectangle(SCREEN_WIDTH - 220, 80, 200 * (rightPlayer.health / 100.0f), 20, GREEN);

            DrawText("Combo: ", SCREEN_WIDTH - 200, 110, 20, BLACK);
            DrawText(rightPlayer.combo, SCREEN_WIDTH - 100, 110, 20, BLACK);

            // Draw arrows for each player
            for (int i = 0; i < leftPlayer.arrowCount; i++) {
                DrawArrow(leftPlayer.arrows[i].position, leftPlayer.arrows[i].direction, leftPlayer.laneColor, upArrow, downArrow, leftArrow, rightArrow);
            }

            for (int i = 0; i < rightPlayer.arrowCount; i++) {
                DrawArrow(rightPlayer.arrows[i].position, rightPlayer.arrows[i].direction, rightPlayer.laneColor, upArrow, downArrow, leftArrow, rightArrow);
            }

            // Draw target zone line
            DrawLine(0, TARGET_ZONE_Y, SCREEN_WIDTH, TARGET_ZONE_Y, RED);
        }

        EndDrawing();
    }

    // Unload resources
    UnloadTexture(upArrow);
    UnloadTexture(downArrow);
    UnloadTexture(leftArrow);
    UnloadTexture(rightArrow);
    UnloadTexture(background);
    UnloadTexture(leftCharacterTexture);
    UnloadTexture(rightCharacterTexture);
    StopMusicStream(music); // Stop music before unloading
    UnloadMusicStream(music); // Unload music from memory

    CloseAudioDevice(); // Close the audio device
    CloseWindow();

    return 0;
}