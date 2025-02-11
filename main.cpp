/*******************************************************************************************
 *
 *   Raylib - 2D Platformer with Editor Mode
 *
 *   - In an EDITOR_BUILD the game starts in editor mode.
 *     In editor mode you can click & drag enemies, edit tiles, etc.
 *     A Play button (drawn in screen–space) is used to start the game.
 *   - In game mode, if compiled as an EDITOR_BUILD a Stop button is drawn which returns
 *     you to editor mode. In a release build (EDITOR_BUILD not defined) the game
 *     immediately starts in game mode and no editor UI is drawn.
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

// When compiling an editor build, start in editor mode; otherwise, game mode.
#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define LEVEL_WIDTH 3000
#define LEVEL_HEIGHT 800
#define TILE_SIZE 50
#define MAP_COLS (LEVEL_WIDTH / TILE_SIZE)
#define MAP_ROWS (LEVEL_HEIGHT / TILE_SIZE)

#define LEVEL_FILE "level.txt"
#define CHECKPOINT_FILE "checkpoint.txt"

#define MAX_ENEMIES 10
#define MAX_PLAYER_BULLETS 20
#define MAX_ENEMY_BULLETS 20
#define MAX_BULLETS (MAX_PLAYER_BULLETS + MAX_ENEMY_BULLETS)

// Define enemy type so we can differentiate behavior.
enum EnemyType
{
    ENEMY_GROUND = 0,
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

int mapTiles[MAP_ROWS][MAP_COLS] = {0}; // 0 = empty, 1 = solid, 2 = death
int draggedBoundEnemy = -1;             // index of enemy being bound-dragged
int boundType = -1;                     // 0 = left bound, 1 = right bound
bool draggingBound = false;
int enemyPlacementType = -1; // -1 = none; otherwise ENEMY_GROUND or ENEMY_FLYING.
int selectedEnemyIndex = -1;

void DrawTilemap(Camera2D camera)
{
    // Calculate the visible area in world coordinates:
    float camWorldWidth = LEVEL_WIDTH / camera.zoom;
    float camWorldHeight = LEVEL_HEIGHT / camera.zoom;

    float camLeft = camera.target.x - camWorldWidth / 2;
    float camRight = camera.target.x + camWorldWidth / 2;
    float camTop = camera.target.y - camWorldHeight / 2;
    float camBottom = camera.target.y + camWorldHeight / 2;

    int minTileX = (int)(camLeft / TILE_SIZE);
    int maxTileX = (int)(camRight / TILE_SIZE);
    int minTileY = (int)(camTop / TILE_SIZE);
    int maxTileY = (int)(camBottom / TILE_SIZE);

    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= MAP_COLS)
        maxTileX = MAP_COLS - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= MAP_ROWS)
        maxTileY = MAP_ROWS - 1;

    for (int y = minTileY; y <= maxTileY; y++)
    {
        for (int x = minTileX; x <= maxTileX; x++)
        {
            if (mapTiles[y][x] > 0)
            {
                DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE,
                              mapTiles[y][x] == 2 ? MAROON : BROWN);
            }
            else
            {
                DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, LIGHTGRAY);
            }
        }
    }
}

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius)
{
    float left = pos->x - radius;
    float right = pos->x + radius;
    float top = pos->y - radius;
    float bottom = pos->y + radius;

    int minTileX = (int)(left / TILE_SIZE);
    int maxTileX = (int)(right / TILE_SIZE);
    int minTileY = (int)(top / TILE_SIZE);
    int maxTileY = (int)(bottom / TILE_SIZE);

    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= MAP_COLS)
        maxTileX = MAP_COLS - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= MAP_ROWS)
        maxTileY = MAP_ROWS - 1;

    for (int ty = minTileY; ty <= maxTileY; ty++)
    {
        for (int tx = minTileX; tx <= maxTileX; tx++)
        {
            if (mapTiles[ty][tx] != 0)
            {
                Rectangle tileRect = {tx * TILE_SIZE, ty * TILE_SIZE, TILE_SIZE, TILE_SIZE};

                if (CheckCollisionCircleRec(*pos, radius, tileRect))
                {
                    if (mapTiles[ty][tx] == 1)
                    {
                        float overlapLeft = (tileRect.x + tileRect.width) - (pos->x - radius);
                        float overlapRight = (pos->x + radius) - tileRect.x;
                        float overlapTop = (tileRect.y + tileRect.height) - (pos->y - radius);
                        float overlapBottom = (pos->y + radius) - tileRect.y;

                        float minOverlap = overlapLeft;
                        char axis = 'x';
                        int sign = 1;

                        if (overlapRight < minOverlap)
                        {
                            minOverlap = overlapRight;
                            axis = 'x';
                            sign = -1;
                        }
                        if (overlapTop < minOverlap)
                        {
                            minOverlap = overlapTop;
                            axis = 'y';
                            sign = 1;
                        }
                        if (overlapBottom < minOverlap)
                        {
                            minOverlap = overlapBottom;
                            axis = 'y';
                            sign = -1;
                        }

                        if (axis == 'x')
                        {
                            pos->x += sign * minOverlap;
                            vel->x = 0;
                        }
                        else
                        {
                            pos->y += sign * minOverlap;
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
               struct Player player, struct Enemy enemies[MAX_ENEMIES],
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
    fprintf(file, "PLAYER %.2f %.2f %.2f\n", player.position.x, player.position.y, player.radius);

    int activeEnemies = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].health > 0)
            activeEnemies++;
    }
    fprintf(file, "ENEMY_COUNT %d\n", activeEnemies);
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].health > 0)
        {
            fprintf(file, "ENEMY %d %.2f %.2f %.2f %.2f %d %.2f %.2f\n",
                    enemies[i].type,
                    enemies[i].position.x, enemies[i].position.y,
                    enemies[i].leftBound, enemies[i].rightBound,
                    enemies[i].health, enemies[i].speed, enemies[i].shootCooldown);
        }
    }

    fprintf(file, "CHECKPOINT %.2f %.2f\n", checkpointPos.x, checkpointPos.y);
    fclose(file);
    return true;
}

bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               struct Player *player, struct Enemy enemies[MAX_ENEMIES],
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

    int enemyCount = 0;
    if (fscanf(file, "%s", token) != 1)
    {
        enemyCount = 0;
    }
    else if (strcmp(token, "ENEMY_COUNT") == 0)
    {
        if (fscanf(file, "%d", &enemyCount) != 1)
        {
            fclose(file);
            return false;
        }
    }
    else if (strcmp(token, "CHECKPOINT") == 0)
    {
        enemyCount = 0;
        fseek(file, -((long)strlen(token)), SEEK_CUR);
    }
    else
    {
        enemyCount = 0;
    }

    for (int i = 0; i < enemyCount; i++)
    {
        int type;
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
            break;
        if (fscanf(file, "%d %f %f %f %f %d %f %f", &type,
                   &enemies[i].position.x, &enemies[i].position.y,
                   &enemies[i].leftBound, &enemies[i].rightBound,
                   &enemies[i].health, &enemies[i].speed, &enemies[i].shootCooldown) != 8)
            break;
        enemies[i].type = (enum EnemyType)type;
        enemies[i].radius = 20;
        enemies[i].direction = -1;
        enemies[i].shootTimer = 0;
        enemies[i].baseY = 0;
        enemies[i].waveOffset = 0;
        enemies[i].waveAmplitude = 0;
        enemies[i].waveSpeed = 0;
        enemies[i].velocity = (Vector2){0, 0};
    }

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

bool SaveCheckpointState(const char *filename, struct Player player, struct Enemy enemies[MAX_ENEMIES])
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    fprintf(file, "PLAYER %.2f %.2f %d\n",
            player.position.x, player.position.y, player.health);

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        fprintf(file, "ENEMY %d %.2f %.2f %d\n",
                enemies[i].type,
                enemies[i].position.x, enemies[i].position.y,
                enemies[i].health);
    }

    fclose(file);
    return true;
}

bool LoadCheckpointState(const char *filename, struct Player *player, struct Enemy enemies[MAX_ENEMIES])
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        return false;

    char token[32];
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
        enemies[i].type = (enum EnemyType)type;
    }

    fclose(file);
    return true;
}

void UpdateBullets(struct Bullet bullets[], int count, float levelWidth, float levelHeight)
{
    for (int i = 0; i < count; i++)
    {
        if (bullets[i].active)
        {
            bullets[i].position.x += bullets[i].velocity.x;
            bullets[i].position.y += bullets[i].velocity.y;
            if (bullets[i].position.x < 0 || bullets[i].position.x > levelWidth ||
                bullets[i].position.y < 0 || bullets[i].position.y > levelHeight)
            {
                bullets[i].active = false;
            }
        }
    }
}

void CheckBulletCollisions(struct Bullet bullets[], int count, struct Player *player, struct Enemy enemies[], int enemyCount, float bulletRadius)
{
    for (int i = 0; i < count; i++)
    {
        if (!bullets[i].active)
            continue;

        if (bullets[i].fromPlayer)
        {
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
                        break;
                    }
                }
            }
        }
        else
        {
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

bool DrawButton(char *text, Rectangle rect, Color buttonColor, Color textColor)
{
    DrawRectangleRec(rect, buttonColor);
    DrawText(text, rect.x + 10, rect.y + 7, 20, BLACK);

    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rect);

    return clicked;
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
    struct Player player = {
        .position = {200.0f, 50.0f},
        .velocity = {0, 0},
        .radius = 20.0f,
        .health = 5};

    struct Enemy enemies[MAX_ENEMIES];
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].health = 0;
        enemies[i].velocity = (Vector2){0, 0};
        enemies[i].radius = 0;
        enemies[i].speed = 0;
        enemies[i].direction = 1;
        enemies[i].shootTimer = 0;
        enemies[i].shootCooldown = 0;
        enemies[i].baseY = 0;
        enemies[i].waveOffset = 0;
        enemies[i].waveAmplitude = 0;
        enemies[i].waveSpeed = 0;
    }

    float enemyShootRange = 300.0f;

    struct Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 10.0f;
    const float bulletRadius = 5.0f;

    bool checkpointExists = false;
    bool draggingCheckpoint = false;
    Vector2 checkpointPos = {0, 0};
    Rectangle checkpointRect = {0, 0, TILE_SIZE, TILE_SIZE * 2};

    bool draggingEnemy = false;
    int draggedEnemyIndex = -1;
    Vector2 dragOffset = {0};

    bool draggingPlayer = false;

    if (LoadLevel(LEVEL_FILE, mapTiles, &player, enemies, &checkpointPos))
    {
        checkpointExists = (checkpointPos.x != 0.0f || checkpointPos.y != 0.0f);
    }
    else
    {
        for (int x = 0; x < MAP_COLS; x++)
            mapTiles[MAP_ROWS - 1][x] = 1;
    }

    if (!editorMode)
    {
        if (!LoadCheckpointState(CHECKPOINT_FILE, &player, enemies))
        {
            TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
        }
    }

    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();
        Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);
        bool isOverUi = false;

        // -------------------------------
        // EDITOR MODE
        // -------------------------------
        if (editorMode)
        {
            if (IsKeyPressed(KEY_S))
            {
                if (SaveLevel(LEVEL_FILE, mapTiles, player, enemies, checkpointPos))
                    TraceLog(LOG_INFO, "Level saved successfully!");
                else
                    TraceLog(LOG_ERROR, "Failed to save Level!");
            }

            // Define header panel dimensions.
            const int headerHeight = 50;
            Rectangle headerPanel = {0, 0, SCREEN_WIDTH, headerHeight};

            // --- Header UI Elements ---
            // Tile tool buttons on the left.
            Rectangle btnGround = {10, 10, 100, 30};
            Rectangle btnDeath = {120, 10, 80, 30};
            Rectangle btnEraser = {210, 10, 100, 30};

            // Enemy addition buttons.
            Rectangle btnAddGroundEnemy = {320, 10, 160, 30};
            Rectangle btnAddFlyingEnemy = {490, 10, 140, 30};
            Rectangle enemyInspectorPanel = {0, headerHeight + 10, 200, 200};

            // Top right: Play button.
            Rectangle playButton = {SCREEN_WIDTH - 90, 10, 80, 30};
            Rectangle checkpointButton = {10, SCREEN_HEIGHT - 40, 150, 30};

            // Only process world–editing interactions if the mouse is NOT in the header.
            if (CheckCollisionPointRec(mousePos, headerPanel) || CheckCollisionPointRec(mousePos, enemyInspectorPanel))
            {
                isOverUi = true;
            }

            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))
            {
                Vector2 delta = GetMouseDelta();
                camera.target.x -= delta.x / camera.zoom;
                camera.target.y -= delta.y / camera.zoom;
            }

            float wheelMove = GetMouseWheelMove();
            if (wheelMove != 0)
            {
                camera.zoom += wheelMove * 0.05f;
                if (camera.zoom < 0.1f)
                    camera.zoom = 0.1f;
                if (camera.zoom > 3.0f)
                    camera.zoom = 3.0f;
            }

            // --- World Click Processing for Enemy Placement / Selection ---
            // (Before your existing enemy dragging code)
            if (enemyPlacementType != -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isOverUi)
            {
                // Check if the click is on an existing enemy:
                bool clickedOnEnemy = false;
                for (int i = 0; i < MAX_ENEMIES; i++)
                {
                    float dx = screenPos.x - enemies[i].position.x;
                    float dy = screenPos.y - enemies[i].position.y;
                    if ((dx * dx + dy * dy) <= (enemies[i].radius * enemies[i].radius))
                    {
                        clickedOnEnemy = true;
                        selectedEnemyIndex = i;  // select that enemy
                        enemyPlacementType = -1; // cancel placement mode
                        break;
                    }
                }
                // Also check if clicking on the player:
                float pdx = screenPos.x - player.position.x;
                float pdy = screenPos.y - player.position.y;
                if ((pdx * pdx + pdy * pdy) <= (player.radius * player.radius))
                {
                    clickedOnEnemy = true;
                }
                if (!clickedOnEnemy)
                {
                    // Find an empty slot (we consider an enemy slot empty if health <= 0)
                    int newIndex = -1;
                    for (int i = 0; i < MAX_ENEMIES; i++)
                    {
                        if (enemies[i].health <= 0)
                        {
                            newIndex = i;
                            break;
                        }
                    }
                    if (newIndex != -1)
                    {
                        // Create a new enemy in this slot with default parameters.
                        enemies[newIndex].type = (EnemyType)enemyPlacementType;
                        enemies[newIndex].position = screenPos;
                        enemies[newIndex].velocity = (Vector2){0, 0};
                        enemies[newIndex].radius = 20.0f;
                        enemies[newIndex].health = (enemyPlacementType == ENEMY_GROUND) ? 3 : 2;
                        enemies[newIndex].speed = (enemyPlacementType == ENEMY_GROUND) ? 2.0f : 1.5f;
                        enemies[newIndex].leftBound = screenPos.x - 50;
                        enemies[newIndex].rightBound = screenPos.x + 50;
                        enemies[newIndex].direction = 1;
                        enemies[newIndex].shootTimer = 0.0f;
                        enemies[newIndex].shootCooldown = (enemyPlacementType == ENEMY_GROUND) ? 60.0f : 90.0f;
                        enemies[newIndex].baseY = screenPos.y; // for flying enemy
                        enemies[newIndex].waveOffset = 0.0f;
                        enemies[newIndex].waveAmplitude = (enemyPlacementType == ENEMY_FLYING) ? 40.0f : 0.0f;
                        enemies[newIndex].waveSpeed = (enemyPlacementType == ENEMY_FLYING) ? 0.04f : 0.0f;

                        // Select the newly created enemy.
                        selectedEnemyIndex = newIndex;
                        // (Optionally, you can leave enemyPlacementType active so that multiple enemies can be added.)
                        // enemyPlacementType = -1; // Uncomment this if you want to cancel placement mode after one addition.
                    }
                }
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
                    dragOffset = (Vector2){dxP, dyP};
                }
            }
            if (draggingPlayer)
            {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                {
                    player.position.x = screenPos.x - dragOffset.x;
                    player.position.y = screenPos.y - dragOffset.y;
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
                    dragOffset.x = screenPos.x - checkpointPos.x;
                    dragOffset.y = screenPos.y - checkpointPos.y;
                }
                if (draggingCheckpoint)
                {
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
                    {
                        checkpointPos.x = screenPos.x - dragOffset.x;
                        checkpointPos.y = screenPos.y - dragOffset.y;
                    }
                    else
                    {
                        draggingCheckpoint = false;
                    }
                }
            }

            // --- Place/Remove Tiles Based on the Current Tool ---
            // We allow tile placement if not dragging any enemies or bounds:
            bool placementEditing = (draggingEnemy || draggingBound || draggingPlayer || draggingCheckpoint);
            if (enemyPlacementType == -1 && !placementEditing && !isOverUi && (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_RIGHT_BUTTON)))
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

            BeginDrawing();
            ClearBackground(RAYWHITE);

            DrawRectangleRec(headerPanel, LIGHTGRAY);

            if(DrawButton("Ground", btnGround, (currentTool == TOOL_GROUND) ? GREEN : WHITE, BLACK))
            {
                currentTool = TOOL_GROUND;
                enemyPlacementType = -1;
            }
            if(DrawButton("Death", btnDeath, (currentTool == TOOL_DEATH) ? GREEN : WHITE, BLACK))
            {
                currentTool = TOOL_DEATH;
                enemyPlacementType = -1;
            }
            if(DrawButton("Eraser", btnEraser, (currentTool == TOOL_ERASER) ? GREEN : WHITE, BLACK))
            {
                currentTool = TOOL_ERASER;
                enemyPlacementType = -1;
            }
            if(DrawButton("Ground Enemy", btnAddGroundEnemy, (enemyPlacementType == ENEMY_GROUND) ? GREEN : WHITE, BLACK))
            {
                enemyPlacementType = ENEMY_GROUND;
            }
            if(DrawButton("Flying Enemy", btnAddFlyingEnemy, (enemyPlacementType == ENEMY_FLYING) ? GREEN : WHITE, BLACK))
            {
                enemyPlacementType = ENEMY_FLYING;
            }
            if(DrawButton("Play", playButton, BLUE, BLACK))
            {
                // On Play, load checkpoint state for game mode.
                if (!LoadCheckpointState(CHECKPOINT_FILE, &player, enemies))
                    TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
                editorMode = false;
            }

            if(DrawButton("Create Checkpoint", checkpointButton, WHITE, BLACK))
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

            DrawRectangleRec(enemyInspectorPanel, LIGHTGRAY);
            DrawText("Enemy Inspector", enemyInspectorPanel.x + 5, enemyInspectorPanel.y + 5, 10, BLACK);
    
                // If an enemy is selected, show its data and provide editing buttons.
            if (selectedEnemyIndex != -1)
            {
                char info[128];
                sprintf(info, "Type: %s", (enemies[selectedEnemyIndex].type == ENEMY_GROUND) ? "Ground" : "Flying");
                DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
                sprintf(info, "Health: %d", enemies[selectedEnemyIndex].health);
                DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 65, 10, BLACK);
                sprintf(info, "Pos: %.0f, %.0f", enemies[selectedEnemyIndex].position.x, enemies[selectedEnemyIndex].position.y);
                DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 80, 10, BLACK);

                // Buttons to adjust health and toggle type.
                Rectangle btnHealthUp = {enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 40, 20};
                Rectangle btnHealthDown = {enemyInspectorPanel.x + 60, enemyInspectorPanel.y + 100, 40, 20};
                Rectangle btnToggleType = {enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 130, 90, 20};

                DrawRectangleRec(btnHealthUp, WHITE);
                DrawText("+", btnHealthUp.x + 15, btnHealthUp.y + 2, 10, BLACK);
                DrawRectangleRec(btnHealthDown, WHITE);
                DrawText("-", btnHealthDown.x + 15, btnHealthDown.y + 2, 10, BLACK);
                DrawRectangleRec(btnToggleType, WHITE);
                DrawText("Toggle Type", btnToggleType.x + 2, btnToggleType.y + 2, 10, BLACK);

                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    if (CheckCollisionPointRec(mousePos, btnHealthUp))
                    {
                        enemies[selectedEnemyIndex].health++;
                    }
                    else if (CheckCollisionPointRec(mousePos, btnHealthDown))
                    {
                        if (enemies[selectedEnemyIndex].health > 0)
                            enemies[selectedEnemyIndex].health--;
                    }
                    else if (CheckCollisionPointRec(mousePos, btnToggleType))
                    {
                        enemies[selectedEnemyIndex].type =
                            (enemies[selectedEnemyIndex].type == ENEMY_GROUND) ? ENEMY_FLYING : ENEMY_GROUND;
                    }
                }
            }
            else if (enemyPlacementType != -1)
            {
                // If no enemy is selected but a placement tool is active, show a placement message.
                if (enemyPlacementType == ENEMY_GROUND)
                    DrawText("Placement mode: Ground", enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
                else if (enemyPlacementType == ENEMY_FLYING)
                    DrawText("Placement mode: Flying", enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
            }

            BeginMode2D(camera);
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

        // -------------------------------
        // GAME MODE
        // -------------------------------
        camera.target = player.position;
        camera.rotation = 0.0f;
        camera.zoom = 0.66f;

        if (player.health > 0)
        {
            player.velocity.x = 0;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
                player.velocity.x = -4.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
                player.velocity.x = 4.0f;

            if (IsKeyPressed(KEY_SPACE))
            {
                if (fabsf(player.velocity.y) < 0.001f)
                    player.velocity.y = -8.0f;
            }

            player.velocity.y += 0.4f;
            player.position.x += player.velocity.x;
            player.position.y += player.velocity.y;
            ResolveCircleTileCollisions(&player.position, &player.velocity, &player.health, player.radius);
        }

        if (CheckCollisionPointRec(player.position, (Rectangle){checkpointPos.x, checkpointPos.y, TILE_SIZE, TILE_SIZE * 2}))
        {
            if (SaveCheckpointState(CHECKPOINT_FILE, player, enemies))
                TraceLog(LOG_INFO, "Checkpoint saved!");
        }

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

        // (Enemy update code remains the same as your original game logic)
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (enemies[i].health > 0)
            {
                if (enemies[i].type == ENEMY_GROUND)
                {
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

        UpdateBullets(bullets, MAX_BULLETS, LEVEL_WIDTH, LEVEL_HEIGHT);
        CheckBulletCollisions(bullets, MAX_BULLETS, &player, enemies, MAX_ENEMIES, bulletRadius);

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("GAME MODE: Tilemap collisions active. (ESC to exit)", 10, 10, 20, DARKGRAY);
        DrawText(TextFormat("Player Health: %d", player.health), 600, 10, 20, MAROON);

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
                DrawText("Press R to Respawn at Checkpoint", SCREEN_WIDTH / 2 - 130, 110, 20, DARKGRAY);

            if (checkpointExists && IsKeyPressed(KEY_R))
            {
                if (!LoadCheckpointState(CHECKPOINT_FILE, &player, enemies))
                    TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                else
                    TraceLog(LOG_INFO, "Checkpoint reloaded!");

                for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                    bullets[i].active = false;
                for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                    bullets[i].active = false;

                player.health = 5;
                player.velocity = (Vector2){0, 0};
                for (int i = 0; i < MAX_ENEMIES; i++)
                    enemies[i].velocity = (Vector2){0, 0};

                camera.target = player.position;
            }
            else if (IsKeyPressed(KEY_SPACE))
            {
                bool confirmNewGame = false;
                while (!WindowShouldClose() && !confirmNewGame)
                {
                    BeginDrawing();
                    ClearBackground(RAYWHITE);
                    DrawText("Are you sure you want to start a NEW GAME?", SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 40, 20, DARKGRAY);
                    DrawText("Press ENTER to confirm, or ESC to cancel.", SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2, 20, DARKGRAY);
                    EndDrawing();
                    if (IsKeyPressed(KEY_ENTER))
                        confirmNewGame = true;
                    else if (IsKeyPressed(KEY_ESCAPE))
                        break;
                }
                if (confirmNewGame)
                {
                    remove(CHECKPOINT_FILE);
                    checkpointExists = false;
                    checkpointPos = (Vector2){0, 0};
                    if (!LoadLevel(LEVEL_FILE, mapTiles, &player, enemies, &checkpointPos))
                        TraceLog(LOG_ERROR, "Failed to load level default state!");

                    player.health = 5;
                    for (int i = 0; i < MAX_BULLETS; i++)
                        bullets[i].active = false;
                    player.velocity = (Vector2){0, 0};
                    for (int i = 0; i < MAX_ENEMIES; i++)
                        enemies[i].velocity = (Vector2){0, 0};
                    camera.target = player.position;
                }
            }
        }

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

#ifdef EDITOR_BUILD
        // In an editor build, draw a Stop button in game mode so you can return to editor mode.
        Rectangle stopButton = {SCREEN_WIDTH - 90, 10, 80, 30};
        if(DrawButton("Stop", stopButton, LIGHTGRAY, BLACK))
        {
            if (!LoadLevel(LEVEL_FILE, mapTiles, &player, enemies, &checkpointPos))
                TraceLog(LOG_ERROR, "Failed to reload level for editor mode!");
            editorMode = true;
        }
#endif

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
