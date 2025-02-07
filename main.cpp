/*******************************************************************************************
 *
 *   Raylib - 2D Platformer with Editor Mode
 *
 *   - On startup, we're in "editor mode".
 *   - In editor mode, you can click & drag enemies and their patrol bounds to
 *     reposition them on the screen.
 *   - Left click to add tiles, Right click to remove tiles.
 *   - Press ENTER to switch to game mode. Then enemies run their normal patrol/flying logic.
 *   - WASD movement, space to jump, mouse aiming.
 *
 *   Compile (Windows + MinGW, for example):
 *   gcc -o platformer_with_editor platformer_with_editor.c -I C:\raylib\include -L C:\raylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm
 *
 ********************************************************************************************/

#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 450

#define LEVEL_WIDTH 1600
#define LEVEL_HEIGHT 900

#define LEVEL_FILE "level.txt"
#define CHECKPOINT_FILE "checkpoint.txt"

#define TILE_SIZE 50
#define MAP_COLS (LEVEL_WIDTH / TILE_SIZE)  // 16 columns of 50 = 800
#define MAP_ROWS (LEVEL_HEIGHT / TILE_SIZE) // 9 rows of 50 = 450
#define MAX_PLAYER_BULLETS 20
#define MAX_ENEMY_BULLETS 20

struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool fromPlayer; // true if shot by player, false if shot by enemy
};

struct Enemy
{
    Vector2 position;
    Vector2 velocity;
    float radius;
    bool alive;
    float speed;
    float leftBound;
    float rightBound;
    int direction; // 1=right, -1=left
    float shootTimer;
    float shootCooldown;

    // For flying enemy only (sine wave)
    float baseY;
    float waveOffset;
    float waveAmplitude;
    float waveSpeed;
};

int mapTiles[MAP_ROWS][MAP_COLS] = {0}; // 0 = empty, 1 = solid
int draggedBoundEnemy = -1;             // 0 = ground enemy, 1 = flying enemy
int boundType = -1;                     // 0 = left bound, 1 = right bound
bool draggingBound = false;

static void DrawTilemap(Camera2D camera)
{
    // Calculate the visible area in world coordinates:
    // The visible width and height in world space are the screen dimensions divided by the zoom.
    float camWorldWidth = LEVEL_WIDTH / camera.zoom;
    float camWorldHeight = LEVEL_HEIGHT / camera.zoom;

    // The camera.target is the point that the camera is centered on.
    float camLeft = camera.target.x - camWorldWidth / 2;
    float camRight = camera.target.x + camWorldWidth / 2;
    float camTop = camera.target.y - camWorldHeight / 2;
    float camBottom = camera.target.y + camWorldHeight / 2;

    // Convert the world coordinates to tile indices:
    int minTileX = (int)(camLeft / TILE_SIZE);
    int maxTileX = (int)(camRight / TILE_SIZE);
    int minTileY = (int)(camTop / TILE_SIZE);
    int maxTileY = (int)(camBottom / TILE_SIZE);

    // Clamp indices to map dimensions:
    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= MAP_COLS)
        maxTileX = MAP_COLS - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= MAP_ROWS)
        maxTileY = MAP_ROWS - 1;

    // Iterate over the visible tiles only:
    for (int y = minTileY; y <= maxTileY; y++)
    {
        for (int x = minTileX; x <= maxTileX; x++)
        {
            if (mapTiles[y][x] == 1)
            {
                // Draw a filled tile
                DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, DARKGRAY);
            }
            else
            {
                // Draw grid lines for clarity
                DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, LIGHTGRAY);
            }
        }
    }
}

static void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, float radius)
{
    // We'll figure out which tiles in the map might overlap the circle by a bounding box check
    // This helps us skip checking all tiles.
    // bounding box of the circle:
    float left = pos->x - radius;
    float right = pos->x + radius;
    float top = pos->y - radius;
    float bottom = pos->y + radius;

    // The tile indices that might overlap
    int minTileX = (int)(left / TILE_SIZE);
    int maxTileX = (int)(right / TILE_SIZE);
    int minTileY = (int)(top / TILE_SIZE);
    int maxTileY = (int)(bottom / TILE_SIZE);

    // Clamp to map range
    if (minTileX < 0)
    {
        minTileX = 0;
    }
    if (maxTileX >= MAP_COLS)
    {
        maxTileX = MAP_COLS - 1;
    }
    if (minTileY < 0)
    {
        minTileY = 0;
    }
    if (maxTileY >= MAP_ROWS)
    {
        maxTileY = MAP_ROWS - 1;
    }

    // Check collisions with each tile in this bounding range
    for (int ty = minTileY; ty <= maxTileY; ty++)
    {
        for (int tx = minTileX; tx <= maxTileX; tx++)
        {
            if (mapTiles[ty][tx] == 1)
            {
                // tile rect
                Rectangle tileRect = {
                    tx * TILE_SIZE,
                    ty * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE};

                if (CheckCollisionCircleRec(*pos, radius, tileRect))
                {
                    // compute the circle center relative to the tile rect
                    float cx = pos->x - tileRect.x;
                    float cy = pos->y - tileRect.y;

                    // measure overlap on each axis.
                    float overlapLeft = (tileRect.x + tileRect.width) - (pos->x - radius);
                    float overlapRight = (pos->x + radius) - tileRect.x;
                    float overlapTop = (tileRect.y + tileRect.height) - (pos->y - radius);
                    float overlapBottom = (pos->y + radius) - tileRect.y;

                    // find the minimal overlap
                    float minOverlap = overlapLeft;
                    // push on x by default
                    char axis = 'x';
                    // 1 = push right, -1 = push left
                    int sign = 1;

                    if (overlapRight < minOverlap)
                    {
                        minOverlap = overlapRight;
                        axis = 'x';
                        sign = -1; // push left
                    }
                    if (overlapTop < minOverlap)
                    {
                        minOverlap = overlapTop;
                        axis = 'y';
                        sign = 1; // push down
                    }
                    if (overlapBottom < minOverlap)
                    {
                        minOverlap = overlapBottom;
                        axis = 'y';
                        sign = -1; // push up
                    }

                    // Now push out
                    if (axis == 'x')
                    {
                        pos->x += sign * minOverlap;
                        // zero out x velocity
                        vel->x = 0;
                    }
                    else // axis == 'y'
                    {
                        pos->y += sign * minOverlap;
                        // zero out y velocity
                        vel->y = 0;
                    }
                }
            }
        }
    }
}

bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Vector2 playerStart,
               Enemy groundEnemy, Enemy flyingEnemy,
               Vector2 checkpointPos)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    // Write map dimensions
    fprintf(file, "%d %d\n", MAP_ROWS, MAP_COLS);

    // Write tilemap data
    for (int y = 0; y < MAP_ROWS; y++)
    {
        for (int x = 0; x < MAP_COLS; x++)
        {
            fprintf(file, "%d ", mapTiles[y][x]);
        }
        fprintf(file, "\n");
    }

    // Write player start position
    fprintf(file, "PLAYER_START %.2f %.2f\n", playerStart.x, playerStart.y);

    // Write ground enemy data (position and patrol bounds)
    fprintf(file, "GROUND_ENEMY %.2f %.2f %.2f %.2f\n",
            groundEnemy.position.x, groundEnemy.position.y,
            groundEnemy.leftBound, groundEnemy.rightBound);

    // Write flying enemy data (position, patrol bounds, and sine-wave parameters)
    fprintf(file, "FLYING_ENEMY %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",
            flyingEnemy.position.x, flyingEnemy.position.y,
            flyingEnemy.leftBound, flyingEnemy.rightBound,
            flyingEnemy.baseY, flyingEnemy.waveAmplitude, flyingEnemy.waveSpeed);

    // Write checkpoint position
    fprintf(file, "CHECKPOINT %.2f %.2f\n", checkpointPos.x, checkpointPos.y);

    fclose(file);
    return true;
}

bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Vector2 *playerStart,
               Enemy *groundEnemy, Enemy *flyingEnemy,
               Vector2 *checkpointPos)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        return false;

    int rows, cols;
    if (fscanf(file, "%d %d", &rows, &cols) != 2)
    {
        fclose(file);
        return false;
    }
    // (Optionally check that rows==MAP_ROWS and cols==MAP_COLS)

    for (int y = 0; y < MAP_ROWS; y++)
    {
        for (int x = 0; x < MAP_COLS; x++)
        {
            if (fscanf(file, "%d", &mapTiles[y][x]) != 1)
            {
                fclose(file);
                return false;
            }
        }
    }

    char token[32];
    // Load player start position
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "PLAYER_START") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f", &playerStart->x, &playerStart->y) != 2)
    {
        fclose(file);
        return false;
    }

    // Load ground enemy data
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "GROUND_ENEMY") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %f %f", &groundEnemy->position.x, &groundEnemy->position.y,
               &groundEnemy->leftBound, &groundEnemy->rightBound) != 4)
    {
        fclose(file);
        return false;
    }

    // Load flying enemy data
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "FLYING_ENEMY") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %f %f %f %f %f", &flyingEnemy->position.x, &flyingEnemy->position.y,
               &flyingEnemy->leftBound, &flyingEnemy->rightBound,
               &flyingEnemy->baseY, &flyingEnemy->waveAmplitude, &flyingEnemy->waveSpeed) != 7)
    {
        fclose(file);
        return false;
    }

    // Load checkpoint position
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "CHECKPOINT") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f", &checkpointPos->x, &checkpointPos->y) != 2)
    {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

//---------------------
// Checkpoint File Save/Load (checkpoint.txt)
//---------------------
bool SaveCheckpointState(const char *filename,
                         Vector2 playerPos, int playerHealth,
                         Enemy groundEnemy, Enemy flyingEnemy)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    // Save player state: position and health
    fprintf(file, "PLAYER %.2f %.2f %d\n", playerPos.x, playerPos.y, playerHealth);

    // Save ground enemy state: alive flag and position
    fprintf(file, "GROUND_ENEMY %d %.2f %.2f\n", groundEnemy.alive ? 1 : 0,
            groundEnemy.position.x, groundEnemy.position.y);

    // Save flying enemy state: alive flag and position
    fprintf(file, "FLYING_ENEMY %d %.2f %.2f\n", flyingEnemy.alive ? 1 : 0,
            flyingEnemy.position.x, flyingEnemy.position.y);

    fclose(file);
    return true;
}

bool LoadCheckpointState(const char *filename,
                         Vector2 *playerPos, int *playerHealth,
                         Enemy *groundEnemy, Enemy *flyingEnemy)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        return false;

    char token[32];
    // Load player state
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "PLAYER") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %d", &playerPos->x, &playerPos->y, playerHealth) != 3)
    {
        fclose(file);
        return false;
    }

    // Load ground enemy state
    int aliveFlag;
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "GROUND_ENEMY") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%d %f %f", &aliveFlag, &groundEnemy->position.x, &groundEnemy->position.y) != 3)
    {
        fclose(file);
        return false;
    }
    groundEnemy->alive = (aliveFlag == 1);

    // Load flying enemy state
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "FLYING_ENEMY") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%d %f %f", &aliveFlag, &flyingEnemy->position.x, &flyingEnemy->position.y) != 3)
    {
        fclose(file);
        return false;
    }
    flyingEnemy->alive = (aliveFlag == 1);

    fclose(file);
    return true;
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer Test");
    SetTargetFPS(60);

    Camera2D camera = {0};
    camera.target = (Vector2){LEVEL_WIDTH / 2.0f, LEVEL_HEIGHT / 2.0f};
    camera.offset = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Player
    Vector2 playerPos = {200.0f, 50.0f};
    Vector2 playerVel = {0, 0};
    float playerRadius = 20.0f;
    int playerHealth = 5;

    // Ground Enemy
    Enemy groundEnemy = {0};
    groundEnemy.position = (Vector2){400.0f, 50.0f};
    groundEnemy.velocity = (Vector2){0, 0};
    groundEnemy.radius = 20.0f;
    groundEnemy.alive = true;
    groundEnemy.speed = 2.0f;
    groundEnemy.leftBound = 300.0f;
    groundEnemy.rightBound = 500.0f;
    groundEnemy.direction = 1;
    groundEnemy.shootTimer = 0.0f;
    groundEnemy.shootCooldown = 60.0f;
    groundEnemy.baseY = groundEnemy.position.y; // Not used for ground enemy:
    groundEnemy.waveOffset = 0;                 // Not used for ground enemy:
    groundEnemy.waveAmplitude = 0;              // Not used for ground enemy:
    groundEnemy.waveSpeed = 0;                  // Not used for ground enemy:

    // Flying Enemy
    Enemy flyingEnemy = {0};
    flyingEnemy.position = (Vector2){600.0f, 100.0f};
    flyingEnemy.velocity = (Vector2){0, 0};
    flyingEnemy.radius = 20.0f;
    flyingEnemy.alive = true;
    flyingEnemy.speed = 1.5f;
    flyingEnemy.leftBound = 500.0f;
    flyingEnemy.rightBound = 700.0f;
    flyingEnemy.direction = 1;
    flyingEnemy.shootTimer = 0.0f;
    flyingEnemy.shootCooldown = 90.0f;
    flyingEnemy.baseY = flyingEnemy.position.y;
    flyingEnemy.waveOffset = 0.0f;
    flyingEnemy.waveAmplitude = 40.0f;
    flyingEnemy.waveSpeed = 0.04f;

    float enemyShootRange = 300.0f;

    // Bullets
    Bullet playerBullets[MAX_PLAYER_BULLETS] = {0};
    Bullet enemyBullets[MAX_ENEMY_BULLETS] = {0};
    const float bulletSpeed = 10.0f;
    const float bulletRadius = 5.0f;

    // Checkpoint variables
    bool checkpointExists = false;
    bool draggingCheckpoint = false;
    Vector2 checkpointPos = {0, 0};
    Vector2 checkpointDragOffset = {0, 0};
    Rectangle checkpointRect = {0, 0, TILE_SIZE, TILE_SIZE * 2};

    // Editor / Game mode state
    bool editorMode = true;
    bool draggingEnemy = false;
    int draggedEnemy = -1; // 0 = ground, 1 = flying
    Vector2 dragOffset = {0};

    if (LoadLevel(LEVEL_FILE, mapTiles, &playerPos, &groundEnemy, &flyingEnemy, &checkpointPos)) {
        checkpointExists = (checkpointPos.x != 0.0f || checkpointPos.y != 0.0f);
    } else {
        // Initialize default tilemap (for example, fill the bottom row with ground)
        for (int x = 0; x < MAP_COLS; x++)
            mapTiles[MAP_ROWS - 1][x] = 1;
    }

    while (!WindowShouldClose())
    {
        bool playerDead = (playerHealth <= 0);
        Vector2 mousePos = GetMousePosition();
        Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

        // EDITOR MODE:
        if (editorMode)
        {
            // Press ENTER to exit editor mode and start the actual game
            if (IsKeyPressed(KEY_ENTER))
            {
                editorMode = false;
            }

            if (IsKeyPressed(KEY_S))
            {
                if (SaveLevel(LEVEL_FILE, mapTiles, playerPos, groundEnemy, flyingEnemy, checkpointPos))
                    TraceLog(LOG_INFO, "Level saved successfully!");
                else
                    TraceLog(LOG_ERROR, "Failed to save Level!");
            }

            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))
            {
                Vector2 delta = GetMouseDelta();
                camera.target.x -= delta.x / camera.zoom;
                camera.target.y -= delta.y / camera.zoom;
            }

            // --- Update Camera Zoom ---
            float wheelMove = GetMouseWheelMove();
            if (wheelMove != 0)
            {
                camera.zoom += wheelMove * 0.05f;
                if (camera.zoom < 0.1f)
                    camera.zoom = 0.1f;
                if (camera.zoom > 3.0f)
                    camera.zoom = 3.0f;
            }

            // --- Bound Editing ---
            // Check if the mouse is near a bound marker (within 10 pixels)
            if (!draggingEnemy && !draggingBound && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                // Check ground enemy's bounds
                if (fabsf(screenPos.x - groundEnemy.leftBound) < 10)
                {
                    draggingBound = true;
                    draggedBoundEnemy = 0;
                    boundType = 0;
                }
                else if (fabsf(screenPos.x - groundEnemy.rightBound) < 10)
                {
                    draggingBound = true;
                    draggedBoundEnemy = 0;
                    boundType = 1;
                }
                // Check flying enemy's bounds
                else if (fabsf(screenPos.x - flyingEnemy.leftBound) < 10)
                {
                    draggingBound = true;
                    draggedBoundEnemy = 1;
                    boundType = 0;
                }
                else if (fabsf(screenPos.x - flyingEnemy.rightBound) < 10)
                {
                    draggingBound = true;
                    draggedBoundEnemy = 1;
                    boundType = 1;
                }
            }
            if (draggingBound)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    // Update the appropriate bound to the current mouse x-position
                    if (draggedBoundEnemy == 0)
                    {
                        if (boundType == 0)
                            groundEnemy.leftBound = screenPos.x;
                        else if (boundType == 1)
                            groundEnemy.rightBound = screenPos.x;
                    }
                    else if (draggedBoundEnemy == 1)
                    {
                        if (boundType == 0)
                            flyingEnemy.leftBound = screenPos.x;
                        else if (boundType == 1)
                            flyingEnemy.rightBound = screenPos.x;
                    }
                }
                else
                {
                    draggingBound = false;
                    draggedBoundEnemy = -1;
                    boundType = -1;
                }
            }

            // --- Enemy Position Dragging ---
            // Only allow enemy dragging if not currently dragging a bound.
            if (!draggingEnemy && !draggingBound && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                float dxG = screenPos.x - groundEnemy.position.x;
                float dyG = screenPos.y - groundEnemy.position.y;
                if (dxG * dxG + dyG * dyG <= (groundEnemy.radius * groundEnemy.radius))
                {
                    draggingEnemy = true;
                    draggedEnemy = 0;
                    dragOffset = (Vector2){dxG, dyG};
                }
                else
                {
                    float dxF = screenPos.x - flyingEnemy.position.x;
                    float dyF = screenPos.y - flyingEnemy.position.y;
                    if (dxF * dxF + dyF * dyF <= (flyingEnemy.radius * flyingEnemy.radius))
                    {
                        draggingEnemy = true;
                        draggedEnemy = 1;
                        dragOffset = (Vector2){dxF, dyF};
                    }
                }
            }
            if (draggingEnemy)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    if (draggedEnemy == 0)
                    {
                        groundEnemy.position.x = screenPos.x - dragOffset.x;
                        groundEnemy.position.y = screenPos.y - dragOffset.y;
                    }
                    else
                    {
                        flyingEnemy.position.x = screenPos.x - dragOffset.x;
                        flyingEnemy.position.y = screenPos.y - dragOffset.y;
                        // update baseY for sine wave
                        flyingEnemy.baseY = flyingEnemy.position.y;
                    }
                }
                else
                {
                    draggingEnemy = false;
                    draggedEnemy = -1;
                }
            }

            // -- Checkpoint Dragging --
            if (checkpointExists)
            {
                // Define checkpoint rectangle (in world space):
                Rectangle cpRect = {checkpointPos.x, checkpointPos.y, TILE_SIZE, TILE_SIZE * 2};
                if (!draggingEnemy && !draggingBound && !draggingCheckpoint &&
                    CheckCollisionPointRec(screenPos, cpRect) &&
                    IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    draggingCheckpoint = true;
                    checkpointDragOffset.x = screenPos.x - checkpointPos.x;
                    checkpointDragOffset.y = screenPos.y - checkpointPos.y;
                }
                if (draggingCheckpoint)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                    {
                        checkpointPos.x = screenPos.x - checkpointDragOffset.x;
                        checkpointPos.y = screenPos.y - checkpointDragOffset.y;
                    }
                    else
                    {
                        draggingCheckpoint = false;
                    }
                }
            }

            // --- Place/remove tiles ---
            bool enemyEditing = draggingEnemy || draggingBound;
            // Only allow tile creation if not doing anything enemy related
            if (!enemyEditing && (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_RIGHT_BUTTON)))
            {
                int tileX = (int)(screenPos.x / TILE_SIZE);
                int tileY = (int)(screenPos.y / TILE_SIZE);
                if (tileX >= 0 && tileX < MAP_COLS && tileY >= 0 && tileY < MAP_ROWS)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                    {
                        // place
                        mapTiles[tileY][tileX] = 1;
                    }
                    else if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
                    {
                        // remove
                        mapTiles[tileY][tileX] = 0;
                    }
                }
            }

            // Draw Editor View
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("EDITOR MODE: LMB=Place tile/drag enemy, RMB=Remove tile, ENTER=Play", 10, 10, 20, DARKGRAY);

            // Define a button in screen space:
            Rectangle checkpointButton = {10, SCREEN_HEIGHT - 40, 150, 30};
            DrawRectangleRec(checkpointButton, WHITE);
            DrawText("Create Checkpoint", checkpointButton.x + 5, checkpointButton.y + 5, 10, BLACK);
            // Check if button is clicked (using screen coordinates)
            Vector2 screenMousePos = GetMousePosition();
            if (CheckCollisionPointRec(screenMousePos, checkpointButton))
            {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !checkpointExists)
                {
                    // Compute the world center of the camera:
                    Vector2 worldCenter = camera.target;
                    // Compute tile indices near the center:
                    int tileX = (int)(worldCenter.x / TILE_SIZE);
                    int tileY = (int)(worldCenter.y / TILE_SIZE);
                    // Set checkpoint position to the top-left corner of that tile:
                    checkpointPos.x = tileX * TILE_SIZE;
                    checkpointPos.y = tileY * TILE_SIZE;
                    checkpointExists = true;
                }
            }

            // Draw world (tilemap, enemies, player, etc.) using the camera
            BeginMode2D(camera);

            // Draw the tilemap
            DrawTilemap(camera);

            if (checkpointExists)
            {
                float halfSide = groundEnemy.radius;
                float side = halfSide * 2;
                DrawRectangle(checkpointPos.x, checkpointPos.y, TILE_SIZE, TILE_SIZE * 2, Fade(GREEN, 0.3f));
            }

            // Ground enemy as a square
            if (groundEnemy.alive)
            {
                float halfSide = groundEnemy.radius;
                float side = halfSide * 2;
                DrawRectangle((int)groundEnemy.position.x - halfSide, (int)groundEnemy.position.y - halfSide, (int)side, (int)side, RED);
            }
            // Flying enemy as a diamond
            if (flyingEnemy.alive)
            {
                DrawPoly(flyingEnemy.position, 4, flyingEnemy.radius, 45.0f, ORANGE);
            }

            // Draw bound markers for enemies (vertical red lines)
            DrawLine((int)groundEnemy.leftBound, 0, (int)groundEnemy.leftBound, groundEnemy.radius, BLUE);
            DrawLine((int)groundEnemy.rightBound, 0, (int)groundEnemy.rightBound, groundEnemy.radius, BLUE);
            DrawLine((int)flyingEnemy.leftBound, 0, (int)flyingEnemy.leftBound, flyingEnemy.radius, BLUE);
            DrawLine((int)flyingEnemy.rightBound, 0, (int)flyingEnemy.rightBound, flyingEnemy.radius, BLUE);

            // If dragging enemy or bound, highlight
            if (draggingEnemy)
            {
                if (draggedEnemy == 0)
                {
                    DrawCircleLines((int)groundEnemy.position.x, (int)groundEnemy.position.y, groundEnemy.radius + 3, YELLOW);
                }
                else
                {
                    DrawCircleLines((int)flyingEnemy.position.x, (int)flyingEnemy.position.y, flyingEnemy.radius + 3, YELLOW);
                }
            }
            if (draggingBound)
            {
                DrawText("Editing Bound", 10, SCREEN_HEIGHT - 30, 20, RED);
            }

            EndMode2D();
            EndDrawing();
            continue;
        }

        // GAME MODE: The normal platformer logic begins here
        if (!playerDead)
        {
            camera.target = playerPos;
            camera.rotation = 0.0f;
            camera.zoom = .66f;

            // Player input
            playerVel.x = 0;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
            {
                playerVel.x = -4.0f;
            }
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
            {
                playerVel.x = 4.0f;
            }

            // Jump
            bool onGround = false;
            if (IsKeyPressed(KEY_SPACE))
            {
                // hack: only jump if velocity.y == 0
                if (fabsf(playerVel.y) < 0.001f)
                {
                    playerVel.y = -8.0f;
                }
            }

            // Gravity
            playerVel.y += 0.4f;

            // Move
            playerPos.x += playerVel.x;
            playerPos.y += playerVel.y;

            // Collision with tilemap
            ResolveCircleTileCollisions(&playerPos, &playerVel, playerRadius);

            // If the velocity.y == 0 after collisions, we consider the player on ground
            if (fabsf(playerVel.y) < 0.001f)
            {
                onGround = true;
            }

            if (CheckCollisionPointRec(playerPos, {checkpointPos.x, checkpointPos.y, TILE_SIZE, TILE_SIZE * 2}))
            {
                // Save the current state as a checkpoint.
                if (SaveCheckpointState("checkpoint.txt", playerPos, playerHealth, groundEnemy, flyingEnemy))
                {
                    TraceLog(LOG_INFO, "Checkpoint saved!");
                }
            }

            // Shooting
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                {
                    if (!playerBullets[i].active)
                    {
                        playerBullets[i].active = true;
                        playerBullets[i].fromPlayer = true;
                        playerBullets[i].position = playerPos;

                        Vector2 dir = {screenPos.x - playerPos.x, screenPos.y - playerPos.y};
                        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                        if (len > 0.0f)
                        {
                            dir.x /= len;
                            dir.y /= len;
                        }
                        playerBullets[i].velocity.x = dir.x * bulletSpeed;
                        playerBullets[i].velocity.y = dir.y * bulletSpeed;
                        break;
                    }
                }
            }
        }

        // Update player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        {
            if (playerBullets[i].active)
            {
                playerBullets[i].position.x += playerBullets[i].velocity.x;
                playerBullets[i].position.y += playerBullets[i].velocity.y;

                // Off-screen?
                if (playerBullets[i].position.x < 0 || playerBullets[i].position.x > LEVEL_WIDTH ||
                    playerBullets[i].position.y < 0 || playerBullets[i].position.y > LEVEL_HEIGHT)
                {
                    playerBullets[i].active = false;
                }
            }
        }

        // --- Ground Enemy update ---
        if (groundEnemy.alive)
        {
            groundEnemy.velocity.x = groundEnemy.direction * groundEnemy.speed;
            groundEnemy.velocity.y += 0.4f;

            groundEnemy.position.x += groundEnemy.velocity.x;
            groundEnemy.position.y += groundEnemy.velocity.y;
            ResolveCircleTileCollisions(&groundEnemy.position, &groundEnemy.velocity, groundEnemy.radius);

            // Patrol: enforce bounds if set (if bounds are nonzero)
            if (groundEnemy.leftBound != 0 || groundEnemy.rightBound != 0)
            {
                if (groundEnemy.position.x < groundEnemy.leftBound)
                {
                    groundEnemy.position.x = groundEnemy.leftBound;
                    groundEnemy.direction = 1;
                }
                else if (groundEnemy.position.x > groundEnemy.rightBound)
                {
                    groundEnemy.position.x = groundEnemy.rightBound;
                    groundEnemy.direction = -1;
                }
            }

            // Shooting
            groundEnemy.shootTimer += 1.0f;
            float dx = playerPos.x - groundEnemy.position.x;
            float dy = playerPos.y - groundEnemy.position.y;
            float distSqr = dx * dx + dy * dy;

            if (!playerDead && distSqr < (enemyShootRange * enemyShootRange))
            {
                if (groundEnemy.shootTimer >= groundEnemy.shootCooldown)
                {
                    for (int b = 0; b < MAX_ENEMY_BULLETS; b++)
                    {
                        if (!enemyBullets[b].active)
                        {
                            enemyBullets[b].active = true;
                            enemyBullets[b].fromPlayer = false;
                            enemyBullets[b].position = groundEnemy.position;

                            float len = sqrtf(dx * dx + dy * dy);
                            Vector2 dir = {0};
                            if (len > 0.0f)
                            {
                                dir.x = dx / len;
                                dir.y = dy / len;
                            }
                            enemyBullets[b].velocity.x = dir.x * bulletSpeed;
                            enemyBullets[b].velocity.y = dir.y * bulletSpeed;
                            break;
                        }
                    }

                    groundEnemy.shootTimer = 0.0f;
                }
            }

            // Collisions with player bullets
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (playerBullets[i].active)
                {
                    float bx = playerBullets[i].position.x - groundEnemy.position.x;
                    float by = playerBullets[i].position.y - groundEnemy.position.y;
                    float dist2 = bx * bx + by * by;
                    float combined = bulletRadius + groundEnemy.radius;
                    if (dist2 <= (combined * combined))
                    {
                        groundEnemy.alive = false;
                        playerBullets[i].active = false;
                        break;
                    }
                }
            }
        }

        // --- Flying Enemy update ---
        if (flyingEnemy.alive)
        {
            // Horizontal patrol using bounds
            if (flyingEnemy.leftBound != 0 || flyingEnemy.rightBound != 0)
            {
                flyingEnemy.position.x += flyingEnemy.direction * flyingEnemy.speed;
                if (flyingEnemy.position.x < flyingEnemy.leftBound)
                {
                    flyingEnemy.position.x = flyingEnemy.leftBound;
                    flyingEnemy.direction = 1;
                }
                else if (flyingEnemy.position.x > flyingEnemy.rightBound)
                {
                    flyingEnemy.position.x = flyingEnemy.rightBound;
                    flyingEnemy.direction = -1;
                }
            }

            // Sine wave vertical movement
            flyingEnemy.waveOffset += flyingEnemy.waveSpeed;
            flyingEnemy.position.y = flyingEnemy.baseY + sinf(flyingEnemy.waveOffset) * flyingEnemy.waveAmplitude;

            // Shooting
            flyingEnemy.shootTimer += 1.0f;
            float dx = playerPos.x - flyingEnemy.position.x;
            float dy = playerPos.y - flyingEnemy.position.y;
            float distSqr = dx * dx + dy * dy;

            if (!playerDead && distSqr < (enemyShootRange * enemyShootRange))
            {
                if (flyingEnemy.shootTimer >= flyingEnemy.shootCooldown)
                {
                    for (int b = 0; b < MAX_ENEMY_BULLETS; b++)
                    {
                        if (!enemyBullets[b].active)
                        {
                            enemyBullets[b].active = true;
                            enemyBullets[b].fromPlayer = false;
                            enemyBullets[b].position = flyingEnemy.position;

                            float len = sqrtf(dx * dx + dy * dy);
                            Vector2 dir = {0};
                            if (len > 0.0f)
                            {
                                dir.x = dx / len;
                                dir.y = dy / len;
                            }
                            enemyBullets[b].velocity.x = dir.x * bulletSpeed;
                            enemyBullets[b].velocity.y = dir.y * bulletSpeed;
                            break;
                        }
                    }

                    flyingEnemy.shootTimer = 0.0f;
                }
            }

            // Collisions with player bullets
            for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
            {
                if (playerBullets[i].active)
                {
                    float bx = playerBullets[i].position.x - flyingEnemy.position.x;
                    float by = playerBullets[i].position.y - flyingEnemy.position.y;
                    float dist2 = bx * bx + by * by;
                    float combined = bulletRadius + flyingEnemy.radius;
                    if (dist2 <= (combined * combined))
                    {
                        flyingEnemy.alive = false;
                        playerBullets[i].active = false;
                        break;
                    }
                }
            }
        }

        // --- Enemy bullets update ---
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        {
            if (enemyBullets[i].active)
            {
                enemyBullets[i].position.x += enemyBullets[i].velocity.x;
                enemyBullets[i].position.y += enemyBullets[i].velocity.y;

                // Off-screen?
                if (enemyBullets[i].position.x < 0 || enemyBullets[i].position.x > SCREEN_WIDTH ||
                    enemyBullets[i].position.y < 0 || enemyBullets[i].position.y > SCREEN_HEIGHT)
                {
                    enemyBullets[i].active = false;
                }

                // Collide with player
                if (!playerDead)
                {
                    float bx = enemyBullets[i].position.x - playerPos.x;
                    float by = enemyBullets[i].position.y - playerPos.y;
                    float dist2 = bx * bx + by * by;
                    float combined = bulletRadius + playerRadius;
                    if (dist2 <= (combined * combined))
                    {
                        playerHealth--;
                        enemyBullets[i].active = false;
                    }
                }
            }
        }

        // Draw the game when editorMode=false
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("GAME MODE: Tilemap collisions active. (ESC to exit)", 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("Player Health: %d", playerHealth), 600, 10, 20, MAROON);

        // Draw world (tilemap, enemies, player, etc.) using the camera
        BeginMode2D(camera);
        DrawTilemap(camera);

        if (!playerDead)
        {
            DrawCircleV(playerPos, playerRadius, RED);
            DrawLine((int)playerPos.x, (int)playerPos.y,
                     (int)screenPos.x, (int)screenPos.y,
                     GRAY);
        }
        else
        {
            DrawCircleV(playerPos, playerRadius, DARKGRAY);
            DrawText("YOU DIED!", SCREEN_WIDTH / 2 - 50, 30, 30, RED);
        }

        // Player bullets
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
        {
            if (playerBullets[i].active)
            {
                DrawCircle((int)playerBullets[i].position.x,
                           (int)playerBullets[i].position.y,
                           bulletRadius, BLUE);
            }
        }

        // Ground enemy as square
        if (groundEnemy.alive)
        {
            float halfSide = groundEnemy.radius;
            float side = groundEnemy.radius * 2;
            float gx = groundEnemy.position.x - halfSide;
            float gy = groundEnemy.position.y - halfSide;
            DrawRectangle((int)gx, (int)gy, (int)side, (int)side, GREEN);
            DrawText("GROUND ENEMY",
                     (int)(groundEnemy.position.x - 60),
                     (int)(gy - 20),
                     20, DARKGREEN);
        }
        else
        {
            DrawText("Ground enemy defeated!",
                     SCREEN_WIDTH / 2 - 100, 70, 20, DARKGREEN);
        }

        // Flying enemy as diamond
        if (flyingEnemy.alive)
        {
            DrawPoly(flyingEnemy.position, 4, flyingEnemy.radius, 45.0f, ORANGE);
            DrawText("FLYING ENEMY",
                     (int)(flyingEnemy.position.x - 60),
                     (int)(flyingEnemy.position.y - flyingEnemy.radius - 20),
                     20, BROWN);
        }
        else
        {
            DrawText("Flying enemy defeated!",
                     SCREEN_WIDTH / 2 - 100, 100, 20, BROWN);
        }

        // Enemy bullets
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
        {
            if (enemyBullets[i].active)
            {
                DrawCircle((int)enemyBullets[i].position.x,
                           (int)enemyBullets[i].position.y,
                           bulletRadius, PURPLE);
            }
        }

        EndMode2D();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
