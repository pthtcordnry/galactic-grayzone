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

#include "main.h"
#include <raylib.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "memory_arena.h"
#include "file_io.h"
#include "editor_mode.h"
#include "game_state.h"
#include <rlImGui.h>

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

bool SaveEntityAssetToJson(const char *filename, const Entity *asset, bool allowOverwrite)
{
    // If overwriting is not allowed, check if the file already exists.
    if (!allowOverwrite)
    {
        FILE *checkFile = fopen(filename, "r");
        if (checkFile != NULL)
        {
            TraceLog(LOG_ERROR, "File %s already exists with no overwrite!", filename);
            fclose(checkFile);
            return false; // File exists; do not overwrite.
        }
    }

    // Extract the directory portion from filename.
    char directory[256] = {0};
    ExtractDirectoryFromFilename(filename, directory, sizeof(directory));
    if (directory[0] != '\0')
    {
        if (!EnsureDirectoryExists(directory))
        {
            TraceLog(LOG_ERROR, "Directory %s doesn't exist!", directory);
            return false;
        }
    }

    FILE *file = fopen(filename, "w");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open %s for writing!", filename);
        return false;
    }

    // Write asset data in JSON format.
    fprintf(file,
            "{\n"
            "    \"name\": \"%s\",\n"
            "    \"kind\": \"%s\",\n"
            "    \"enemyType\": %d,\n"
            "    \"health\": %d,\n"
            "    \"speed\": %.2f,\n"
            "    \"radius\": %.2f,\n"
            "    \"shootCooldown\": %.2f,\n"
            "    \"leftBound\": %d,\n"
            "    \"rightBound\": %d,\n"
            "    \"baseY\": %.2f,\n"
            "    \"waveAmplitude\": %.2f,\n"
            "    \"waveSpeed\": %.2f\n"
            "}\n",
            asset->name,
            GetEntityKindString(asset->kind),
            asset->physicsType,
            asset->health,
            asset->speed,
            asset->radius,
            asset->shootCooldown,
            asset->leftBound,
            asset->rightBound,
            asset->baseY,
            asset->waveAmplitude,
            asset->waveSpeed);

    fclose(file);
    return true;
}

bool SaveAllEntityAssets(const char *directory, Entity *assets, int count, bool allowOverwrite)
{
    bool allSaved = true; // Flag to track if every asset saved successfully.

    for (int i = 0; i < count; i++)
    {
        char filename[256];
        // Create a filename based on the asset's name.
        // (You might want to sanitize asset->name if it contains spaces or invalid characters.)
        snprintf(filename, sizeof(filename), "%s/%s.json", directory, assets[i].name);

        if (!SaveEntityAssetToJson(filename, &assets[i], allowOverwrite))
        {
            TraceLog(LOG_ERROR, "Failed to save entity: %s", filename);
            allSaved = false; // Mark that at least one asset failed.
            // Continue processing the remaining entities.
        }
    }
    return allSaved;
}

// Loads a single asset from a JSON file.
bool LoadEntityAssetFromJson(const char *filename, Entity *asset)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return false;

    // Read the entire file into a buffer.
    char buffer[1024];
    size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[size] = '\0';
    fclose(file);

    char kindStr[32] = {0};
    int ret = sscanf(buffer,
                     " { \"name\" : \"%63[^\"]\" , \"kind\" : \"%31[^\"]\" , \"enemyType\" : %d , \"health\" : %d , \"speed\" : %f , \"radius\" : %f , \"shootCooldown\" : %f , \"leftBound\" : %d , \"rightBound\" : %d , \"baseY\" : %f , \"waveAmplitude\" : %f , \"waveSpeed\" : %f }",
                     asset->name,
                     kindStr,
                     &asset->physicsType,
                     &asset->health,
                     &asset->speed,
                     &asset->radius,
                     &asset->shootCooldown,
                     &asset->leftBound,
                     &asset->rightBound,
                     &asset->baseY,
                     &asset->waveAmplitude,
                     &asset->waveSpeed);
    if (ret < 12)
        return false;

    asset->kind = GetEntityKindFromString(kindStr);
    return true;
}

// Loads all entity asset JSON files from the specified directory.
bool LoadEntityAssets(const char *directory, Entity *assets, int *count)
{
    char fileList[256][256];
    int numFiles = ListFilesInDirectory(directory, "*.json", fileList, 256);
    int assetCount = 0;

    for (int i = 0; i < numFiles; i++)
    {
        if (LoadEntityAssetFromJson(fileList[i], &assets[assetCount]))
        {
            assetCount++;
        }
        // Optionally, you can handle errors or log files that failed to load.
    }
    *count = assetCount;
    return true;
}

bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Entity *player, Entity *enemies, Entity *bossEnemy)
{
    // Build full path by prepending the levels directory.
    char fullPath[256];
    // Use "./levels/" as the directory; adjust if needed.
    snprintf(fullPath, sizeof(fullPath), "./levels/%s", filename);

    if (!EnsureDirectoryExists("./levels/"))
    {
        TraceLog(LOG_ERROR, "Failed to secure directory for saving file: %s", fullPath);
    }

    FILE *file = fopen(fullPath, "w");
    if (file == NULL)
    {
        TraceLog(LOG_ERROR, "Failed to open file for saving: %s", fullPath);
        return false;
    }

    // Write map dimensions.
    fprintf(file, "%d %d\n", MAP_ROWS, MAP_COLS);

    // Save tile map.
    for (int y = 0; y < MAP_ROWS; y++)
    {
        for (int x = 0; x < MAP_COLS; x++)
            fprintf(file, "%d ", mapTiles[y][x]);
        fprintf(file, "\n");
    }

    // Save player data.
    if (player != NULL)
    {
        fprintf(file, "PLAYER %.2f %.2f %.2f\n",
                player->position.x, player->position.y, player->radius);
    }
    else
    {
        // In case player is NULL, write a default (or log an error).
        fprintf(file, "PLAYER 0 0 0\n");
    }

    // Save enemy data.
    fprintf(file, "ENEMY_COUNT %d\n", gameState->enemyCount);
    if (gameState->enemyCount > 0 && enemies != NULL)
    {
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            fprintf(file, "ENEMY %d %.2f %.2f %.2f %.2f %d %.2f %.2f\n",
                    enemies[i].physicsType,
                    enemies[i].position.x, enemies[i].position.y,
                    enemies[i].leftBound, enemies[i].rightBound,
                    enemies[i].health, enemies[i].speed, enemies[i].shootCooldown);
        }
    }

    // Save boss data only if a boss has been placed.
    if (bossEnemy != NULL)
    {
        fprintf(file, "BOSS %.2f %.2f %.2f %.2f %d %.2f %.2f\n",
                bossEnemy->position.x, bossEnemy->position.y,
                bossEnemy->leftBound, bossEnemy->rightBound,
                bossEnemy->health, bossEnemy->speed, bossEnemy->shootCooldown);
    }

    // Save checkpoints.
    fprintf(file, "CHECKPOINT_COUNT %d\n", gameState->checkpointCount);
    for (int i = 0; i < gameState->checkpointCount; i++)
    {
        fprintf(file, "CHECKPOINT %.2f %.2f\n",
                gameState->checkpoints[i].x, gameState->checkpoints[i].y);
    }

    fclose(file);
    return true;
}

bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               struct Entity **player, struct Entity **enemies, int *enemyCount, struct Entity **bossEnemy,
               Vector2 **checkpoints, int *checkpointCount)
{

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "./levels/%s", filename);

    FILE *file = fopen(fullPath, "r");
    if (file == NULL)
    {
        TraceLog(LOG_ERROR, "Failed to open file {%s} for reading!", filename);
        return false;
    }

    int rows, cols;
    if (fscanf(file, "%d %d", &rows, &cols) != 2)
    {
        TraceLog(LOG_INFO, "Failed to find tilemap initial data, skipping.");
    }
    else
    {
        for (int y = 0; y < MAP_ROWS; y++)
        {
            for (int x = 0; x < MAP_COLS; x++)
            {
                if (fscanf(file, "%d", &mapTiles[y][x]) != 1)
                {
                    TraceLog(LOG_ERROR, "Failed to load mapTile data!");
                    fclose(file);
                    return false;
                }
            }
        }
    }

    char token[32];
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "PLAYER") != 0)
    {
        arena_free(&gameState->gameArena, *player);
        *player = NULL;
        TraceLog(LOG_INFO, "Failed to find player data, skipping.");
    }
    else
    {
        if (*player == NULL)
        {
            *player = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            if (*player == NULL)
            {
                TraceLog(LOG_ERROR, "Failed to allocate memory for Player data!");
                fclose(file);
                return false;
            }
        }

        if (fscanf(file, "%f %f %f", &(*player)->position.x, &(*player)->position.y, &(*player)->radius) != 3)
        {
            TraceLog(LOG_ERROR, "Failed to load Player data!");
            fclose(file);
            return false;
        }
    }

    if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY_COUNT") != 0)
    {
        TraceLog(LOG_INFO, "Failed to find enemy count, skipping.");
    }
    else
    {
        int oldEnemyCount = *enemyCount;
        if (fscanf(file, "%d", enemyCount) != 1)
        {
            if (*enemies != NULL)
            {
                arena_free(&gameState->gameArena, *enemies);
                *enemies = NULL;
            }

            TraceLog(LOG_ERROR, "Failed to load enemy count!");
            fclose(file);
            return false;
        }

        if (*enemyCount > 0)
        {
            if (*enemies == NULL)
            {
                TraceLog(LOG_INFO, "Allocating Enemy memory.");
                *enemies = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity) * *enemyCount);
                if (*enemies == NULL)
                {
                    TraceLog(LOG_ERROR, "Failed to allocate memory for enemy data!");
                    fclose(file);
                    return false;
                }
            }

            if (oldEnemyCount != *enemyCount)
            {
                *enemies = (Entity *)arena_realloc(&gameState->gameArena, *enemies, sizeof(Entity) * *enemyCount);
            }

            for (int i = 0; i < *enemyCount; i++)
            {
                int type;
                if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
                {
                    TraceLog(LOG_ERROR, "Failed to find enemy array data!");
                    fclose(file);
                    return false;
                }
                if (fscanf(file, "%d %f %f %f %f %d %f %f", &type,
                           &(*enemies)[i].position.x, &(*enemies)[i].position.y,
                           &(*enemies)[i].leftBound, &(*enemies)[i].rightBound,
                           &(*enemies)[i].health, &(*enemies)[i].speed, &(*enemies)[i].shootCooldown) != 8)
                {
                    TraceLog(LOG_ERROR, "Failed to load enemy [%i] data!", i);
                    fclose(file);
                    return false;
                }

                (*enemies)[i].physicsType = (enum PhysicsType)type;
                (*enemies)[i].radius = 20;
                (*enemies)[i].direction = -1;
                (*enemies)[i].shootTimer = 0;
                (*enemies)[i].baseY = (*enemies)[i].position.y;
                (*enemies)[i].waveOffset = 0;
                (*enemies)[i].waveAmplitude = 0;
                (*enemies)[i].waveSpeed = 0;
                (*enemies)[i].velocity = (Vector2){0, 0};
            }
        }
    }

    if (fscanf(file, "%s", token) != 1 || strcmp(token, "BOSS") != 0)
    {
        arena_free(&gameState->gameArena, *bossEnemy);
        *bossEnemy = NULL;
        TraceLog(LOG_INFO, "Failed to find boss enemy data, skipping.");
    }
    else
    {
        if (*bossEnemy == NULL)
        {
            *bossEnemy = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            if (*bossEnemy == NULL)
            {
                TraceLog(LOG_ERROR, "Failed to allocate memory for boss data!");
                fclose(file);
                return false;
            }
        }

        if (fscanf(file, "%f %f %f %f %d %f %f",
                   &(*bossEnemy)->position.x, &(*bossEnemy)->position.y,
                   &(*bossEnemy)->leftBound, &(*bossEnemy)->rightBound,
                   &(*bossEnemy)->health, &(*bossEnemy)->speed, &(*bossEnemy)->shootCooldown) != 7)
        {
            TraceLog(LOG_ERROR, "Failed to load boss data!");
            fclose(file);
            return false;
        }
        else
        {
            // Successfully read boss data – initialize additional boss values.
            (*bossEnemy)->physicsType = FLYING;
            (*bossEnemy)->baseY = (*bossEnemy)->position.y - 200;
            (*bossEnemy)->radius = 40.0f;
            (*bossEnemy)->health = BOSS_MAX_HEALTH;
            (*bossEnemy)->speed = 2.0f; // initial phase 1 speed
            (*bossEnemy)->leftBound = 100;
            (*bossEnemy)->rightBound = LEVEL_WIDTH - 100;
            (*bossEnemy)->direction = 1;
            (*bossEnemy)->shootTimer = 0;
            (*bossEnemy)->shootCooldown = 120.0f;
            (*bossEnemy)->waveOffset = 0;
            (*bossEnemy)->waveAmplitude = 20.0f;
            (*bossEnemy)->waveSpeed = 0.02f;
        }
    }

    // Read checkpoint count.
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "CHECKPOINT_COUNT") != 0)
    {
        arena_free(&gameState->gameArena, *checkpoints);
        *checkpoints = NULL;
        *checkpointCount = 0;
        TraceLog(LOG_INFO, "Failed to find checkpoint count, skipping.");
    }
    else
    {
        int oldCheckpointCount = *checkpointCount;
        if (fscanf(file, "%d", checkpointCount) != 1)
        {
            checkpointCount = 0;
            *checkpoints = NULL;
            TraceLog(LOG_ERROR, "Failed to load checkpoint count!");
            fclose(file);
            return false;
        }

        if (*checkpoints == NULL)
        {
            *checkpoints = (Vector2 *)arena_alloc(&gameState->gameArena, sizeof(Vector2) * (*checkpointCount));
            if (*bossEnemy == NULL)
            {
                TraceLog(LOG_ERROR, "Failed to allocate memory for boss data!");
                fclose(file);
                return false;
            }
        }

        if (oldCheckpointCount != *checkpointCount)
        {
            *checkpoints = (Vector2 *)arena_realloc(&gameState->gameArena, *checkpoints, sizeof(Vector2) * (*checkpointCount));
        }

        for (int i = 0; i < *checkpointCount; i++)
        {
            if (fscanf(file, "%s", token) != 1 || strcmp(token, "CHECKPOINT") != 0)
            {
                TraceLog(LOG_ERROR, "Failed to find checkpoint [%i]!", i);
                fclose(file);
                return false;
            }
            if (fscanf(file, "%f %f", &(*checkpoints)[i].x, &(*checkpoints)[i].y) != 2)
            {
                TraceLog(LOG_ERROR, "Failed to load checkpoint [%i] data!", i);
                fclose(file);
                return false;
            }
        }
    }

    fclose(file);
    return true;
}

bool SaveCheckpointState(const char *filename, struct Entity player,
                         struct Entity *enemies, struct Entity bossEnemy,
                         Vector2 checkpoints[], int checkpointCount, int currentIndex)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
        return false;

    // Save player data.
    fprintf(file, "PLAYER %.2f %.2f %d\n",
            player.position.x, player.position.y, player.health);

    // Save only the created enemy entries.
    for (int i = 0; i < gameState->enemyCount; i++)
    {
        fprintf(file, "ENEMY %d %.2f %.2f %d\n",
                enemies[i].physicsType,
                enemies[i].position.x, enemies[i].position.y,
                enemies[i].health);
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

bool LoadCheckpointState(const char *filename, struct Entity **player,
                         struct Entity **enemies, struct Entity **bossEnemy,
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
    if (fscanf(file, "%f %f %d", (*player)->position.x, (*player)->position.y, (*player)->health) != 3)
    {
        fclose(file);
        return false;
    }

    // Read enemy entries.
    for (int i = 0; i < gameState->enemyCount; i++)
    {
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
            break;
        if (fscanf(file, "%d %f %f %d",
                   (int *)&(*enemies[i]).physicsType,
                   &(*enemies[i]).position.x, &(*enemies[i]).position.y,
                   &(*enemies[i]).health) != 4)
            break;
    }

    // Read boss data.
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "BOSS") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %d",
               &(*bossEnemy)->position.x, &(*bossEnemy)->position.y,
               &(*bossEnemy)->health) != 3)
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

void CheckBulletCollisions(struct Bullet bullets[], int count, struct Entity *player, struct Entity enemies[], int enemyCount, struct Entity *bossEnemy, bool *bossActive, bool *gameWon, float bulletRadius)
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

    rlImGuiSetup(true);

    gameState = (GameState *)malloc(sizeof(GameState));
    if (gameState == NULL)
    {
        // Handle allocation failure (e.g., exit the program)
        fprintf(stderr, "Failed to allocate memory for gameState.\n");
        exit(1);
    }

    // Zero out the memory to ensure all pointers are set to NULL
    memset(gameState, 0, sizeof(GameState));

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

    bool bossActive = false;
    float enemyShootRange = 300.0f;
    int bossMeleeFlashTimer = 0;

    struct Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 10.0f;
    const float bulletRadius = 5.0f;

    bool gameWon = false;
    Rectangle checkpointRect = {0, 0, TILE_SIZE, TILE_SIZE * 2};

    if (!editorMode && gameState->currentLevelFilename[0] != '\0')
    {
        if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, &gameState->enemies, &gameState->enemyCount, &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
        {
            TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
        }

        if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, &gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
        {
            TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
        }
    }

    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();
        Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

        if (editorMode)
        {
            BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode2D(camera);
            DrawTilemap(camera);

            if (gameState->checkpoints != NULL)
            {
                for (int i = 0; i < gameState->checkpointCount; i++)
                {
                    DrawRectangle(gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2, Fade(GREEN, 0.3f));
                }
            }

            if (gameState->enemies != NULL)
            {
                // Draw enemies
                for (int i = 0; i < gameState->enemyCount; i++)
                {
                    if (gameState->enemies[i].physicsType == GROUND)
                    {
                        float halfSide = gameState->enemies[i].radius;
                        DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                      (int)(gameState->enemies[i].position.y - halfSide),
                                      (int)(gameState->enemies[i].radius * 2),
                                      (int)(gameState->enemies[i].radius * 2), RED);
                    }
                    else if (gameState->enemies[i].physicsType == FLYING)
                    {
                        DrawPoly(gameState->enemies[i].position, 4, gameState->enemies[i].radius, 45.0f, ORANGE);
                    }
                    // Draw bound markers:
                    DrawLine((int)gameState->enemies[i].leftBound, 0, (int)gameState->enemies[i].leftBound, 20, BLUE);
                    DrawLine((int)gameState->enemies[i].rightBound, 0, (int)gameState->enemies[i].rightBound, 20, BLUE);
                }
            }

            // Draw the boss in the editor if placed.
            if (gameState->bossEnemy != NULL)
            {
                DrawCircleV(gameState->bossEnemy->position, gameState->bossEnemy->radius, PURPLE);
            }

            if (gameState->player != NULL)
            {
                DrawCircleV(gameState->player->position, gameState->player->radius, BLUE);
                DrawText("PLAYER", gameState->player->position.x - 20, gameState->player->position.y - gameState->player->radius - 20, 12, BLACK);
            }

            // End 2D world drawing
            EndMode2D();

            DrawEditor();
            EndDrawing();
            continue;
        }

        if (gameState->player == NULL)
            return 1;

        camera.target = gameState->player->position;
        camera.rotation = 0.0f;
        camera.zoom = 0.66f;

        // Update the music stream each frame.
        if (!IsMusicStreamPlaying(*currentTrack) && gameState->player->health > 0)
            PlayMusicStream(*currentTrack);
        UpdateMusicStream(*currentTrack);

        if (gameState->player->health > 0)
        {
            gameState->player->velocity.x = 0;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
                gameState->player->velocity.x = -4.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
                gameState->player->velocity.x = 4.0f;

            if (IsKeyPressed(KEY_SPACE))
            {
                if (fabsf(gameState->player->velocity.y) < 0.001f)
                    gameState->player->velocity.y = -8.0f;
            }

            gameState->player->velocity.y += 0.4f;
            gameState->player->position.x += gameState->player->velocity.x;
            gameState->player->position.y += gameState->player->velocity.y;
            ResolveCircleTileCollisions(&gameState->player->position, &gameState->player->velocity, &gameState->player->health, gameState->player->radius);
        }

        // Loop through all checkpoints.
        for (int i = 0; i < gameState->checkpointCount; i++)
        {
            // Define the checkpoint rectangle (using the same dimensions you use elsewhere).
            Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2};

            // Only trigger if the player collides with this checkpoint AND it hasn't been activated yet.
            if (!checkpointActivated[i] && CheckCollisionPointRec(gameState->player->position, cpRect))
            {
                // Save the checkpoint state (this saves player, enemies, boss, and the checkpoint array).
                if (SaveCheckpointState(CHECKPOINT_FILE, *gameState->player, gameState->enemies, *gameState->bossEnemy, gameState->checkpoints, gameState->checkpointCount, i))
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
                    bullets[i].position = gameState->player->position;

                    Vector2 dir = {screenPos.x - gameState->player->position.x, screenPos.y - gameState->player->position.y};
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

        for (int i = 0; i < gameState->enemyCount; i++)
        {
            if (gameState->enemies[i].health > 0)
            {
                if (gameState->enemies[i].physicsType == GROUND)
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
                else if (gameState->enemies[i].physicsType == FLYING)
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
                float dx = gameState->player->position.x - gameState->enemies[i].position.x;
                float dy = gameState->player->position.y - gameState->enemies[i].position.y;
                float distSqr = dx * dx + dy * dy;
                if (gameState->player->health > 0 && distSqr < (enemyShootRange * enemyShootRange))
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
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            if (gameState->enemies[i].health > 0)
            {
                anyEnemiesAlive = true;
                break;
            }
        }
        if (!anyEnemiesAlive && !bossActive)
        {
            bossActive = (gameState->bossEnemy->health >= 0);
            gameState->bossEnemy->shootTimer = 0; // reset attack timer on spawn
        }
        if (bossActive)
        {
            // Increment the boss's attack timer every frame.
            gameState->bossEnemy->shootTimer += 1.0f;

            // Phase 1: Ground (Melee Attack) if boss health is at least 50%
            if (gameState->bossEnemy->health >= (BOSS_MAX_HEALTH * 0.5f))
            {
                gameState->bossEnemy->physicsType = GROUND;
                gameState->bossEnemy->speed = 2.0f;
                gameState->bossEnemy->velocity.x = gameState->bossEnemy->direction * gameState->bossEnemy->speed;
                gameState->bossEnemy->velocity.y += 0.4f;
                gameState->bossEnemy->position.x += gameState->bossEnemy->velocity.x;
                gameState->bossEnemy->position.y += gameState->bossEnemy->velocity.y;
                ResolveCircleTileCollisions(&gameState->bossEnemy->position, &gameState->bossEnemy->velocity, &gameState->bossEnemy->health, gameState->bossEnemy->radius);

                // Melee attack: if the player is close enough...
                float dx = gameState->player->position.x - gameState->bossEnemy->position.x;
                float dy = gameState->player->position.y - gameState->bossEnemy->position.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < gameState->bossEnemy->radius + gameState->player->radius + 10.0f)
                {
                    if (gameState->bossEnemy->shootTimer >= gameState->bossEnemy->shootCooldown)
                    {
                        gameState->player->health -= 1; // Damage the player.
                        gameState->bossEnemy->shootTimer = 0;
                        bossMeleeFlashTimer = 10; // Trigger melee flash effect.
                    }
                }
            }
            // Phase 2: Flying with single projectile if boss health is below 50% but at least 20%
            else if (gameState->bossEnemy->health >= (BOSS_MAX_HEALTH * 0.2f))
            {
                gameState->bossEnemy->physicsType = FLYING;
                gameState->bossEnemy->speed = 4.0f;
                gameState->bossEnemy->waveOffset += gameState->bossEnemy->waveSpeed;
                gameState->bossEnemy->position.y = gameState->bossEnemy->baseY + sinf(gameState->bossEnemy->waveOffset) * gameState->bossEnemy->waveAmplitude;
                gameState->bossEnemy->position.x += gameState->bossEnemy->direction * gameState->bossEnemy->speed;

                if (gameState->bossEnemy->shootTimer >= gameState->bossEnemy->shootCooldown)
                {
                    gameState->bossEnemy->shootTimer = 0;

                    // Compute a normalized vector from boss to player.
                    float dx = gameState->player->position.x - gameState->bossEnemy->position.x;
                    float dy = gameState->player->position.y - gameState->bossEnemy->position.y;
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
                            bullets[b].position = gameState->bossEnemy->position;
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
                gameState->bossEnemy->speed = 4.0f;
                gameState->bossEnemy->waveOffset += gameState->bossEnemy->waveSpeed;
                gameState->bossEnemy->position.y = gameState->bossEnemy->baseY + sinf(gameState->bossEnemy->waveOffset) * gameState->bossEnemy->waveAmplitude;
                gameState->bossEnemy->position.x += gameState->bossEnemy->direction * gameState->bossEnemy->speed;

                if (gameState->bossEnemy->shootTimer >= gameState->bossEnemy->shootCooldown)
                {
                    gameState->bossEnemy->shootTimer = 0;

                    // Compute the central angle from the boss to the player.
                    float dx = gameState->player->position.x - gameState->bossEnemy->position.x;
                    float dy = gameState->player->position.y - gameState->bossEnemy->position.y;
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
                                bullets[b].position = gameState->bossEnemy->position;
                                bullets[b].velocity.x = projDir.x * bulletSpeed;
                                bullets[b].velocity.y = projDir.y * bulletSpeed;
                                break; // Move on to spawn the next projectile.
                            }
                        }
                    }
                }
            }

            // Common horizontal boundary checking for the boss.
            if (gameState->bossEnemy->position.x < gameState->bossEnemy->leftBound)
            {
                gameState->bossEnemy->position.x = gameState->bossEnemy->leftBound;
                gameState->bossEnemy->direction = 1;
            }
            else if (gameState->bossEnemy->position.x > gameState->bossEnemy->rightBound)
            {
                gameState->bossEnemy->position.x = gameState->bossEnemy->rightBound;
                gameState->bossEnemy->direction = -1;
            }
        }
        UpdateBullets(bullets, MAX_BULLETS, LEVEL_WIDTH, LEVEL_HEIGHT);
        CheckBulletCollisions(bullets, MAX_BULLETS, gameState->player, gameState->enemies, gameState->enemyCount, gameState->bossEnemy, &bossActive, &gameWon, bulletRadius);

        BeginDrawing();
        if (!gameWon)
        {
            ClearBackground(RAYWHITE);
            DrawText("GAME MODE: Tilemap collisions active. (ESC to exit)", 10, 10, 20, DARKGRAY);
            DrawText(TextFormat("Player Health: %d", gameState->player->health), 600, 10, 20, MAROON);

            BeginMode2D(camera);
            DrawTilemap(camera);
            if (gameState->player->health > 0)
            {
                DrawCircleV(gameState->player->position, gameState->player->radius, RED);
                DrawLine((int)gameState->player->position.x, (int)gameState->player->position.y,
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

                DrawCircleV(gameState->player->position, gameState->player->radius, DARKGRAY);
                DrawText("YOU DIED!", SCREEN_WIDTH / 2 - 50, 30, 30, RED);
                DrawText("Press SPACE for New Game", SCREEN_WIDTH / 2 - 100, 80, 20, DARKGRAY);
                if (checkpointActivated[0])
                {
                    DrawText("Press R to Respawn at Checkpoint", SCREEN_WIDTH / 2 - 130, 110, 20, DARKGRAY);
                    if (IsKeyPressed(KEY_R))
                    {
                        if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, &gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
                            TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                        else
                            TraceLog(LOG_INFO, "Checkpoint reloaded!");

                        for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
                            bullets[i].active = false;
                        for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
                            bullets[i].active = false;

                        gameState->player->health = 5;
                        gameState->player->velocity = (Vector2){0, 0};
                        for (int i = 0; i < gameState->enemyCount; i++)
                            gameState->enemies[i].velocity = (Vector2){0, 0};

                        camera.target = gameState->player->position;
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
                        if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, &gameState->enemies, &gameState->enemyCount, &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                            TraceLog(LOG_ERROR, "Failed to load level default state!");

                        gameState->player->health = 5;
                        for (int i = 0; i < MAX_BULLETS; i++)
                            bullets[i].active = false;
                        gameState->player->velocity = (Vector2){0, 0};
                        for (int i = 0; i < gameState->enemyCount; i++)
                            gameState->enemies[i].velocity = (Vector2){0, 0};
                        camera.target = gameState->player->position;
                        bossActive = false;
                        ResumeMusicStream(music);
                    }
                }
            }

            for (int i = 0; i < gameState->enemyCount; i++)
            {
                if (gameState->enemies[i].health > 0)
                {
                    if (gameState->enemies[i].physicsType == GROUND)
                    {
                        float halfSide = gameState->enemies[i].radius;
                        DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                      (int)(gameState->enemies[i].position.y - halfSide),
                                      (int)(gameState->enemies[i].radius * 2),
                                      (int)(gameState->enemies[i].radius * 2), GREEN);
                    }
                    else if (gameState->enemies[i].physicsType == FLYING)
                    {
                        DrawPoly(gameState->enemies[i].position, 4, gameState->enemies[i].radius, 45.0f, ORANGE);
                    }
                }
            }

            // Draw boss enemy if active.
            if (bossActive)
            {
                DrawCircleV(gameState->bossEnemy->position, gameState->bossEnemy->radius, PURPLE);
                DrawText(TextFormat("Boss HP: %d", gameState->bossEnemy->health), gameState->bossEnemy->position.x - 30, gameState->bossEnemy->position.y - gameState->bossEnemy->radius - 20, 10, RED);

                // If the melee flash timer is active, draw a red outline.
                if (bossMeleeFlashTimer > 0)
                {
                    // Draw a red circle slightly larger than the boss to indicate a melee hit.
                    DrawCircleLines(gameState->bossEnemy->position.x, gameState->bossEnemy->position.y, gameState->bossEnemy->radius + 5, RED);
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
        EndDrawing();
    }

    rlImGuiShutdown();

    StopMusicStream(music);
    StopSound(shotSound);
    UnloadMusicStream(music);
    UnloadSound(shotSound);
    CloseAudioDevice();

    arena_destroy(&gameState->gameArena);
    CloseWindow();
    return 0;
}
