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

// In your main source file (e.g. main.c)
#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define LEVEL_WIDTH 3000
#define LEVEL_HEIGHT 800

#define LEVEL_FILE "level.txt"
#define CHECKPOINT_FILE "checkpoint.txt"

#define TILE_SIZE 50
#define MAP_COLS (LEVEL_WIDTH / TILE_SIZE)
#define MAP_ROWS (LEVEL_HEIGHT / TILE_SIZE)
#define MAX_PLAYER_BULLETS 20
#define MAX_ENEMY_BULLETS 20
#define MAX_BULLETS (MAX_PLAYER_BULLETS + MAX_ENEMY_BULLETS)

// New: maximum number of enemies in our array.
#define MAX_ENEMIES 2

// Define enemy type so we can differentiate behavior.
typedef enum EnemyType
{
    ENEMY_GROUND,
    ENEMY_FLYING
};

struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool fromPlayer; // true if shot by player, false if shot by enemy
};

struct Enemy
{
    EnemyType type;
    Vector2 position;
    Vector2 velocity;
    float radius;
    int health;
    float speed;
    float leftBound;
    float rightBound;
    int direction; // 1 = right, -1 = left
    float shootTimer;
    float shootCooldown;
    // For flying enemy only (sine wave)
    float baseY;
    float waveOffset;
    float waveAmplitude;
    float waveSpeed;
};

struct Player
{
    Vector2 position;
    Vector2 velocity;
    float radius;
    int health;
    float shootTimer;
    float shootCooldown;
};

enum TileTool
{
    TOOL_GROUND = 0,
    TOOL_DEATH = 1,
    TOOL_ERASER = 2
};

TileTool currentTool = TOOL_GROUND;

int mapTiles[MAP_ROWS][MAP_COLS] = {0}; // 0 = empty, 1 = solid
int draggedBoundEnemy = -1;             // 0 = ground enemy, 1 = flying enemy
int boundType = -1;                     // 0 = left bound, 1 = right bound
bool draggingBound = false;

void DrawTilemap(Camera2D camera)
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

    // Iterate over the visible tiles only:
    for (int y = minTileY; y <= maxTileY; y++)
    {
        for (int x = minTileX; x <= maxTileX; x++)
        {
            if (mapTiles[y][x] > 0)
            {
                // populated tile
                DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE,
                              mapTiles[y][x] == 2 ? MAROON : BROWN);
            }
            else
            {
                // Empty: draw grid lines
                DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, LIGHTGRAY);
            }
        }
    }
}

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius)
{
    // bounding box of the circle
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
            if (mapTiles[ty][tx] != 0)
            {
                Rectangle tileRect = {
                    tx * TILE_SIZE,
                    ty * TILE_SIZE,
                    TILE_SIZE,
                    TILE_SIZE};

                if (CheckCollisionCircleRec(*pos, radius, tileRect))
                {
                    if (mapTiles[ty][tx] == 1)
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
                    if (mapTiles[ty][tx] == 2)
                    {
                        *health = 0;
                    }
                }
            }
        }
    }
}

bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Player player, struct Enemy enemies[MAX_ENEMIES],
               Vector2 checkpointPos)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    fprintf(file, "%d %d\n", MAP_ROWS, MAP_COLS);
    for (int y = 0; y < MAP_ROWS; y++)
    {
        for (int x = 0; x < MAP_COLS; x++)
            fprintf(file, "%d ", mapTiles[y][x]);
        fprintf(file, "\n");
    }
    // Save player start position
    fprintf(file, "PLAYER %.2f %.2f %.2f\n", player.position.x, player.position.y, player.radius);

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        fprintf(file, "ENEMY %d %.2f %.2f %.2f %.2f %d\n",
                enemies[i].type,
                enemies[i].position.x, enemies[i].position.y,
                enemies[i].leftBound, enemies[i].rightBound,
                enemies[i].health);
    }

    fprintf(file, "CHECKPOINT %.2f %.2f\n", checkpointPos.x, checkpointPos.y);
    fclose(file);
    return true;
}

bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Player *player, Enemy enemies[MAX_ENEMIES],
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
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "PLAYER") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %f", &player->position.x, &player->position.y, &player->radius) != 3)
    {
        fclose(file);
        return false;
    }

    // Load each enemy from the file:
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        int type;
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
        {
            fclose(file);
            return false;
        }
        if (fscanf(file, "%d %f %f %f %f %d", &type,
                   &enemies[i].position.x, &enemies[i].position.y,
                   &enemies[i].leftBound, &enemies[i].rightBound,
                   &enemies[i].health) != 6)
        {
            fclose(file);
            return false;
        }
        enemies[i].type = (EnemyType)type;
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

bool SaveCheckpointState(const char *filename, Player player, Enemy enemies[MAX_ENEMIES])
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    // Save player state: position and health
    fprintf(file, "PLAYER %.2f %.2f %d\n",
            player.position.x, player.position.y, player.health);

    // Save each enemy in order, now including the type.
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        // Save type, then position and health.
        fprintf(file, "ENEMY %d %.2f %.2f %d\n",
                enemies[i].type,
                enemies[i].position.x, enemies[i].position.y,
                enemies[i].health);
    }

    fclose(file);
    return true;
}

bool LoadCheckpointState(const char *filename, Player *player, Enemy enemies[MAX_ENEMIES])
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
    if (fscanf(file, "%f %f %d",
               &player->position.x, &player->position.y, &player->health) != 3)
    {
        fclose(file);
        return false;
    }

    // Load each enemy from the file:
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        int type;
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
        {
            fclose(file);
            return false;
        }
        if (fscanf(file, "%d %f %f %d",
                   &type, &enemies[i].position.x, &enemies[i].position.y, &enemies[i].health) != 4)
        {
            fclose(file);
            return false;
        }
        enemies[i].type = (EnemyType)type;
    }

    fclose(file);
    return true;
}

void UpdateBullets(Bullet bullets[], int count, float levelWidth, float levelHeight)
{
    for (int i = 0; i < count; i++)
    {
        if (bullets[i].active)
        {
            bullets[i].position.x += bullets[i].velocity.x;
            bullets[i].position.y += bullets[i].velocity.y;
            // Check if bullet is off-screen (using level boundaries for player bullets and screen boundaries for enemy bullets)
            // Here, you may choose a common boundary (or test conditionally based on fromPlayer)
            if (bullets[i].position.x < 0 || bullets[i].position.x > levelWidth ||
                bullets[i].position.y < 0 || bullets[i].position.y > levelHeight)
            {
                bullets[i].active = false;
            }
        }
    }
}

void CheckBulletCollisions(Bullet bullets[], int count, Player *player, Enemy enemies[], int enemyCount, float bulletRadius)
{
    for (int i = 0; i < count; i++)
    {
        if (!bullets[i].active)
            continue;

        if (bullets[i].fromPlayer)
        {
            // Bullet fired by player: check collision with each enemy
            for (int e = 0; e < enemyCount; e++)
            {
                if (enemies[e].health > 0)
                {
                    float dx = bullets[i].position.x - enemies[e].position.x;
                    float dy = bullets[i].position.y - enemies[e].position.y;
                    float dist2 = dx * dx + dy * dy;
                    float combined = bulletRadius + enemies[e].radius;
                    if (dist2 <= (combined * combined))
                    {
                        enemies[e].health--;
                        bullets[i].active = false;
                        break; // A bullet can hit only one enemy.
                    }
                }
            }
        }
        else
        {
            // Bullet fired by enemy: check collision with the player
            float dx = bullets[i].position.x - player->position.x;
            float dy = bullets[i].position.y - player->position.y;
            float dist2 = dx * dx + dy * dy;
            float combined = bulletRadius + player->radius;
            if (dist2 <= (combined * combined))
            {
                player->health--;
                bullets[i].active = false;
            }
        }
    }
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
    Player player = {
        .position = {200.0f, 50.0f},
        .velocity = {0, 0},
        .radius = 20.0f,
        .health = 5};

    // Instead of separate enemy variables, create an array:
    Enemy enemies[MAX_ENEMIES];

    // Initialize enemy 0 as ground enemy
    enemies[0].position = (Vector2){400.0f, 50.0f};
    enemies[0].velocity = (Vector2){0, 0};
    enemies[0].radius = 20.0f;
    enemies[0].health = 3;
    enemies[0].speed = 2.0f;
    enemies[0].leftBound = 300.0f;
    enemies[0].rightBound = 500.0f;
    enemies[0].direction = 1;
    enemies[0].shootTimer = 0.0f;
    enemies[0].shootCooldown = 60.0f;
    enemies[0].baseY = 0.0f; // Not used for ground enemy
    enemies[0].waveOffset = 0.0f;
    enemies[0].waveAmplitude = 0.0f;
    enemies[0].waveSpeed = 0.0f;
    enemies[0].type = ENEMY_GROUND;

    // Initialize enemy 1 as flying enemy
    enemies[1].position = (Vector2){600.0f, 100.0f};
    enemies[1].velocity = (Vector2){0, 0};
    enemies[1].radius = 20.0f;
    enemies[1].health = 2;
    enemies[1].speed = 1.5f;
    enemies[1].leftBound = 500.0f;
    enemies[1].rightBound = 700.0f;
    enemies[1].direction = 1;
    enemies[1].shootTimer = 0.0f;
    enemies[1].shootCooldown = 90.0f;
    enemies[1].baseY = 100.0f;
    enemies[1].waveOffset = 0.0f;
    enemies[1].waveAmplitude = 40.0f;
    enemies[1].waveSpeed = 0.04f;
    enemies[1].type = ENEMY_FLYING;

    float enemyShootRange = 300.0f;

    // Bullets
    Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 10.0f;
    const float bulletRadius = 5.0f;

    // Checkpoint variables
    bool checkpointExists = false;
    bool draggingCheckpoint = false;
    Vector2 checkpointPos = {0, 0};
    Vector2 checkpointDragOffset = {0, 0};
    Rectangle checkpointRect = {0, 0, TILE_SIZE, TILE_SIZE * 2};

    // Editor / Game mode state
    bool draggingEnemy = false;
    bool isOverUi = false;
    int draggedEnemyIndex = -1;
    Vector2 dragOffset = {0};

    bool draggingPlayer = false;
    Vector2 playerDragOffset = {0, 0};

    if (LoadLevel(LEVEL_FILE, mapTiles, &player, enemies, &checkpointPos))
    {
        checkpointExists = (checkpointPos.x != 0.0f || checkpointPos.y != 0.0f);
    }
    else
    {
        // Initialize default tilemap (for example, fill the bottom row with ground)
        for (int x = 0; x < MAP_COLS; x++)
            mapTiles[MAP_ROWS - 1][x] = 1;
    }

    // Try loading from the checkpoint file
    if (!LoadCheckpointState(CHECKPOINT_FILE, &player, enemies))
    {
        TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
    }

    while (!WindowShouldClose())
    {
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
                if (SaveLevel(LEVEL_FILE, mapTiles, player, enemies, checkpointPos))
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

            if (!draggingEnemy && !draggingBound && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                // Loop over enemies and check if mouse is near a bound marker (within 10 pixels)
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    if (fabsf(screenPos.x - enemies[i].leftBound) < 10)
                    {
                        draggingBound = true;
                        draggedBoundEnemy = i;
                        boundType = 0;
                        break;
                    }
                    else if (fabsf(screenPos.x - enemies[i].rightBound) < 10)
                    {
                        draggingBound = true;
                        draggedBoundEnemy = i;
                        boundType = 1;
                        break;
                    }
                }
            }
            if (draggingBound)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    if (boundType == 0)
                        enemies[draggedBoundEnemy].leftBound = screenPos.x;
                    else
                        enemies[draggedBoundEnemy].rightBound = screenPos.x;
                }
                else
                {
                    draggingBound = false;
                    draggedBoundEnemy = -1;
                    boundType = -1;
                }
            }

            // --- Enemy Dragging ---
            if (!draggingEnemy && !draggingBound && !IsMouseButtonDown(MOUSE_RIGHT_BUTTON))
            {
                // Loop over each enemy and check if mouse is inside its radius.
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    float dx = screenPos.x - enemies[i].position.x;
                    float dy = screenPos.y - enemies[i].position.y;
                    if ((dx * dx + dy * dy) <= (enemies[i].radius * enemies[i].radius))
                    {
                        draggingEnemy = true;
                        draggedEnemyIndex = i;
                        dragOffset = (Vector2){dx, dy};
                        break;
                    }
                }
            }
            if (draggingEnemy)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    enemies[draggedEnemyIndex].position.x = screenPos.x - dragOffset.x;
                    enemies[draggedEnemyIndex].position.y = screenPos.y - dragOffset.y;
                    if (enemies[draggedEnemyIndex].type == ENEMY_FLYING)
                        enemies[draggedEnemyIndex].baseY = enemies[draggedEnemyIndex].position.y;
                }
                else
                {
                    draggingEnemy = false;
                    draggedEnemyIndex = -1;
                }
            }

            // -- Player Dragging --
            if (!draggingEnemy && !draggingBound && !draggingCheckpoint && !isOverUi && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                float dxP = screenPos.x - player.position.x;
                float dyP = screenPos.y - player.position.y;
                if ((dxP * dxP + dyP * dyP) <= (player.radius * player.radius))
                {
                    draggingPlayer = true;
                    playerDragOffset = (Vector2){dxP, dyP};
                }
            }
            if (draggingPlayer)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    player.position.x = screenPos.x - playerDragOffset.x;
                    player.position.y = screenPos.y - playerDragOffset.y;
                }
                else
                {
                    draggingPlayer = false;
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

            // --- Tilemap Panel---
            Rectangle toolPanel = {SCREEN_WIDTH - 210, 10, 200, 150};

            // Define button rectangles within the panel:
            Rectangle btnGround = {toolPanel.x + 10, toolPanel.y + 10, 180, 30};
            Rectangle btnDeath = {toolPanel.x + 10, toolPanel.y + 50, 180, 30};
            Rectangle btnEraser = {toolPanel.x + 10, toolPanel.y + 90, 180, 30};
            Rectangle btnClearAll = {toolPanel.x + 10, toolPanel.y + 130, 180, 30};

            isOverUi = CheckCollisionPointRec(mousePos, toolPanel);

            // Check for button clicks (using screen coordinates)
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                if (CheckCollisionPointRec(mousePos, btnGround))
                {
                    currentTool = TOOL_GROUND;
                }
                else if (CheckCollisionPointRec(mousePos, btnDeath))
                {
                    currentTool = TOOL_DEATH;
                }
                else if (CheckCollisionPointRec(mousePos, btnEraser))
                {
                    currentTool = TOOL_ERASER;
                }
                else if (CheckCollisionPointRec(mousePos, btnClearAll))
                {
                    // Clear all tiles:
                    for (int y = 0; y < MAP_ROWS; y++)
                        for (int x = 0; x < MAP_COLS; x++)
                            mapTiles[y][x] = 0;
                }
            }

            // --- Place/Remove Tiles Based on the Current Tool ---
            // We allow tile placement if not dragging any enemies or bounds:
            bool enemyEditing = (draggingEnemy || draggingBound);
            if (!enemyEditing && !isOverUi && (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_RIGHT_BUTTON)))
            {
                // Use worldPos (converted via GetScreenToWorld2D) for tile placement.
                int tileX = (int)(screenPos.x / TILE_SIZE);
                int tileY = (int)(screenPos.y / TILE_SIZE);
                if (tileX >= 0 && tileX < MAP_COLS && tileY >= 0 && tileY < MAP_ROWS)
                {
                    // Depending on the current tool, set the tile value:
                    if (currentTool == TOOL_GROUND && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                    {
                        mapTiles[tileY][tileX] = 1;
                    }
                    else if (currentTool == TOOL_DEATH && IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                    {
                        mapTiles[tileY][tileX] = 2;
                    }
                    else if (currentTool == TOOL_ERASER)
                    {
                        // Eraser: remove tile regardless of mouse button (or require RMB)
                        mapTiles[tileY][tileX] = 0;
                    }
                }
            }

            // Draw Editor View
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("EDITOR MODE: LMB=Use Tile tool/drag enemy, ENTER=Play", 10, 10, 20, DARKGRAY);

            // Draw the tool panel (UI elements in screen space)
            DrawRectangleRec(toolPanel, LIGHTGRAY);
            DrawRectangleRec(btnGround, (currentTool == TOOL_GROUND) ? GREEN : WHITE);
            DrawRectangleRec(btnDeath, (currentTool == TOOL_DEATH) ? GREEN : WHITE);
            DrawRectangleRec(btnEraser, (currentTool == TOOL_ERASER) ? GREEN : WHITE);
            DrawRectangleRec(btnClearAll, WHITE);
            DrawText("Ground Tile", btnGround.x + 10, btnGround.y + 8, 12, BLACK);
            DrawText("Death Tile", btnDeath.x + 10, btnDeath.y + 8, 12, BLACK);
            DrawText("Eraser", btnEraser.x + 10, btnEraser.y + 8, 12, BLACK);
            DrawText("Clear All", btnClearAll.x + 10, btnClearAll.y + 8, 12, BLACK);

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
                DrawRectangle(checkpointPos.x, checkpointPos.y, TILE_SIZE, TILE_SIZE * 2, Fade(GREEN, 0.3f));
            }

            // Draw enemies
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (enemies[i].health > 0)
                {
                    if (enemies[i].type == ENEMY_GROUND)
                    {
                        float halfSide = enemies[i].radius;
                        DrawRectangle((int)(enemies[i].position.x - halfSide),
                                      (int)(enemies[i].position.y - halfSide),
                                      (int)(enemies[i].radius * 2),
                                      (int)(enemies[i].radius * 2), RED);
                    }
                    else if (enemies[i].type == ENEMY_FLYING)
                    {
                        DrawPoly(enemies[i].position, 4, enemies[i].radius, 45.0f, ORANGE);
                    }
                    // Draw bound markers:
                    DrawLine((int)enemies[i].leftBound, 0, (int)enemies[i].leftBound, 20, BLUE);
                    DrawLine((int)enemies[i].rightBound, 0, (int)enemies[i].rightBound, 20, BLUE);
                }
            }

            DrawCircleV(player.position, player.radius, BLUE);
            DrawText("PLAYER", player.position.x - 20, player.position.y - player.radius - 20, 12, BLACK);

            EndMode2D();
            EndDrawing();
            continue;
        }

        // GAME MODE: The normal platformer logic begins here
        camera.target = player.position;
        camera.rotation = 0.0f;
        camera.zoom = .66f;

        if (player.health > 0)
        {
            // Player input
            player.velocity.x = 0;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
            {
                player.velocity.x = -4.0f;
            }
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
            {
                player.velocity.x = 4.0f;
            }

            // Jump
            bool onGround = false;
            if (IsKeyPressed(KEY_SPACE))
            {
                // hack: only jump if velocity.y == 0
                if (fabsf(player.velocity.y) < 0.001f)
                {
                    player.velocity.y = -8.0f;
                }
            }

            // Gravity
            player.velocity.y += 0.4f;

            // Move
            player.position.x += player.velocity.x;
            player.position.y += player.velocity.y;

            // Collision with tilemap
            ResolveCircleTileCollisions(&player.position, &player.velocity, &player.health, player.radius);

            // If the velocity.y == 0 after collisions, we consider the player on ground
            if (fabsf(player.velocity.y) < 0.001f)
            {
                onGround = true;
            }

            if (CheckCollisionPointRec(player.position, {checkpointPos.x, checkpointPos.y, TILE_SIZE, TILE_SIZE * 2}))
            {
                // Save the current state as a checkpoint.
                if (SaveCheckpointState("checkpoint.txt", player, enemies))
                {
                    TraceLog(LOG_INFO, "Checkpoint saved!");
                }
            }

            // Shooting
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                for (int i = 0; i < MAX_BULLETS; i++)
                {
                    if (!bullets[i].active)
                    {
                        bullets[i].active = true;
                        bullets[i].fromPlayer = true;
                        bullets[i].position = player.position;

                        Vector2 dir = {screenPos.x - player.position.x, screenPos.y - player.position.y};
                        float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                        if (len > 0.0f)
                        {
                            dir.x /= len;
                            dir.y /= len;
                        }
                        bullets[i].velocity.x = dir.x * bulletSpeed;
                        bullets[i].velocity.y = dir.y * bulletSpeed;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (enemies[i].health > 0)
            {
                if (enemies[i].type == ENEMY_GROUND)
                {
                    // Ground enemy: apply gravity and patrol bounds.
                    enemies[i].velocity.x = enemies[i].direction * enemies[i].speed;
                    enemies[i].velocity.y += 0.4f;
                    enemies[i].position.x += enemies[i].velocity.x;
                    enemies[i].position.y += enemies[i].velocity.y;
                    ResolveCircleTileCollisions(&enemies[i].position, &enemies[i].velocity, &enemies[i].health, enemies[i].radius);
                    if (enemies[i].leftBound != 0 || enemies[i].rightBound != 0)
                    {
                        if (enemies[i].position.x < enemies[i].leftBound)
                        {
                            enemies[i].position.x = enemies[i].leftBound;
                            enemies[i].direction = 1;
                        }
                        else if (enemies[i].position.x > enemies[i].rightBound)
                        {
                            enemies[i].position.x = enemies[i].rightBound;
                            enemies[i].direction = -1;
                        }
                    }
                }
                else if (enemies[i].type == ENEMY_FLYING)
                {
                    // Flying enemy: horizontal patrol and sine wave vertical movement.
                    if (enemies[i].leftBound != 0 || enemies[i].rightBound != 0)
                    {
                        enemies[i].position.x += enemies[i].direction * enemies[i].speed;
                        if (enemies[i].position.x < enemies[i].leftBound)
                        {
                            enemies[i].position.x = enemies[i].leftBound;
                            enemies[i].direction = 1;
                        }
                        else if (enemies[i].position.x > enemies[i].rightBound)
                        {
                            enemies[i].position.x = enemies[i].rightBound;
                            enemies[i].direction = -1;
                        }
                    }
                    enemies[i].waveOffset += enemies[i].waveSpeed;
                    enemies[i].position.y = enemies[i].baseY + sinf(enemies[i].waveOffset) * enemies[i].waveAmplitude;
                }

                // Shooting
                enemies[i].shootTimer += 1.0f;
                float dx = player.position.x - enemies[i].position.x;
                float dy = player.position.y - enemies[i].position.y;
                float distSqr = dx * dx + dy * dy;

                if (player.health > 0 && distSqr < (enemyShootRange * enemyShootRange))
                {
                    if (enemies[i].shootTimer >= enemies[i].shootCooldown)
                    {
                        for (int b = 0; b < MAX_BULLETS; b++)
                        {
                            if (!bullets[b].active)
                            {
                                bullets[b].active = true;
                                bullets[b].fromPlayer = false;
                                bullets[b].position = enemies[i].position;

                                float len = sqrtf(dx * dx + dy * dy);
                                Vector2 dir = {0};
                                if (len > 0.0f)
                                {
                                    dir.x = dx / len;
                                    dir.y = dy / len;
                                }
                                bullets[b].velocity.x = dir.x * bulletSpeed;
                                bullets[b].velocity.y = dir.y * bulletSpeed;
                                break;
                            }
                        }

                        enemies[i].shootTimer = 0.0f;
                    }
                }
            }
        }

        // Update all bullets:
        UpdateBullets(bullets, MAX_BULLETS, LEVEL_WIDTH, LEVEL_HEIGHT);

        // Check collisions:
        CheckBulletCollisions(bullets, MAX_BULLETS, &player, enemies, MAX_ENEMIES, bulletRadius);

        // Draw
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("GAME MODE: Tilemap collisions active. (ESC to exit)", 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("Player Health: %d", player.health), 600, 10, 20, MAROON);

        // Draw world (tilemap, enemies, player, etc.) using the camera
        BeginMode2D(camera);
        DrawTilemap(camera);

        if (player.health > 0)
        {
            DrawCircleV(player.position, player.radius, RED);
            DrawLine((int)player.position.x, (int)player.position.y,
                     (int)screenPos.x, (int)screenPos.y,
                     GRAY);
        }
        else
        {
            DrawCircleV(player.position, player.radius, DARKGRAY);
            DrawText("YOU DIED!", SCREEN_WIDTH / 2 - 50, 30, 30, RED);
            DrawText("Press SPACE for New Game", SCREEN_WIDTH / 2 - 100, 80, 20, DARKGRAY);
            if (checkpointExists)
            {
                DrawText("Press R to Respawn at Checkpoint", SCREEN_WIDTH / 2 - 130, 110, 20, DARKGRAY);
            }

            if (checkpointExists && IsKeyPressed(KEY_R))
            {
                // Respawn using the checkpoint state:
                if (!LoadCheckpointState(CHECKPOINT_FILE, &player, enemies))
                {
                    TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                }
                else
                {
                    TraceLog(LOG_INFO, "Checkpoint reloaded!");
                }
                // Reset transient state (clear bullets, reset velocities, etc.)
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                {
                    bullets[i].active = false;
                }

                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                {
                    bullets[i].active = false;
                }

                player.health = 5;
                player.velocity = (Vector2){0, 0};
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    enemies[i].velocity = (Vector2){0, 0};
                }

                camera.target = player.position;
                // Continue game loop with respawn state.
            }
            else if (IsKeyPressed(KEY_SPACE))
            {
                // Prompt user for confirmation to start a new game (which will clear checkpoint save)
                bool confirmNewGame = false;
                while (!WindowShouldClose() && !confirmNewGame)
                {
                    BeginDrawing();
                    ClearBackground(RAYWHITE);
                    DrawText("Are you sure you want to start a NEW GAME?", SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 40, 20, DARKGRAY);
                    DrawText("Press ENTER to confirm, or ESC to cancel.", SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2, 20, DARKGRAY);
                    EndDrawing();
                    if (IsKeyPressed(KEY_ENTER))
                    {
                        confirmNewGame = true;
                    }
                    else if (IsKeyPressed(KEY_ESCAPE))
                    {
                        break; // cancel new game
                    }
                }
                if (confirmNewGame)
                {
                    // Delete the checkpoint file (or reset the checkpoint variables)
                    remove(CHECKPOINT_FILE);
                    checkpointExists = false;
                    checkpointPos = (Vector2){0, 0};

                    // Reload the default level state from level.txt:
                    if (!LoadLevel(LEVEL_FILE, mapTiles, &player, enemies, &checkpointPos))
                    {
                        TraceLog(LOG_ERROR, "Failed to load level default state!");
                    }

                    player.health = 5;
                    // Reset bullets and velocities:
                    for (int i = 0; i < MAX_BULLETS; i++)
                        bullets[i].active = false;
                    player.velocity = (Vector2){0, 0};
                    for (int i = 0; i < MAX_ENEMIES; i++)
                    {
                        enemies[i].velocity = (Vector2){0, 0};
                    }
                    camera.target = player.position;
                }
            }
        }

        // Draw enemies:
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (enemies[i].health > 0)
            {
                if (enemies[i].type == ENEMY_GROUND)
                {
                    float halfSide = enemies[i].radius;
                    DrawRectangle((int)(enemies[i].position.x - halfSide),
                                  (int)(enemies[i].position.y - halfSide),
                                  (int)(enemies[i].radius * 2),
                                  (int)(enemies[i].radius * 2), GREEN);
                }
                else if (enemies[i].type == ENEMY_FLYING)
                {
                    DrawPoly(enemies[i].position, 4, enemies[i].radius, 45.0f, ORANGE);
                }
            }
        }

        // Player bullets
        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (bullets[i].active)
            {
                DrawCircle((int)bullets[i].position.x,
                           (int)bullets[i].position.y,
                           bulletRadius, BLUE);
            }
        }

        EndMode2D();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
