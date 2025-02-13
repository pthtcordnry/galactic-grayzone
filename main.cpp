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
#include "memory_arena.h"
#include "file_io.h"
#include "editor_mode.h"
#include "game_state.h"
#include "game_ui.h"
#include "main.h"

#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

GameState *gameState = NULL;
Camera2D camera;
int mapTiles[MAP_ROWS][MAP_COLS] = {0}; // 0 = empty, 1 = solid, 2 = death

struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool fromPlayer; // true if shot by player, false if shot by enemy
};

bool checkpointActivated[MAX_CHECKPOINTS] = {false};

struct Particle
{
    Vector2 position;
    Vector2 velocity;
    float life; // frames remaining
    Color color;
};

Particle particles[MAX_PARTICLES];

void InitParticle(Particle *p)
{
    p->position = (Vector2){GetRandomValue(0, SCREEN_WIDTH), GetRandomValue(0, SCREEN_HEIGHT / 2)};
    float angle = GetRandomValue(0, 360) * DEG2RAD;
    float speed = GetRandomValue(1, 5);
    p->velocity = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
    p->life = GetRandomValue(60, 180);
    p->color = (Color){GetRandomValue(100, 255), GetRandomValue(100, 255), GetRandomValue(100, 255), 255};
}

void UpdateAndDrawFireworks(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        // Update particle position
        particles[i].position.x += particles[i].velocity.x;
        particles[i].position.y += particles[i].velocity.y;
        // Decrease life
        particles[i].life -= 1.0f;
        if (particles[i].life <= 0)
            InitParticle(&particles[i]);
        // Optionally, fade out the alpha:
        particles[i].color.a = (unsigned char)(255 * (particles[i].life / 180.0f));
        DrawCircleV(particles[i].position, 2, particles[i].color);
    }
}

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
               struct Enemy bossEnemy, Vector2 checkpointPos)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    fprintf(file, "%d %d\n", MAP_ROWS, MAP_COLS);
    // Save tile map...
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
    // Only write boss data if a boss has been placed:
    if (bossEnemy.type != ENEMY_NONE)
    {
        fprintf(file, "BOSS %.2f %.2f %.2f %.2f %d %.2f %.2f\n",
                bossEnemy.position.x, bossEnemy.position.y,
                bossEnemy.leftBound, bossEnemy.rightBound,
                bossEnemy.health, bossEnemy.speed, bossEnemy.shootCooldown);
    }
    // Save checkpoints...
    fprintf(file, "CHECKPOINT_COUNT %d\n", gameState->checkpointCount);
    for (int i = 0; i < gameState->checkpointCount; i++)
    {
        fprintf(file, "CHECKPOINT %.2f %.2f\n", gameState->checkpoints[i].x, gameState->checkpoints[i].y);
    }
    fclose(file);
    return true;
}

bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               struct Player *player, struct Enemy enemies[MAX_ENEMIES], struct Enemy *bossEnemy,
               Vector2 checkpoints[], int *checkpointCount)
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
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") == 0)
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
        enemies[i].baseY = enemies[i].position.y;
        enemies[i].waveOffset = 0;
        enemies[i].waveAmplitude = 0;
        enemies[i].waveSpeed = 0;
        enemies[i].velocity = (Vector2){0, 0};
    }

    if (fscanf(file, "%s", token) == 1 && strcmp(token, "BOSS") == 0 &&
        fscanf(file, "%f %f %f %f %d %f %f",
               &bossEnemy->position.x, &bossEnemy->position.y,
               &bossEnemy->leftBound, &bossEnemy->rightBound,
               &bossEnemy->health, &bossEnemy->speed, &bossEnemy->shootCooldown) == 7)
    {
        // Successfully read boss data – initialize additional boss values.
        bossEnemy->type = ENEMY_FLYING;
        bossEnemy->baseY = bossEnemy->position.y - 200;
        bossEnemy->radius = 40.0f;
        bossEnemy->health = BOSS_MAX_HEALTH;
        bossEnemy->speed = 2.0f; // initial phase 1 speed
        bossEnemy->leftBound = 100;
        bossEnemy->rightBound = LEVEL_WIDTH - 100;
        bossEnemy->direction = 1;
        bossEnemy->shootTimer = 0;
        bossEnemy->shootCooldown = 120.0f;
        bossEnemy->waveOffset = 0;
        bossEnemy->waveAmplitude = 20.0f;
        bossEnemy->waveSpeed = 0.02f;
    }

    // Read checkpoint count.
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "CHECKPOINT_COUNT") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%d", checkpointCount) != 1)
    {
        fclose(file);
        return false;
    }
    // Read checkpoint entries.
    for (int i = 0; i < *checkpointCount; i++)
    {
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "CHECKPOINT") != 0)
            break;
        if (fscanf(file, "%f %f", &checkpoints[i].x, &checkpoints[i].y) != 2)
            break;
    }

    fclose(file);
    return true;
}

bool SaveCheckpointState(const char *filename, struct Player player,
                         struct Enemy enemies[MAX_ENEMIES], struct Enemy bossEnemy,
                         Vector2 checkpoints[], int checkpointCount, int currentIndex)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    // Save player data.
    fprintf(file, "PLAYER %.2f %.2f %d\n",
            player.position.x, player.position.y, player.health);

    // Count how many enemy slots are used (even if health == 0).
    int enemyCount = 0;
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].type != ENEMY_NONE) // using ENEMY_NONE as the marker for an unused slot
            enemyCount++;
    }
    fprintf(file, "ENEMY_COUNT %d\n", enemyCount);

    // Save only the created enemy entries.
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (enemies[i].type != ENEMY_NONE)
        {
            fprintf(file, "ENEMY %d %.2f %.2f %d\n",
                    enemies[i].type,
                    enemies[i].position.x, enemies[i].position.y,
                    enemies[i].health);
        }
    }

    // Save boss details.
    fprintf(file, "BOSS %.2f %.2f %d\n",
            bossEnemy.position.x, bossEnemy.position.y,
            bossEnemy.health);

    // Save the last checkpoint index.
    fprintf(file, "LAST_CHECKPOINT_INDEX %d\n", currentIndex);

    fclose(file);
    return true;
}

bool LoadCheckpointState(const char *filename, struct Player *player,
                         struct Enemy enemies[MAX_ENEMIES], struct Enemy *bossEnemy,
                         Vector2 checkpoints[], int *checkpointCount)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        return false;

    char token[32];

    // Read player data.
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "PLAYER") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %d", &player->position.x, &player->position.y, &player->health) != 3)
    {
        fclose(file);
        return false;
    }

    // Read enemy count.
    int enemyCount = 0;
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY_COUNT") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%d", &enemyCount) != 1)
    {
        fclose(file);
        return false;
    }

    // Initialize enemy slots as unused.
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        enemies[i].type = ENEMY_NONE;
    }
    // Read enemy entries.
    for (int i = 0; i < enemyCount; i++)
    {
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
            break;
        if (fscanf(file, "%d %f %f %d",
                   (int *)&enemies[i].type,
                   &enemies[i].position.x, &enemies[i].position.y,
                   &enemies[i].health) != 4)
            break;
    }

    // Read boss data.
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "BOSS") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %d",
               &bossEnemy->position.x, &bossEnemy->position.y,
               &bossEnemy->health) != 3)
    {
        fclose(file);
        return false;
    }

    // Load the last checkpoint index.
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "LAST_CHECKPOINT_INDEX") != 0)
    {
        fclose(file);
        return false;
    }
    int currentIndex = -1;
    if (fscanf(file, "%d", &currentIndex) != 1)
    {
        fclose(file);
        return false;
    }
    else
    {
        for (int i = 0; i <= currentIndex; i++)
        {
            checkpointActivated[i] = true;
        }
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

void CheckBulletCollisions(struct Bullet bullets[], int count, struct Player *player, struct Enemy enemies[], int enemyCount, struct Enemy *bossEnemy, bool *bossActive, bool *gameWon, float bulletRadius)
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

            if (*bossActive)
            {
                float dx = bullets[i].position.x - bossEnemy->position.x;
                float dy = bullets[i].position.y - bossEnemy->position.y;
                float dist2 = dx * dx + dy * dy;
                float combined = bulletRadius + bossEnemy->radius;
                if (dist2 <= (combined * combined))
                {
                    bossEnemy->health--;
                    bullets[i].active = false;
                    if (bossEnemy->health <= 0)
                    {
                        *bossActive = false;
                        *gameWon = true;
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

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer Test");
    SetTargetFPS(60);
    
    gameState = (GameState *)malloc(sizeof(GameState));
    if (gameState == NULL)
    {
        // Handle allocation failure (e.g., exit the program)
        fprintf(stderr, "Failed to allocate memory for gameState.\n");
        exit(1);
    }
    
    arena_init(&gameState->gameArena, GAME_ARENA_SIZE);
    mapTiles[MAP_ROWS][MAP_COLS] = {0};

    gameState->editorMode = &editorMode;

    // --- AUDIO INITIALIZATION ---
    InitAudioDevice();
    // Load a main music track and shot sound (update the paths as needed)
    Music music = LoadMusicStream("resources/music.mp3");
    Music victoryMusic = LoadMusicStream("resources/victory.mp3");
    Music *currentTrack = &music;
    Sound defeatMusic = LoadSound("resources/defeat.mp3");
    Sound shotSound = LoadSound("resources/shot.mp3");
    if (!editorMode)
        PlayMusicStream(music);

    camera = {0};
    camera.target = (Vector2){LEVEL_WIDTH / 2.0f, LEVEL_HEIGHT / 2.0f};
    camera.offset = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    gameState->checkpoints[MAX_CHECKPOINTS] = {0};
    gameState->checkpointCount = 0;

    // Player
    gameState->player = {
        .position = {200.0f, 50.0f},
        .velocity = {0, 0},
        .radius = 20.0f,
        .health = 5};

    gameState->enemies[MAX_ENEMIES];
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        gameState->enemies[i].type = ENEMY_NONE;
        gameState->enemies[i].health = 0;
        gameState->enemies[i].velocity = (Vector2){0, 0};
        gameState->enemies[i].radius = 0;
        gameState->enemies[i].speed = 0;
        gameState->enemies[i].direction = 1;
        gameState->enemies[i].shootTimer = 0;
        gameState->enemies[i].shootCooldown = 0;
        gameState->enemies[i].baseY = 0;
        gameState->enemies[i].waveOffset = 0;
        gameState->enemies[i].waveAmplitude = 0;
        gameState->enemies[i].waveSpeed = 0;
    }

    gameState->bossEnemy = {ENEMY_NONE};
    bool bossActive = false;
    float enemyShootRange = 300.0f;

    struct Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 10.0f;
    const float bulletRadius = 5.0f;

    bool gameWon = false;
    Vector2 checkpointPos = {0, 0};
    Rectangle checkpointRect = {0, 0, TILE_SIZE, TILE_SIZE * 2};

    int bossMeleeFlashTimer = 0;

    if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
    {
        for (int x = 0; x < MAP_COLS; x++)
            mapTiles[MAP_ROWS - 1][x] = 1;
    }

    if (!editorMode)
    {
        if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
        {
            TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
        }
    }

    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();
        Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

        // -------------------------------
        // EDITOR MODE
        // -------------------------------
        if (editorMode)
        {
            if (IsKeyPressed(KEY_S))
            {
                if (SaveLevel(gameState->currentLevelFilename, mapTiles, gameState->player, gameState->enemies, gameState->bossEnemy, checkpointPos))
                    TraceLog(LOG_INFO, "Level saved successfully!");
                else
                    TraceLog(LOG_ERROR, "Failed to save Level!");
            }

            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawEditor();

            // DrawRectangleRec(headerPanel, LIGHTGRAY);

            // if (DrawButton("Ground", btnGround, (currentTool == TOOL_GROUND) ? GREEN : WHITE, BLACK))
            // {
            //     currentTool = TOOL_GROUND;
            //     enemyPlacementType = -1;
            // }
            // if (DrawButton("Death", btnDeath, (currentTool == TOOL_DEATH) ? GREEN : WHITE, BLACK))
            // {
            //     currentTool = TOOL_DEATH;
            //     enemyPlacementType = -1;
            // }
            // if (DrawButton("Eraser", btnEraser, (currentTool == TOOL_ERASER) ? GREEN : WHITE, BLACK))
            // {
            //     currentTool = TOOL_ERASER;
            //     enemyPlacementType = -1;
            // }
            // if (DrawButton("Ground Enemy", btnAddGroundEnemy, (enemyPlacementType == ENEMY_GROUND) ? GREEN : WHITE, BLACK))
            // {
            //     enemyPlacementType = ENEMY_GROUND;
            // }
            // if (DrawButton("Flying Enemy", btnAddFlyingEnemy, (enemyPlacementType == ENEMY_FLYING) ? GREEN : WHITE, BLACK))
            // {
            //     enemyPlacementType = ENEMY_FLYING;
            // }
            // if (DrawButton("Place Boss", btnPlaceBoss, (enemyPlacementType == -2) ? GREEN : WHITE, BLACK))
            // {
            //     enemyPlacementType = -2; // special mode for boss placement
            //     bossSelected = false;
            // }
            // if (DrawButton("Play", playButton, BLUE, BLACK))
            // {
            //     // On Play, load checkpoint state for game mode.
            //     if (!LoadCheckpointState(CHECKPOINT_FILE, &player, enemies, &bossEnemy, checkpoints, &checkpointCount))
            //         TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
            //     editorMode = false;
            // }
            // // New buttons in the header (or wherever you like)
            // Rectangle newLevelButton = {SCREEN_WIDTH - 270, 10, 80, 30};
            // Rectangle openLevelButton = {SCREEN_WIDTH - 180, 10, 80, 30};
            // Rectangle saveLevelAsButton = {SCREEN_WIDTH - 90, 10, 80, 30};

            // // Draw the buttons and process clicks.
            // if (DrawButton("New", newLevelButton, LIGHTGRAY, BLACK))
            // {
            //     // Create a new blank/default tilemap.
            //     for (int y = 0; y < MAP_ROWS; y++)
            //     {
            //         for (int x = 0; x < MAP_COLS; x++)
            //         {
            //             mapTiles[y][x] = 0; // Clear all tiles.
            //         }
            //     }
            //     // (Optional) Add a ground row at the bottom:
            //     for (int x = 0; x < MAP_COLS; x++)
            //     {
            //         mapTiles[MAP_ROWS - 1][x] = 1;
            //     }

            //     // Clear any existing enemy and boss data.
            //     for (int i = 0; i < MAX_ENEMIES; i++)
            //     {
            //         enemies[i].type = ENEMY_NONE;
            //         enemies[i].health = 0;
            //     }
            //     bossEnemy.type = ENEMY_NONE;

            //     // Set the current filename to a new default.
            //     strcpy(currentLevelFilename, "new_level.txt");
            //     TraceLog(LOG_INFO, "New level started: %s", currentLevelFilename);
            // }

            // if (DrawButton("Open", openLevelButton, LIGHTGRAY, BLACK))
            // {
            //     const char *levelsDir = "levels";      // Adjust to your levels directory.
            //     const char *levelExtension = ".level"; // For example, files ending with .level.

            //     // Count how many files match.
            //     levelFileCount = CountFilesWithExtension(levelsDir, levelExtension);
            //     if (levelFileCount <= 0)
            //     {
            //         TraceLog(LOG_WARNING, "No level files found in %s", levelsDir);
            //     }
            //     else
            //     {
            //         // Allocate a contiguous block for fileCount paths.
            //         levelFiles = (char(*)[256])arena_alloc(&gameArena, levelFileCount * sizeof(*levelFiles));
            //         if (levelFiles == NULL)
            //         {
            //             TraceLog(LOG_ERROR, "Failed to allocate memory for level file list!");
            //         }
            //         else
            //         {
            //             int index = 0;
            //             FillFilesWithExtensionContiguous(levelsDir, levelExtension, levelFiles, &index, &gameArena);

            //             // Now, levelFiles[0..index-1] hold the file paths.
            //             // For example, draw each file path on screen:
            //             for (int i = 0; i < index; i++)
            //             {
            //                 TraceLog(LOG_INFO, levelFiles[i]);
            //                 DrawText(levelFiles[i], 50, 100 + i * 20, 10, BLACK);
            //             }
            //         }
            //     }

            //     showFileList = !showFileList;
            // }

            // if (DrawButton("Save As", saveLevelAsButton, LIGHTGRAY, BLACK))
            // {
            //     // Save the level to the currentLevelFilename.
            //     // (You may want to also allow the user to change currentLevelFilename by keyboard input, etc.)
            //     if (!SaveLevel(currentLevelFilename, mapTiles, player, enemies, bossEnemy, checkpointPos))
            //     {
            //         TraceLog(LOG_ERROR, "Failed to save level: %s", currentLevelFilename);
            //     }
            //     else
            //     {
            //         TraceLog(LOG_INFO, "Level saved as: %s", currentLevelFilename);
            //     }
            // }

            // // (Optionally, draw text somewhere showing the current level file name:)
            // DrawText(TextFormat("Level: %s", currentLevelFilename), SCREEN_WIDTH - 270, 45, 10, BLACK);

            // if (DrawButton("Create Checkpoint", checkpointButton, WHITE, BLACK))
            // {
            //     if (checkpointCount < MAX_CHECKPOINTS)
            //     {
            //         // Compute the world center of the camera:
            //         Vector2 worldCenter = camera.target;
            //         // Compute tile indices near the center:
            //         int tileX = (int)(worldCenter.x / TILE_SIZE);
            //         int tileY = (int)(worldCenter.y / TILE_SIZE);
            //         // Set checkpoint position to the top-left corner of that tile:
            //         checkpoints[checkpointCount].x = tileX * TILE_SIZE;
            //         checkpoints[checkpointCount].y = tileY * TILE_SIZE;
            //         checkpointCount++;
            //     }
            //     else
            //     {
            //         TraceLog(LOG_INFO, "Maximum number of checkpoints reached.");
            //     }
            // }

            // if (showFileList)
            // {
            //     // Define a window rectangle (adjust position/size as desired):
            //     int rowHeight = 20;
            //     int windowWidth = 400;
            //     int windowHeight = levelFileCount * rowHeight + 80;
            //     Rectangle fileListWindow = {200, 100, windowWidth, windowHeight};

            //     // Draw window background and border.
            //     DrawRectangleRec(fileListWindow, Fade(LIGHTGRAY, 0.9f));
            //     DrawRectangleLines(fileListWindow.x, fileListWindow.y, fileListWindow.width, fileListWindow.height, BLACK);
            //     DrawText("Select a Level File", fileListWindow.x + 10, fileListWindow.y + 10, rowHeight, BLACK);

            //     // Draw each file name in a row.
            //     for (int i = 0; i < levelFileCount; i++)
            //     {
            //         Rectangle fileRow = {fileListWindow.x + 10, fileListWindow.y + 40 + i * rowHeight, windowWidth - 20, rowHeight};

            //         // Check if the mouse is over this file's rectangle.
            //         if (CheckCollisionPointRec(GetMousePosition(), fileRow))
            //         {
            //             // Draw a highlight behind the text.
            //             DrawRectangleRec(fileRow, Fade(GREEN, 0.3f));
            //             if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            //             {
            //                 selectedFileIndex = i;
            //             }
            //         }

            //         // If this file is selected, draw a border.
            //         if (i == selectedFileIndex)
            //         {
            //             DrawRectangleLines(fileRow.x, fileRow.y, fileRow.width, fileRow.height, RED);
            //         }

            //         // Draw the file name text.
            //         DrawText(levelFiles[i], fileRow.x + 5, fileRow.y + 2, 15, BLACK);
            //     }
            //     // Define Load and Cancel buttons within the window.
            //     Rectangle loadButton = {fileListWindow.x + 10, fileListWindow.y + windowHeight - 30, 100, rowHeight};
            //     Rectangle cancelButton = {fileListWindow.x + 120, fileListWindow.y + windowHeight - 30, 100, rowHeight};

            //     if (DrawButton("Load", loadButton, LIGHTGRAY, BLACK))
            //     {
            //         if (selectedFileIndex >= 0 && selectedFileIndex < levelFileCount)
            //         {
            //             // Copy the selected file path into your currentLevelFilename buffer.
            //             strcpy(currentLevelFilename, levelFiles[selectedFileIndex]);
            //             // Optionally, call your LoadLevel function to load the file.
            //             if (!LoadLevel(currentLevelFilename, mapTiles, &player, enemies, &bossEnemy, checkpoints, &checkpointCount))
            //             {
            //                 TraceLog(LOG_ERROR, "Failed to load level: %s", currentLevelFilename);
            //             }
            //             else
            //             {
            //                 TraceLog(LOG_INFO, "Loaded level: %s", currentLevelFilename);
            //             }
            //             // Hide the file selection window.
            //             showFileList = false;
            //         }
            //     }
            //     if (DrawButton("Cancel", cancelButton, LIGHTGRAY, BLACK))
            //     {
            //         showFileList = false;
            //     }
            // }

            // DrawRectangleRec(enemyInspectorPanel, LIGHTGRAY);
            // DrawText("Enemy Inspector", enemyInspectorPanel.x + 5, enemyInspectorPanel.y + 5, 10, BLACK);

            // // If an enemy is selected, show its data and provide editing buttons.
            // if (selectedEnemyIndex != -1)
            // {
            //     // Buttons to adjust health and toggle type.
            //     Rectangle btnHealthUp = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 75, 40, 20};
            //     Rectangle btnHealthDown = {enemyInspectorPanel.x + 150, enemyInspectorPanel.y + 75, 40, 20};
            //     Rectangle btnToggleType = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 50, 90, 20};
            //     Rectangle btnDeleteEnemy = {enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 90, 20};

            //     char info[128];
            //     sprintf(info, "Type: %s", (enemies[selectedEnemyIndex].type == ENEMY_GROUND) ? "Ground" : "Flying");
            //     DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);

            //     if (DrawButton("Toggle Type", btnToggleType, WHITE, BLACK, 10))
            //     {
            //         enemies[selectedEnemyIndex].type =
            //             (enemies[selectedEnemyIndex].type == ENEMY_GROUND) ? ENEMY_FLYING : ENEMY_GROUND;
            //     }

            //     sprintf(info, "Health: %d", enemies[selectedEnemyIndex].health);
            //     DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 75, 10, BLACK);

            //     if (DrawButton("+", btnHealthUp, WHITE, BLACK, 10))
            //     {
            //         enemies[selectedEnemyIndex].health++;
            //     }
            //     if (DrawButton("-", btnHealthDown, WHITE, BLACK, 10))
            //     {
            //         if (enemies[selectedEnemyIndex].health > 0)
            //             enemies[selectedEnemyIndex].health--;
            //     }

            //     sprintf(info, "Pos: %.0f, %.0f", enemies[selectedEnemyIndex].position.x, enemies[selectedEnemyIndex].position.y);
            //     DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 10, BLACK);

            //     if (DrawButton("Delete", btnDeleteEnemy, RED, WHITE, 10))
            //     {
            //         enemies[selectedEnemyIndex].health = 0;
            //         selectedEnemyIndex = -1;
            //     }
            // }
            // else if (bossSelected)
            // {
            //     Rectangle btnHealthUp = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 75, 40, 20};
            //     Rectangle btnHealthDown = {enemyInspectorPanel.x + 150, enemyInspectorPanel.y + 75, 40, 20};
            //     // For the boss, we display its health and position.
            //     char info[128];
            //     sprintf(info, "Boss HP: %d", bossEnemy.health);
            //     DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
            //     if (DrawButton("+", btnHealthUp, WHITE, BLACK, 10))
            //     {
            //         bossEnemy.health++;
            //     }
            //     if (DrawButton("-", btnHealthDown, WHITE, BLACK, 10))
            //     {
            //         if (bossEnemy.health > 0)
            //             bossEnemy.health--;
            //     }
            //     sprintf(info, "Pos: %.0f, %.0f", bossEnemy.position.x, bossEnemy.position.y);
            //     DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 10, BLACK);
            // }
            // else if (enemyPlacementType != -1)
            // {
            //     // If no enemy is selected but a placement tool is active, show a placement message.
            //     if (enemyPlacementType == ENEMY_GROUND)
            //         DrawText("Placement mode: Ground", enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
            //     else if (enemyPlacementType == ENEMY_FLYING)
            //         DrawText("Placement mode: Flying", enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
            // }

            BeginMode2D(camera);
            DrawTilemap(camera);
            // In BeginMode2D(camera) within editor mode:
            for (int i = 0; i < gameState->checkpointCount; i++)
            {
                DrawRectangle(gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2, Fade(GREEN, 0.3f));
            }

            // Draw enemies
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (gameState->enemies[i].type == ENEMY_GROUND)
                {
                    float halfSide = gameState->enemies[i].radius;
                    DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                  (int)(gameState->enemies[i].position.y - halfSide),
                                  (int)(gameState->enemies[i].radius * 2),
                                  (int)(gameState->enemies[i].radius * 2), RED);
                }
                else if (gameState->enemies[i].type == ENEMY_FLYING)
                {
                    DrawPoly(gameState->enemies[i].position, 4, gameState->enemies[i].radius, 45.0f, ORANGE);
                }
                // Draw bound markers:
                DrawLine((int)gameState->enemies[i].leftBound, 0, (int)gameState->enemies[i].leftBound, 20, BLUE);
                DrawLine((int)gameState->enemies[i].rightBound, 0, (int)gameState->enemies[i].rightBound, 20, BLUE);
            }
            // Draw the boss in the editor if placed.
            if (gameState->bossEnemy.type != ENEMY_NONE)
            {
                DrawCircleV(gameState->bossEnemy.position, gameState->bossEnemy.radius, PURPLE);
            }

            DrawCircleV(gameState->player.position, gameState->player.radius, BLUE);
            DrawText("PLAYER", gameState->player.position.x - 20, gameState->player.position.y - gameState->player.radius - 20, 12, BLACK);

            EndMode2D();
            EndDrawing();
            continue;
        }

        // -------------------------------
        // GAME MODE
        // -------------------------------
        camera.target = gameState->player.position;
        camera.rotation = 0.0f;
        camera.zoom = 0.66f;

        // Update the music stream each frame.
        if (!IsMusicStreamPlaying(*currentTrack) && gameState->player.health > 0)
            PlayMusicStream(*currentTrack);
        UpdateMusicStream(*currentTrack);

        if (gameState->player.health > 0)
        {
            gameState->player.velocity.x = 0;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
                gameState->player.velocity.x = -4.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
                gameState->player.velocity.x = 4.0f;

            if (IsKeyPressed(KEY_SPACE))
            {
                if (fabsf(gameState->player.velocity.y) < 0.001f)
                    gameState->player.velocity.y = -8.0f;
            }

            gameState->player.velocity.y += 0.4f;
            gameState->player.position.x += gameState->player.velocity.x;
            gameState->player.position.y += gameState->player.velocity.y;
            ResolveCircleTileCollisions(&gameState->player.position, &gameState->player.velocity, &gameState->player.health, gameState->player.radius);
        }

        // Loop through all checkpoints.
        for (int i = 0; i < gameState->checkpointCount; i++)
        {
            // Define the checkpoint rectangle (using the same dimensions you use elsewhere).
            Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2};

            // Only trigger if the player collides with this checkpoint AND it hasn't been activated yet.
            if (!checkpointActivated[i] && CheckCollisionPointRec(gameState->player.position, cpRect))
            {
                // Save the checkpoint state (this saves player, enemies, boss, and the checkpoint array).
                if (SaveCheckpointState(CHECKPOINT_FILE, gameState->player, gameState->enemies, gameState->bossEnemy, gameState->checkpoints, gameState->checkpointCount, i))
                {
                    TraceLog(LOG_INFO, "Checkpoint saved!");
                }
                // Mark this checkpoint as activated so that it does not update again
                checkpointActivated[i] = true;
                // Break if you want to update only one checkpoint per frame.
                break;
            }
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (!bullets[i].active)
                {
                    bullets[i].active = true;
                    bullets[i].fromPlayer = true;
                    bullets[i].position = gameState->player.position;

                    Vector2 dir = {screenPos.x - gameState->player.position.x, screenPos.y - gameState->player.position.y};
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

            PlaySound(shotSound);
        }

        // (Enemy update code remains the same as your original game logic)
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (gameState->enemies[i].health > 0)
            {
                if (gameState->enemies[i].type == ENEMY_GROUND)
                {
                    gameState->enemies[i].velocity.x = gameState->enemies[i].direction * gameState->enemies[i].speed;
                    gameState->enemies[i].velocity.y += 0.4f;
                    gameState->enemies[i].position.x += gameState->enemies[i].velocity.x;
                    gameState->enemies[i].position.y += gameState->enemies[i].velocity.y;
                    ResolveCircleTileCollisions(&gameState->enemies[i].position, &gameState->enemies[i].velocity, &gameState->enemies[i].health, gameState->enemies[i].radius);
                    if (gameState->enemies[i].leftBound != 0 || gameState->enemies[i].rightBound != 0)
                    {
                        if (gameState->enemies[i].position.x < gameState->enemies[i].leftBound)
                        {
                            gameState->enemies[i].position.x = gameState->enemies[i].leftBound;
                            gameState->enemies[i].direction = 1;
                        }
                        else if (gameState->enemies[i].position.x > gameState->enemies[i].rightBound)
                        {
                            gameState->enemies[i].position.x = gameState->enemies[i].rightBound;
                            gameState->enemies[i].direction = -1;
                        }
                    }
                }
                else if (gameState->enemies[i].type == ENEMY_FLYING)
                {
                    if (gameState->enemies[i].leftBound != 0 || gameState->enemies[i].rightBound != 0)
                    {
                        gameState->enemies[i].position.x += gameState->enemies[i].direction * gameState->enemies[i].speed;
                        if (gameState->enemies[i].position.x < gameState->enemies[i].leftBound)
                        {
                            gameState->enemies[i].position.x = gameState->enemies[i].leftBound;
                            gameState->enemies[i].direction = 1;
                        }
                        else if (gameState->enemies[i].position.x > gameState->enemies[i].rightBound)
                        {
                            gameState->enemies[i].position.x = gameState->enemies[i].rightBound;
                            gameState->enemies[i].direction = -1;
                        }
                    }
                    gameState->enemies[i].waveOffset += gameState->enemies[i].waveSpeed;
                    gameState->enemies[i].position.y = gameState->enemies[i].baseY + sinf(gameState->enemies[i].waveOffset) * gameState->enemies[i].waveAmplitude;
                }

                gameState->enemies[i].shootTimer += 1.0f;
                float dx = gameState->player.position.x - gameState->enemies[i].position.x;
                float dy = gameState->player.position.y - gameState->enemies[i].position.y;
                float distSqr = dx * dx + dy * dy;
                if (gameState->player.health > 0 && distSqr < (enemyShootRange * enemyShootRange))
                {
                    if (gameState->enemies[i].shootTimer >= gameState->enemies[i].shootCooldown)
                    {
                        for (int b = 0; b < MAX_BULLETS; b++)
                        {
                            if (!bullets[b].active)
                            {
                                bullets[b].active = true;
                                bullets[b].fromPlayer = false;
                                bullets[b].position = gameState->enemies[i].position;
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
                        PlaySound(shotSound);
                        gameState->enemies[i].shootTimer = 0.0f;
                    }
                }
            }
        }

        // --- Boss Enemy Spawn and Update ---
        // If all regular enemies are dead and the boss is not active, spawn the boss.
        bool anyEnemiesAlive = false;
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (gameState->enemies[i].health > 0)
            {
                anyEnemiesAlive = true;
                break;
            }
        }
        if (!anyEnemiesAlive && !bossActive)
        {
            bossActive = (gameState->bossEnemy.health >= 0);
            gameState->bossEnemy.shootTimer = 0; // reset attack timer on spawn
        }
        if (bossActive)
        {
            // Increment the boss's attack timer every frame.
            gameState->bossEnemy.shootTimer += 1.0f;

            // Phase 1: Ground (Melee Attack) if boss health is at least 50%
            if (gameState->bossEnemy.health >= (BOSS_MAX_HEALTH * 0.5f))
            {
                gameState->bossEnemy.type = ENEMY_GROUND;
                gameState->bossEnemy.speed = 2.0f;
                gameState->bossEnemy.velocity.x = gameState->bossEnemy.direction * gameState->bossEnemy.speed;
                gameState->bossEnemy.velocity.y += 0.4f;
                gameState->bossEnemy.position.x += gameState->bossEnemy.velocity.x;
                gameState->bossEnemy.position.y += gameState->bossEnemy.velocity.y;
                ResolveCircleTileCollisions(&gameState->bossEnemy.position, &gameState->bossEnemy.velocity, &gameState->bossEnemy.health, gameState->bossEnemy.radius);

                // Melee attack: if the player is close enough...
                float dx = gameState->player.position.x - gameState->bossEnemy.position.x;
                float dy = gameState->player.position.y - gameState->bossEnemy.position.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < gameState->bossEnemy.radius + gameState->player.radius + 10.0f)
                {
                    if (gameState->bossEnemy.shootTimer >= gameState->bossEnemy.shootCooldown)
                    {
                        gameState->player.health -= 1; // Damage the player.
                        gameState->bossEnemy.shootTimer = 0;
                        bossMeleeFlashTimer = 10; // Trigger melee flash effect.
                    }
                }
            }
            // Phase 2: Flying with single projectile if boss health is below 50% but at least 20%
            else if (gameState->bossEnemy.health >= (BOSS_MAX_HEALTH * 0.2f))
            {
                gameState->bossEnemy.type = ENEMY_FLYING;
                gameState->bossEnemy.speed = 4.0f;
                gameState->bossEnemy.waveOffset += gameState->bossEnemy.waveSpeed;
                gameState->bossEnemy.position.y = gameState->bossEnemy.baseY + sinf(gameState->bossEnemy.waveOffset) * gameState->bossEnemy.waveAmplitude;
                gameState->bossEnemy.position.x += gameState->bossEnemy.direction * gameState->bossEnemy.speed;

                if (gameState->bossEnemy.shootTimer >= gameState->bossEnemy.shootCooldown)
                {
                    gameState->bossEnemy.shootTimer = 0;

                    // Compute a normalized vector from boss to player.
                    float dx = gameState->player.position.x - gameState->bossEnemy.position.x;
                    float dy = gameState->player.position.y - gameState->bossEnemy.position.y;
                    float len = sqrtf(dx * dx + dy * dy);
                    Vector2 dir = {0, 0};
                    if (len > 0.0f)
                    {
                        dir.x = dx / len;
                        dir.y = dy / len;
                    }

                    // Spawn one projectile toward the player.
                    for (int b = 0; b < MAX_BULLETS; b++)
                    {
                        if (!bullets[b].active)
                        {
                            bullets[b].active = true;
                            bullets[b].fromPlayer = false;
                            bullets[b].position = gameState->bossEnemy.position;
                            bullets[b].velocity.x = dir.x * bulletSpeed;
                            bullets[b].velocity.y = dir.y * bulletSpeed;
                            break;
                        }
                    }
                }
            }
            // Phase 3: Flying with fan projectile attack if boss health is below 20%
            else
            {
                gameState->bossEnemy.type = ENEMY_FLYING;
                gameState->bossEnemy.speed = 4.0f;
                gameState->bossEnemy.waveOffset += gameState->bossEnemy.waveSpeed;
                gameState->bossEnemy.position.y = gameState->bossEnemy.baseY + sinf(gameState->bossEnemy.waveOffset) * gameState->bossEnemy.waveAmplitude;
                gameState->bossEnemy.position.x += gameState->bossEnemy.direction * gameState->bossEnemy.speed;

                if (gameState->bossEnemy.shootTimer >= gameState->bossEnemy.shootCooldown)
                {
                    gameState->bossEnemy.shootTimer = 0;

                    // Compute the central angle from the boss to the player.
                    float dx = gameState->player.position.x - gameState->bossEnemy.position.x;
                    float dy = gameState->player.position.y - gameState->bossEnemy.position.y;
                    float centerAngle = atan2f(dy, dx);
                    // Define the fan: total spread of 60° (i.e. 30° to each side).
                    float fanSpread = 30.0f * DEG2RAD;
                    int numProjectiles = 5;
                    // The spacing (offset) for each projectile.
                    float spacing = fanSpread / 2.0f; // 15° in radians (~0.2618)

                    // Spawn five projectiles with angles offset from the center.
                    for (int i = -2; i <= 2; i++)
                    {
                        float angle = centerAngle + i * spacing;
                        Vector2 projDir = {cosf(angle), sinf(angle)};

                        for (int b = 0; b < MAX_BULLETS; b++)
                        {
                            if (!bullets[b].active)
                            {
                                bullets[b].active = true;
                                bullets[b].fromPlayer = false;
                                bullets[b].position = gameState->bossEnemy.position;
                                bullets[b].velocity.x = projDir.x * bulletSpeed;
                                bullets[b].velocity.y = projDir.y * bulletSpeed;
                                break; // Move on to spawn the next projectile.
                            }
                        }
                    }
                }
            }

            // Common horizontal boundary checking for the boss.
            if (gameState->bossEnemy.position.x < gameState->bossEnemy.leftBound)
            {
                gameState->bossEnemy.position.x = gameState->bossEnemy.leftBound;
                gameState->bossEnemy.direction = 1;
            }
            else if (gameState->bossEnemy.position.x > gameState->bossEnemy.rightBound)
            {
                gameState->bossEnemy.position.x = gameState->bossEnemy.rightBound;
                gameState->bossEnemy.direction = -1;
            }
        }
        UpdateBullets(bullets, MAX_BULLETS, LEVEL_WIDTH, LEVEL_HEIGHT);
        CheckBulletCollisions(bullets, MAX_BULLETS, &gameState->player, gameState->enemies, MAX_ENEMIES, &gameState->bossEnemy, &bossActive, &gameWon, bulletRadius);

        BeginDrawing();
        if (!gameWon)
        {
            ClearBackground(RAYWHITE);
            DrawText("GAME MODE: Tilemap collisions active. (ESC to exit)", 10, 10, 20, DARKGRAY);
            DrawText(TextFormat("Player Health: %d", gameState->player.health), 600, 10, 20, MAROON);

            BeginMode2D(camera);
            DrawTilemap(camera);
            if (gameState->player.health > 0)
            {
                DrawCircleV(gameState->player.position, gameState->player.radius, RED);
                DrawLine((int)gameState->player.position.x, (int)gameState->player.position.y,
                         (int)screenPos.x, (int)screenPos.y,
                         GRAY);
            }
            else
            {
                if (IsMusicStreamPlaying(music))
                {
                    PauseMusicStream(music);
                    PlaySound(defeatMusic);
                }

                DrawCircleV(gameState->player.position, gameState->player.radius, DARKGRAY);
                DrawText("YOU DIED!", SCREEN_WIDTH / 2 - 50, 30, 30, RED);
                DrawText("Press SPACE for New Game", SCREEN_WIDTH / 2 - 100, 80, 20, DARKGRAY);
                if (checkpointActivated[0])
                {
                    DrawText("Press R to Respawn at Checkpoint", SCREEN_WIDTH / 2 - 130, 110, 20, DARKGRAY);
                    if (IsKeyPressed(KEY_R))
                    {
                        if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
                            TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                        else
                            TraceLog(LOG_INFO, "Checkpoint reloaded!");

                        for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                            bullets[i].active = false;
                        for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                            bullets[i].active = false;

                        gameState->player.health = 5;
                        gameState->player.velocity = (Vector2){0, 0};
                        for (int i = 0; i < MAX_ENEMIES; i++)
                            gameState->enemies[i].velocity = (Vector2){0, 0};

                        camera.target = gameState->player.position;
                        bossActive = false;
                        ResumeMusicStream(music);
                    }
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
                        checkpointPos = (Vector2){0, 0};
                        if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
                            TraceLog(LOG_ERROR, "Failed to load level default state!");

                        gameState->player.health = 5;
                        for (int i = 0; i < MAX_BULLETS; i++)
                            bullets[i].active = false;
                        gameState->player.velocity = (Vector2){0, 0};
                        for (int i = 0; i < MAX_ENEMIES; i++)
                            gameState->enemies[i].velocity = (Vector2){0, 0};
                        camera.target = gameState->player.position;
                        bossActive = false;
                        ResumeMusicStream(music);
                    }
                }
            }

            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (gameState->enemies[i].health > 0)
                {
                    if (gameState->enemies[i].type == ENEMY_GROUND)
                    {
                        float halfSide = gameState->enemies[i].radius;
                        DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                      (int)(gameState->enemies[i].position.y - halfSide),
                                      (int)(gameState->enemies[i].radius * 2),
                                      (int)(gameState->enemies[i].radius * 2), GREEN);
                    }
                    else if (gameState->enemies[i].type == ENEMY_FLYING)
                    {
                        DrawPoly(gameState->enemies[i].position, 4, gameState->enemies[i].radius, 45.0f, ORANGE);
                    }
                }
            }

            // Draw boss enemy if active.
            if (bossActive)
            {
                DrawCircleV(gameState->bossEnemy.position, gameState->bossEnemy.radius, PURPLE);
                DrawText(TextFormat("Boss HP: %d", gameState->bossEnemy.health), gameState->bossEnemy.position.x - 30, gameState->bossEnemy.position.y - gameState->bossEnemy.radius - 20, 10, RED);

                // If the melee flash timer is active, draw a red outline.
                if (bossMeleeFlashTimer > 0)
                {
                    // Draw a red circle slightly larger than the boss to indicate a melee hit.
                    DrawCircleLines(gameState->bossEnemy.position.x, gameState->bossEnemy.position.y, gameState->bossEnemy.radius + 5, RED);
                    // Decrement the timer so the effect fades over time.
                    bossMeleeFlashTimer--;
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
        }
        else
        {
            if (IsMusicStreamPlaying(music))
            {
                StopMusicStream(music);
                PlayMusicStream(victoryMusic);
                currentTrack = &victoryMusic;
            }

            ClearBackground(BLACK);
            // Update and draw fireworks
            for (int i = 0; i < MAX_PARTICLES; i++)
            {
                particles[i].position.x += particles[i].velocity.x;
                particles[i].position.y += particles[i].velocity.y;
                particles[i].life -= 1.0f;
                if (particles[i].life <= 0)
                    InitParticle(&particles[i]);
                particles[i].color.a = (unsigned char)(255 * (particles[i].life / 180.0f));
                DrawCircleV(particles[i].position, 2, particles[i].color);
            }
            int textWidth = MeasureText("YOU WON", 60);
            DrawText("YOU WON", SCREEN_WIDTH / 2 - textWidth / 2, SCREEN_HEIGHT / 2 - 30, 60, YELLOW);
        }

        EndMode2D();

#ifdef EDITOR_BUILD
        // In an editor build, draw a Stop button in game mode so you can return to editor mode.
        Rectangle stopButton = {SCREEN_WIDTH - 90, 10, 80, 30};
        if (DrawButton("Stop", stopButton, LIGHTGRAY, BLACK, 20))
        {
            if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
                TraceLog(LOG_ERROR, "Failed to reload level for editor mode!");
            editorMode = true;
            StopMusicStream(music);
            StopSound(shotSound);
        }
#endif

        EndDrawing();
    }

    UnloadMusicStream(music);
    UnloadSound(shotSound);
    CloseAudioDevice();

    arena_destroy(&gameState->gameArena);
    CloseWindow();
    return 0;
}
