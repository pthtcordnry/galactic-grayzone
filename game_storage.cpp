#include <stdio.h>
#include <string.h>
#include <math.h>
#include <raylib.h>
#include "game_storage.h"
#include "file_io.h"
#include "memory_arena.h"

// Forward-declare a small helper you had in main to find an asset by physics type & radius
static EntityAsset *FindEntityAsset(int physicsType, float radius)
{
    for (int i = 0; i < entityAssetCount; i++)
    {
        if (entityAssets[i].physicsType == physicsType &&
            fabsf(entityAssets[i].radius - radius) < 0.01f)
        {
            return &entityAssets[i];
        }
    }
    return NULL;
}

// ----------------------------------------------------------------------------
// Save/Load for EntityAsset (individual .ent files)
// ----------------------------------------------------------------------------

bool SaveEntityAssetToJson(const char *directory,
                           const char *filename,
                           const EntityAsset *asset,
                           bool allowOverwrite)
{
    if (!allowOverwrite)
    {
        // Check if file already exists
        FILE *checkFile = fopen(filename, "r");
        if (checkFile != NULL)
        {
            TraceLog(LOG_ERROR, "File %s already exists, no overwrite allowed!", filename);
            fclose(checkFile);
            return false;
        }
    }

    // Ensure the directory exists
    if (!EnsureDirectoryExists(directory))
    {
        TraceLog(LOG_ERROR, "Directory %s doesn't exist (or can't create)!", directory);
        return false;
    }

    FILE *file = fopen(filename, "w");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open %s for writing!", filename);
        return false;
    }

    fprintf(file,
            "{\n"
            "    \"name\": \"%s\",\n"
            "    \"kind\": %d,\n"
            "    \"physicsType\": %d,\n"
            "    \"radius\": %.2f,\n"
            "    \"baseHp\": %d,\n"
            "    \"baseSpeed\": %.2f,\n"
            "    \"baseAttackSpeed\": %.2f\n"
            "}\n",
            asset->name,
            asset->kind,
            asset->physicsType,
            asset->radius,
            asset->baseHp,
            asset->baseSpeed,
            asset->baseAttackSpeed);

    fclose(file);
    return true;
}

bool SaveAllEntityAssets(const char *directory,
                         EntityAsset *assets,
                         int count,
                         bool allowOverwrite)
{
    bool error = false;
    for (int i = 0; i < count; i++)
    {
        char filename[256];
        int len = (int)strlen(directory);
        const char *sep = (len > 0 && (directory[len - 1] == '/' || directory[len - 1] == '\\'))
                              ? ""
                              : "/";

        snprintf(filename, sizeof(filename), "%s%s%s.ent", directory, sep, assets[i].name);

        if (!SaveEntityAssetToJson(directory, filename, &assets[i], allowOverwrite))
        {
            TraceLog(LOG_ERROR, "Failed to save entity: %s", assets[i].name);
            error = true;
        }
    }
    return !error;
}

bool LoadEntityAssetFromJson(const char *filename, EntityAsset *asset)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return false;

    char buffer[1024];
    size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[size] = '\0';
    fclose(file);

    int ret = sscanf(buffer,
                     "{\n"
                     "    \"name\": \"%63[^\"]\",\n"
                     "    \"kind\": %d,\n"
                     "    \"physicsType\": %d,\n"
                     "    \"radius\": %f,\n"
                     "    \"baseHp\": %d,\n"
                     "    \"baseSpeed\": %f,\n"
                     "    \"baseAttackSpeed\": %f\n"
                     "}\n",
                     asset->name,
                     &asset->kind,
                     &asset->physicsType,
                     &asset->radius,
                     &asset->baseHp,
                     &asset->baseSpeed,
                     &asset->baseAttackSpeed);

    return (ret == 7);
}

bool LoadEntityAssets(const char *directory, EntityAsset **assets, int *count)
{
    char fileList[256][256];
    int numFiles = ListFilesInDirectory(directory, "*.ent", fileList, 256);

    if (*assets == NULL)
    {
        // Allocate enough memory for all potential assets
        *assets = (EntityAsset *)arena_alloc(&gameState->gameArena,
                                             sizeof(EntityAsset) * numFiles);
    }
    else if (*count != numFiles)
    {
        // Re-allocate if the number of files changed
        *assets = (EntityAsset *)arena_realloc(&gameState->gameArena,
                                               *assets,
                                               sizeof(EntityAsset) * numFiles);
    }

    if (*assets == NULL)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for entity assets (size %d)", numFiles);
        return false;
    }

    int assetCount = 0;
    for (int i = 0; i < numFiles; i++)
    {
        if (LoadEntityAssetFromJson(fileList[i], &((*assets)[assetCount])))
        {
            assetCount++;
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load entity asset %s!", fileList[i]);
        }
    }
    *count = assetCount;
    return true;
}

// ----------------------------------------------------------------------------
// Save/Load for Level (mapTiles + entity placements)
// ----------------------------------------------------------------------------

bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS], Entity *player, Entity *enemies, Entity *bossEnemy)
{
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "./levels/%s", filename);

    if (!EnsureDirectoryExists("./levels/"))
    {
        TraceLog(LOG_ERROR, "Cannot ensure ./levels/ directory!");
        return false;
    }

    FILE *file = fopen(fullPath, "w");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open file for saving: %s", fullPath);
        return false;
    }

    // Write map dimensions (MAP_ROWS, MAP_COLS)
    fprintf(file, "%d %d\n", MAP_ROWS, MAP_COLS);
    for (int y = 0; y < MAP_ROWS; y++)
    {
        for (int x = 0; x < MAP_COLS; x++)
            fprintf(file, "%d ", mapTiles[y][x]);
        fprintf(file, "\n");
    }

    if (player)
    {
        fprintf(file, "PLAYER %.2f %.2f %.2f\n",
                player->position.x,
                player->position.y,
                player->radius);
    }

    fprintf(file, "ENEMY_COUNT %d\n", gameState->enemyCount);
    if (gameState->enemyCount > 0 && enemies)
    {
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            // Save enough data to re-link to the correct entity asset
            fprintf(file, "ENEMY %d %.2f %.2f %.2f %.2f %d %.2f %.2f %.2f\n",
                    enemies[i].asset->physicsType,
                    enemies[i].position.x,
                    enemies[i].position.y,
                    enemies[i].leftBound,
                    enemies[i].rightBound,
                    enemies[i].health,
                    enemies[i].speed,
                    enemies[i].shootCooldown,
                    enemies[i].asset->radius);
        }
    }

    if (bossEnemy)
    {
        fprintf(file, "BOSS %.2f %.2f %.2f %.2f %d %.2f %.2f\n",
                bossEnemy->position.x,
                bossEnemy->position.y,
                bossEnemy->leftBound,
                bossEnemy->rightBound,
                bossEnemy->health,
                bossEnemy->speed,
                bossEnemy->shootCooldown);
    }

    fprintf(file, "CHECKPOINT_COUNT %d\n", gameState->checkpointCount);
    for (int i = 0; i < gameState->checkpointCount; i++)
    {
        fprintf(file, "CHECKPOINT %.2f %.2f\n",
                gameState->checkpoints[i].x,
                gameState->checkpoints[i].y);
    }

    fclose(file);
    return true;
}

bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS], Entity **player, Entity **enemies, int *enemyCount, Entity **bossEnemy, Vector2 **checkpoints, int *checkpointCount)
{
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "./levels/%s", filename);

    FILE *file = fopen(fullPath, "r");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open level file: %s", fullPath);
        return false;
    }

    int rows, cols;
    if (fscanf(file, "%d %d", &rows, &cols) == 2)
    {
        // Load tile map
        for (int y = 0; y < rows; y++)
        {
            for (int x = 0; x < cols; x++)
            {
                if (fscanf(file, "%d", &mapTiles[y][x]) != 1)
                {
                    TraceLog(LOG_ERROR, "Failed reading tile map!");
                    fclose(file);
                    return false;
                }
            }
        }
    }
    else
    {
        TraceLog(LOG_WARNING, "No tilemap dimensions found!");
    }

    // Read optional "PLAYER"
    char token[32];
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "PLAYER") == 0)
    {
        // If we have no allocated player, do it now:
        if (*player == NULL)
        {
            *player = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            if (!(*player))
            {
                TraceLog(LOG_ERROR, "Couldn't allocate memory for player!");
                fclose(file);
                return false;
            }
        }

        if (fscanf(file, "%f %f %f",
                   &(*player)->position.x,
                   &(*player)->position.y,
                   &(*player)->radius) != 3)
        {
            TraceLog(LOG_ERROR, "Failed reading player data!");
            fclose(file);
            return false;
        }
    }
    else
    {
        // No player
        arena_free(&gameState->gameArena, *player);
        *player = NULL;
    }

    // Read ENEMY_COUNT
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "ENEMY_COUNT") == 0)
    {
        int oldEnemyCount = *enemyCount;
        if (fscanf(file, "%d", enemyCount) != 1)
        {
            TraceLog(LOG_ERROR, "Failed reading enemy count!");
            fclose(file);
            return false;
        }

        if (*enemyCount > 0)
        {
            // Allocate or re-allocate
            if (!(*enemies))
            {
                *enemies = (Entity *)arena_alloc(&gameState->gameArena,
                                                 sizeof(Entity) * (*enemyCount));
            }
            else if (oldEnemyCount != *enemyCount)
            {
                *enemies = (Entity *)arena_realloc(&gameState->gameArena,
                                                   *enemies,
                                                   sizeof(Entity) * (*enemyCount));
            }

            if (!(*enemies))
            {
                TraceLog(LOG_ERROR, "Couldn't allocate memory for enemies!");
                fclose(file);
                return false;
            }

            // Load each enemy
            for (int i = 0; i < (*enemyCount); i++)
            {
                if (fscanf(file, "%s", token) == 1 && strcmp(token, "ENEMY") == 0)
                {
                    int physType;
                    float assetRadius;

                    if (fscanf(file, "%d %f %f %f %f %d %f %f %f",
                               &physType,
                               &(*enemies)[i].position.x,
                               &(*enemies)[i].position.y,
                               &(*enemies)[i].leftBound,
                               &(*enemies)[i].rightBound,
                               &(*enemies)[i].health,
                               &(*enemies)[i].speed,
                               &(*enemies)[i].shootCooldown,
                               &assetRadius) == 9)
                    {
                        (*enemies)[i].velocity = (Vector2){0, 0};
                        (*enemies)[i].direction = -1;
                        (*enemies)[i].shootTimer = 0.0f;
                        (*enemies)[i].baseY = (*enemies)[i].position.y;
                        (*enemies)[i].waveOffset = 0;
                        (*enemies)[i].waveAmplitude = 0;
                        (*enemies)[i].waveSpeed = 0;

                        // Link back to the correct asset
                        (*enemies)[i].asset = FindEntityAsset(physType, assetRadius);
                        if (!(*enemies)[i].asset)
                        {
                            // If not found, you might give them a default
                            static EntityAsset defaultEnemyAsset = {
                                "Default Enemy",
                                ENTITY_ENEMY,
                                PHYS_GROUND,
                                20.0f, // radius
                                3,     // baseHp
                                2.0f,  // baseSpeed
                                60.0f  // baseAttackSpeed
                            };
                            (*enemies)[i].asset = &defaultEnemyAsset;
                        }
                    }
                    else
                    {
                        TraceLog(LOG_ERROR, "Failed loading enemy[%d] data!", i);
                        fclose(file);
                        return false;
                    }
                }
                else
                {
                    TraceLog(LOG_ERROR, "Missing 'ENEMY' token for enemy[%d]!", i);
                    fclose(file);
                    return false;
                }
            }
        }
    }
    else
    {
        // No enemies
        if (*enemies)
        {
            arena_free(&gameState->gameArena, *enemies);
            *enemies = NULL;
        }
        *enemyCount = 0;
    }

    // Read BOSS
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "BOSS") == 0)
    {
        if (!(*bossEnemy))
        {
            *bossEnemy = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            if (!(*bossEnemy))
            {
                TraceLog(LOG_ERROR, "Couldn't allocate memory for boss!");
                fclose(file);
                return false;
            }
        }

        if (fscanf(file, "%f %f %f %f %d %f %f",
                   &(*bossEnemy)->position.x,
                   &(*bossEnemy)->position.y,
                   &(*bossEnemy)->leftBound,
                   &(*bossEnemy)->rightBound,
                   &(*bossEnemy)->health,
                   &(*bossEnemy)->speed,
                   &(*bossEnemy)->shootCooldown) == 7)
        {
            // Hardcode some default boss properties
            (*bossEnemy)->asset->physicsType = PHYS_FLYING;
            (*bossEnemy)->baseY = (*bossEnemy)->position.y - 200;
            (*bossEnemy)->radius = 40.0f;
            (*bossEnemy)->direction = 1;
            (*bossEnemy)->shootTimer = 0.0f;
            (*bossEnemy)->waveOffset = 0.0f;
            (*bossEnemy)->waveAmplitude = 20.0f;
            (*bossEnemy)->waveSpeed = 0.02f;
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed reading boss data!");
            fclose(file);
            return false;
        }
    }
    else
    {
        // No boss
        if (*bossEnemy)
        {
            arena_free(&gameState->gameArena, *bossEnemy);
            *bossEnemy = NULL;
        }
    }

    // Read CHECKPOINT_COUNT
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "CHECKPOINT_COUNT") == 0)
    {
        int oldCount = *checkpointCount;
        if (fscanf(file, "%d", checkpointCount) != 1)
        {
            TraceLog(LOG_ERROR, "Failed reading checkpointCount!");
            fclose(file);
            return false;
        }

        if (*checkpoints == NULL)
        {
            *checkpoints = (Vector2 *)arena_alloc(&gameState->gameArena,
                                                  sizeof(Vector2) * (*checkpointCount));
        }
        else if (oldCount != *checkpointCount)
        {
            *checkpoints = (Vector2 *)arena_realloc(&gameState->gameArena,
                                                    *checkpoints,
                                                    sizeof(Vector2) * (*checkpointCount));
        }

        if (!(*checkpoints))
        {
            TraceLog(LOG_ERROR, "Could not allocate for checkpoints!");
            fclose(file);
            return false;
        }

        for (int i = 0; i < (*checkpointCount); i++)
        {
            if (fscanf(file, "%s", token) == 1 && strcmp(token, "CHECKPOINT") == 0)
            {
                if (fscanf(file, "%f %f",
                           &(*checkpoints)[i].x,
                           &(*checkpoints)[i].y) != 2)
                {
                    TraceLog(LOG_ERROR, "Failed reading checkpoint[%d] data!", i);
                    fclose(file);
                    return false;
                }
            }
            else
            {
                TraceLog(LOG_ERROR, "Missing 'CHECKPOINT' token at index %d!", i);
                fclose(file);
                return false;
            }
        }
    }
    else
    {
        // No checkpoints
        if (*checkpoints)
        {
            arena_free(&gameState->gameArena, *checkpoints);
            *checkpoints = NULL;
        }
        *checkpointCount = 0;
    }

    fclose(file);
    return true;
}

// ----------------------------------------------------------------------------
// Save/Load for Checkpoints
// ----------------------------------------------------------------------------

bool SaveCheckpointState(const char *filename, Entity player, Entity *enemies, Entity bossEnemy, Vector2 checkpoints[], int checkpointCount, int currentIndex)
{
    FILE *file = fopen(filename, "w");
    if (!file)
        return false;

    // Save player
    fprintf(file, "PLAYER %.2f %.2f %d\n",
            player.position.x,
            player.position.y,
            player.health);

    // Save enemies
    for (int i = 0; i < gameState->enemyCount; i++)
    {
        fprintf(file, "ENEMY %d %.2f %.2f %d\n",
                enemies[i].asset->physicsType,
                enemies[i].position.x,
                enemies[i].position.y,
                enemies[i].health);
    }

    // Save boss
    fprintf(file, "BOSS %.2f %.2f %d\n",
            bossEnemy.position.x,
            bossEnemy.position.y,
            bossEnemy.health);

    // Save index of last checkpoint
    fprintf(file, "LAST_CHECKPOINT_INDEX %d\n", currentIndex);

    fclose(file);
    return true;
}

bool LoadCheckpointState(const char *filename, Entity **player, Entity **enemies, Entity **bossEnemy, Vector2 checkpoints[], int *checkpointCount)
{
    FILE *file = fopen(filename, "r");
    if (!file)
        return false;

    char token[32];

    // Player
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "PLAYER") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %d",
               &(*player)->position.x,
               &(*player)->position.y,
               &(*player)->health) != 3)
    {
        fclose(file);
        return false;
    }

    // Enemies
    for (int i = 0; i < gameState->enemyCount; i++)
    {
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
            break; // Possibly out of data

        if (fscanf(file, "%d %f %f %d",
                   (int *)&(*enemies)[i].asset->physicsType,
                   &(*enemies)[i].position.x,
                   &(*enemies)[i].position.y,
                   &(*enemies)[i].health) != 4)
        {
            break;
        }
    }

    // Boss
    if (fscanf(file, "%s", token) != 1 || strcmp(token, "BOSS") != 0)
    {
        fclose(file);
        return false;
    }
    if (fscanf(file, "%f %f %d",
               &(*bossEnemy)->position.x,
               &(*bossEnemy)->position.y,
               &(*bossEnemy)->health) != 3)
    {
        fclose(file);
        return false;
    }

    // Last checkpoint index
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
        // Mark all checkpoints up to that index as "activated"
        for (int i = 0; i <= currentIndex; i++)
        {
            // You had an external array `checkpointActivated[i] = true;`
            // Make sure to keep that in the code that references it.
            // e.g.: checkpointActivated[i] = true;
        }
    }

    fclose(file);
    return true;
}