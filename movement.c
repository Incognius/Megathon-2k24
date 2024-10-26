#include <raylib.h>

int main(void) {
    // Initialization
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Raylib Double Jump Example with Ceiling");

    // Circle properties
    Vector2 circlePosition = { screenWidth / 2.0f, screenHeight - 50.0f };
    float circleRadius = 50.0f;
    
    // Jump properties
    float jumpVelocity = 0.0f;
    float jumpAcceleration = -0.4f;  // Gravity effect
    float maxJumpVelocity = -8.0f;   // Initial jump speed
    bool isJumping = false;
    int jumpCount = 0;                // Track number of jumps
    const int maxJumps = 2;           // Maximum jumps (double jump)
    float groundY = screenHeight - 50 - circleRadius;

    // Ceiling limit
    float ceilingY = 100.0f; // Limit for the upper end of the window

    // Momentum variables
    float horizontalMomentum = 0.0f;
    float momentumDecay = 0.1f; // How quickly momentum decays

    SetTargetFPS(60); // Set target frames per second

    // Main game loop
    while (!WindowShouldClose()) {
        // Update horizontal movement
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            horizontalMomentum += 0.5f; // Increase momentum to the right
        }
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            horizontalMomentum -= 0.5f; // Increase momentum to the left
        }

        // Apply momentum and decay
        circlePosition.x += horizontalMomentum;
        horizontalMomentum *= (1.0f - momentumDecay); // Decay momentum over time

        // Jump logic
        if (IsKeyPressed(KEY_SPACE)) {
            if (!isJumping) {
                // First jump
                isJumping = true;
                jumpCount = 1; // Reset jump count for first jump
                jumpVelocity = maxJumpVelocity; // Set initial jump velocity
            } else if (jumpCount < maxJumps) {
                // Double jump
                jumpCount++;
                jumpVelocity = maxJumpVelocity; // Set jump velocity for double jump
            }
        }

        if (isJumping) {
            // Apply gravity
            circlePosition.y += jumpVelocity;
            jumpVelocity -= jumpAcceleration; // Decrease jump velocity over time

            // Check if the circle has landed
            if (circlePosition.y >= groundY) {
                circlePosition.y = groundY; // Reset to ground level
                isJumping = false; // End jump
                jumpVelocity = 0.0f; // Reset jump velocity
                jumpCount = 0; // Reset jump count on landing
            }

            // Prevent the circle from going above the ceiling
            if (circlePosition.y < ceilingY) {
                circlePosition.y = ceilingY; // Set to ceiling limit
                jumpVelocity = 0.0f; // Stop upward movement
            }
        }

        // Keep circle within window bounds
        if (circlePosition.x - circleRadius < 0) {
            circlePosition.x = circleRadius;
            horizontalMomentum = 0.0f; // Stop momentum if hitting left wall
        }
        if (circlePosition.x + circleRadius > screenWidth) {
            circlePosition.x = screenWidth - circleRadius;
            horizontalMomentum = 0.0f; // Stop momentum if hitting right wall
        }

        // Draw
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        // Draw ground line
        DrawLine(0, groundY + circleRadius, screenWidth, groundY + circleRadius, DARKGRAY);
        
        // Draw ceiling limit
        DrawLine(0, ceilingY, screenWidth, ceilingY, DARKGRAY);
        
        // Draw the circle
        DrawCircleV(circlePosition, circleRadius, BLUE);
        EndDrawing();
    }

    // De-Initialization
    CloseWindow(); // Close window and OpenGL context

 return 0;
}