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
            fabsf(entityAssets[i].baseRadius - radius) < 0.01f)
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

    // Print JSON including the new uint64_t id field.
    // Note: we cast to (unsigned long long) if needed for %llu format
    fprintf(file,
            "{\n"
            "    \"id\": %llu,\n"
            "    \"name\": \"%s\",\n"
            "    \"kind\": %d,\n"
            "    \"physicsType\": %d,\n"
            "    \"baseRadius\": %.2f,\n"
            "    \"baseHp\": %d,\n"
            "    \"baseSpeed\": %.2f,\n"
            "    \"baseAttackSpeed\": %.2f\n"
            "}\n",
            (unsigned long long)asset->id,
            asset->name,
            (int)asset->kind,
            (int)asset->physicsType,
            asset->baseRadius,
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
                     "    \"id\": %llu,\n"
                     "    \"name\": \"%63[^\"]\",\n"
                     "    \"kind\": %d,\n"
                     "    \"physicsType\": %d,\n"
                     "    \"baseRadius\": %f,\n"
                     "    \"baseHp\": %d,\n"
                     "    \"baseSpeed\": %f,\n"
                     "    \"baseAttackSpeed\": %f\n"
                     "}\n",
                     (unsigned long long *)&asset->id, // parse the 64-bit id
                     asset->name,
                     (int *)&asset->kind,
                     (int *)&asset->physicsType,
                     &asset->baseRadius,
                     &asset->baseHp,
                     &asset->baseSpeed,
                     &asset->baseAttackSpeed);

    return (ret == 8);
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

bool SaveLevel(const char *filename,
               int **mapTiles,
               Entity *player,
               Entity *enemies,
               Entity *bossEnemy)
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

    // Write map dimensions
    fprintf(file, "%d %d\n", currentMapWidth, currentMapHeight);

    for (int y = 0; y < currentMapHeight; y++)
    {
        for (int x = 0; x < currentMapWidth; x++)
        {
            fprintf(file, "%d ", mapTiles[y][x]);
        }
        fprintf(file, "\n");
    }
    if (player)
    {
        // Format: PLAYER <assetId> <kind> <physicsType> <pos.x> <pos.y> <health> <speed> <shootCooldown> <radius>
        fprintf(file,
                "PLAYER %llu %d %d %.2f %.2f %d %.2f %.2f %.2f\n",
                player->assetId,
                player->kind,
                player->physicsType,
                player->basePos.x,
                player->basePos.y,
                player->health,
                player->speed,
                player->shootCooldown,
                player->radius);
    }

    fprintf(file, "ENEMY_COUNT %d\n", gameState->enemyCount);
    if (gameState->enemyCount > 0 && enemies)
    {
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            Entity *e = &enemies[i];
            // Format: ENEMY <assetId> <kind> <physicsType> <pos.x> <pos.y> <leftBound> <rightBound> <health> <speed> <shootCooldown> <radius>
            fprintf(file,
                    "ENEMY %llu %d %d %.2f %.2f %.2f %.2f %d %.2f %.2f %.2f\n",
                    e->assetId,
                    e->kind,
                    e->physicsType,
                    e->basePos.x,
                    e->basePos.y,
                    e->leftBound,
                    e->rightBound,
                    e->health,
                    e->speed,
                    e->shootCooldown,
                    e->radius);
        }
    }

    if (bossEnemy)
    {
        // Same extended format as above, but labeled "BOSS"
        fprintf(file,
                "BOSS %llu %d %d %.2f %.2f %.2f %.2f %d %.2f %.2f %.2f\n",
                bossEnemy->assetId,
                bossEnemy->kind,
                bossEnemy->physicsType,
                bossEnemy->basePos.x,
                bossEnemy->basePos.y,
                bossEnemy->leftBound,
                bossEnemy->rightBound,
                bossEnemy->health,
                bossEnemy->speed,
                bossEnemy->shootCooldown,
                bossEnemy->radius);
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

bool LoadLevel(const char *filename,
               int **mapTiles,
               Entity **player,
               Entity **enemies,
               int *enemyCount,
               Entity **bossEnemy,
               Vector2 **checkpoints,
               int *checkpointCount)
{
    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "./levels/%s", filename);

    FILE *file = fopen(fullPath, "r");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open level file: %s", fullPath);
        return false;
    }
    TraceLog(LOG_INFO, "Opened level file.");

    int rows = 0, cols = 0;
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

    char token[32];
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "PLAYER") == 0)
    {
        if (*player == NULL)
        {
            TraceLog(LOG_INFO, "Player is NULL, allocating memory...");
            *player = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            if (!(*player))
            {
                TraceLog(LOG_ERROR, "Couldn't allocate memory for player!");
                fclose(file);
                return false;
            }
        }
        TraceLog(LOG_INFO, "Player memory was allocated successfully.");
        Entity *p = *player;

        // "PLAYER <assetId> <kind> <physicsType> <pos.x> <pos.y> <health> <speed> <shootCooldown> <radius>"
        int res = fscanf(file,
                         "%llu %d %d %f %f %d %f %f %f",
                         &p->assetId,
                         &p->kind,
                         &p->physicsType,
                         &p->basePos.x,
                         &p->basePos.y,
                         &p->health,
                         &p->speed,
                         &p->shootCooldown,
                         &p->radius);

        p->position = p->basePos;
        p->velocity = (Vector2){0, 0};
        p->direction = 1;     // default or from save if you wish
        p->shootTimer = 0.0f; // reset on load or keep if you wanted
    }
    else
    {
        // No player found
        arena_free(&gameState->gameArena, *player);
        *player = NULL;
    }

    TraceLog(LOG_INFO, player ? "Player loaded." : "Successfully skipped player.");

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
            // Allocate or re-allocate memory for the enemies array
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

            // 3a) Load each ENEMY
            for (int i = 0; i < (*enemyCount); i++)
            {
                if (fscanf(file, "%s", token) == 1 && strcmp(token, "ENEMY") == 0)
                {
                    Entity *e = &(*enemies)[i];

                    // "ENEMY <assetId> <kind> <physicsType> <pos.x> <pos.y> <left> <right> <health> <speed> <shootCooldown> <radius>"
                    int res = fscanf(file,
                                     "%llu %d %d %f %f %f %f %d %f %f %f",
                                     &e->assetId,
                                     &e->kind,
                                     &e->physicsType,
                                     &e->basePos.x,
                                     &e->basePos.y,
                                     &e->leftBound,
                                     &e->rightBound,
                                     &e->health,
                                     &e->speed,
                                     &e->shootCooldown,
                                     &e->radius);

                    e->position = e->basePos;
                    e->velocity = (Vector2){0, 0};
                    e->direction = 1; // or set from save if you store direction
                    e->shootTimer = 0.0f;
                }
                else
                {
                    TraceLog(LOG_ERROR, "Missing 'ENEMY' token for enemy[%d]!", i);
                    fclose(file);
                    return false;
                }
            }
        }
        else
        {
            // 0 enemies
            if (*enemies)
            {
                arena_free(&gameState->gameArena, *enemies);
                *enemies = NULL;
            }
        }
    }
    else
    {
        // If there's no "ENEMY_COUNT" line, zero them out
        if (*enemies)
        {
            arena_free(&gameState->gameArena, *enemies);
            *enemies = NULL;
        }
        *enemyCount = 0;
    }

    TraceLog(LOG_INFO, enemies ? *enemyCount + "Enemies loaded." : "Successfully skipped enemies.");

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
        Entity *b = *bossEnemy;

        // "BOSS <assetId> <kind> <physicsType> <pos.x> <pos.y> <left> <right> <health> <speed> <shootCooldown> <radius>"
        int res = fscanf(file,
                         "%llu %d %d %f %f %f %f %d %f %f %f",
                         &b->assetId,
                         &b->kind,
                         &b->physicsType,
                         &b->basePos.x,
                         &b->basePos.y,
                         &b->leftBound,
                         &b->rightBound,
                         &b->health,
                         &b->speed,
                         &b->shootCooldown,
                         &b->radius);

        b->position = b->basePos;
        b->velocity = (Vector2){0, 0};
        b->direction = 1;
        b->shootTimer = 0.0f;
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
    TraceLog(LOG_INFO, bossEnemy ? "Boss loaded." : "Successfully skipped boss.");

    if (fscanf(file, "%s", token) == 1 && strcmp(token, "CHECKPOINT_COUNT") == 0)
    {
        int oldCount = *checkpointCount;
        if (fscanf(file, "%d", checkpointCount) != 1)
        {
            TraceLog(LOG_ERROR, "Failed reading checkpointCount!");
            fclose(file);
            return false;
        }

        // re-alloc if needed
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

        // read each checkpoint
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
        // no checkpoints
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
                enemies[i].physicsType,
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
                   (int *)&(*enemies)[i].physicsType,
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