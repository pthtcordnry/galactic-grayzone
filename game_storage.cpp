#include <stdio.h>
#include <string.h>
#include <math.h>
#include <raylib.h>
#include "game_storage.h"
#include "file_io.h"
#include "memory_arena.h"
#include "tile.h"

static TextureCacheEntry textureCache[MAX_TEXTURE_CACHE];
static int textureCacheCount = 0;

Texture2D GetCachedTexture(const char *path)
{
    for (int i = 0; i < textureCacheCount; i++) {
        if (strcmp(textureCache[i].path, path) == 0) {
            return textureCache[i].texture;
        }
    }
    // Return an invalid texture (id == 0 means not loaded)
    return (Texture2D){0};
}

void AddTextureToCache(const char *path, Texture2D texture) 
{
    if (textureCacheCount < MAX_TEXTURE_CACHE) {
        strcpy(textureCache[textureCacheCount].path, path);
        textureCache[textureCacheCount].texture = texture;
        textureCacheCount++;
    }
}

void LoadLevelFiles()
{
    const char *levelsDir = "levels";
    const char *levelExtension = ".level";
    int currentCount = CountFilesWithExtension(levelsDir, levelExtension);
    if (currentCount <= 0)
    {
        TraceLog(LOG_WARNING, "No level files found in %s", levelsDir);
    }
    else
    {
        if (levelFiles == NULL)
        {
            levelFiles = (char(*)[256])arena_alloc(&assetArena, currentCount * sizeof(*levelFiles));
            if (levelFiles == NULL)
                TraceLog(LOG_ERROR, "Failed to allocate memory for level file list!");
        }
        else if (currentCount != levelFileCount)
        {
            levelFiles = (char(*)[256])arena_realloc(&assetArena, levelFiles, currentCount * sizeof(*levelFiles));
        }
        levelFileCount = currentCount;
        ListFilesInDirectory(levelsDir, levelExtension, levelFiles, levelFileCount);
    }
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

    fprintf(file, EntityAssetToJSON(asset));
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
    char buffer[1024 * 10];
    size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[size] = '\0';
    fclose(file);

    if (!EntityAssetFromJSON(buffer, asset))
    {
        TraceLog(LOG_ERROR, "Failed to convert json to entity!");
        arena_free(&assetArena, buffer);
        return false;
    }

    return true;
}

bool LoadEntityAssets(const char *directory, EntityAsset **assets, int *count)
{
    char fileList[256][256];
    int numFiles = ListFilesInDirectory(directory, "*.ent", fileList, 256);

    if (*assets == NULL)
    {
        // Allocate enough memory for all potential assets
        *assets = (EntityAsset *)arena_alloc(&assetArena,
                                             sizeof(EntityAsset) * numFiles);
    }
    else if (*count != numFiles)
    {
        // Re-allocate if the number of files changed
        *assets = (EntityAsset *)arena_realloc(&assetArena,
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
               Entity player,
               Entity *enemies,
               Entity bossEnemy)
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
    if (player.kind != EMPTY)
    {
        // Format: PLAYER <assetId> <kind> <physicsType> <pos.x> <pos.y> <health> <speed> <shootCooldown> <radius>
        fprintf(file,
                "PLAYER %llu %d %d %.2f %.2f %d %.2f %.2f %.2f\n",
                player.assetId,
                player.kind,
                player.physicsType,
                player.basePos.x,
                player.basePos.y,
                player.health,
                player.speed,
                player.shootCooldown,
                player.radius);
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

    if (bossEnemy.kind != EMPTY)
    {
        // Same extended format as above, but labeled "BOSS"
        fprintf(file,
                "BOSS %llu %d %d %.2f %.2f %.2f %.2f %d %.2f %.2f %.2f\n",
                bossEnemy.assetId,
                bossEnemy.kind,
                bossEnemy.physicsType,
                bossEnemy.basePos.x,
                bossEnemy.basePos.y,
                bossEnemy.leftBound,
                bossEnemy.rightBound,
                bossEnemy.health,
                bossEnemy.speed,
                bossEnemy.shootCooldown,
                bossEnemy.radius);
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
               int ***mapTiles,
               Entity *player,
               Entity **enemies,
               int *enemyCount,
               Entity *bossEnemy,
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

    int rows = 0, cols = 0;
    // Read tilemap dimensions.
    if (fscanf(file, "%d %d", &cols, &rows) != 2)
    {
        TraceLog(LOG_WARNING, "No tilemap dimensions found!");
        fclose(file);
        return false;
    }

    TraceLog(LOG_INFO, "Initializing Tilemap memory.");
    // Allocate tilemap based on the dimensions read from the file.
    *mapTiles = InitializeTilemap(cols, rows);
    TraceLog(LOG_INFO, "Initialized Tilemap memory.");

    // Read tile data into the allocated tilemap.
    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            if (fscanf(file, "%d", &(*mapTiles)[y][x]) != 1)
            {
                TraceLog(LOG_ERROR, "Failed reading tile map at (%d,%d)!", x, y);
                fclose(file);
                return false;
            }
        }
    }

    TraceLog(LOG_INFO, "Loaded Tilemap data.");
    char token[32];
    TraceLog(LOG_INFO, "Reading Player data.");
    // Read player data.
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "PLAYER") == 0)
    {
        Entity *p = player;
        // PLAYER <assetId> <kind> <physicsType> <pos.x> <pos.y> <health> <speed> <shootCooldown> <radius>
        if (fscanf(file, "%llu %d %d %f %f %d %f %f %f",
                   &p->assetId,
                   &p->kind,
                   &p->physicsType,
                   &p->basePos.x,
                   &p->basePos.y,
                   &p->health,
                   &p->speed,
                   &p->shootCooldown,
                   &p->radius) != 9)
        {
            TraceLog(LOG_ERROR, "Failed reading player data!");
            fclose(file);
            return false;
        }
        p->state = ENTITY_STATE_IDLE;
        p->position = p->basePos;
        p->velocity = (Vector2){0, 0};
        p->direction = 1;
        p->shootTimer = 0.0f;

        EntityAsset *asset = GetEntityAssetById(p->assetId);
        if (asset)
        {
            InitEntityAnimation(&p->idle, &asset->idle, asset->texture);
            InitEntityAnimation(&p->walk, &asset->walk, asset->texture);
            InitEntityAnimation(&p->ascend,  &asset->ascend,  asset->texture);
            InitEntityAnimation(&p->fall,    &asset->fall,    asset->texture);
            InitEntityAnimation(&p->shoot, &asset->shoot, asset->texture);
            InitEntityAnimation(&p->die, &asset->die, asset->texture);
        }
    }
    TraceLog(LOG_INFO, "Read player data, now reading enemies.");

    // Read enemy count.
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "ENEMY_COUNT") == 0)
    {
        if (fscanf(file, "%d", enemyCount) != 1)
        {
            TraceLog(LOG_ERROR, "Failed reading enemy count!");
            fclose(file);
            return false;
        }

        if (*enemyCount > 0)
        {
            // Check if the enemy array has been allocated yet.
            if (*enemies == NULL)
            {
                *enemies = (Entity *)arena_alloc(&gameArena, sizeof(Entity) * (*enemyCount));
            }
            else
            {
                *enemies = (Entity *)arena_realloc(&gameArena, *enemies, sizeof(Entity) * (*enemyCount));
            }
            if (*enemies == NULL)
            {
                TraceLog(LOG_ERROR, "Couldn't allocate memory for enemies!");
                fclose(file);
                return false;
            }

            // Load each enemy from the level file.
            for (int i = 0; i < (*enemyCount); i++)
            {
                if (fscanf(file, "%s", token) == 1 && strcmp(token, "ENEMY") == 0)
                {
                    Entity *e = &(*enemies)[i];
                    // Read enemy data:
                    if (fscanf(file, "%llu %d %d %f %f %f %f %d %f %f %f",
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
                               &e->radius) != 11)
                    {
                        TraceLog(LOG_ERROR, "Failed reading enemy[%d] data!", i);
                        fclose(file);
                        return false;
                    }
                    // Initialize runtime values.
                    e->state = ENTITY_STATE_IDLE;
                    e->position = e->basePos;
                    e->velocity = (Vector2){0, 0};
                    e->direction = -1;
                    e->shootTimer = 0.0f;
                    EntityAsset *asset = GetEntityAssetById(e->assetId);
                    if (asset)
                    {
                        InitEntityAnimation(&e->idle, &asset->idle, asset->texture);
                        InitEntityAnimation(&e->walk, &asset->walk, asset->texture);
                        InitEntityAnimation(&e->ascend,  &asset->ascend,  asset->texture);
                        InitEntityAnimation(&e->fall,    &asset->fall,    asset->texture);
                        InitEntityAnimation(&e->shoot, &asset->shoot, asset->texture);
                        InitEntityAnimation(&e->die, &asset->die, asset->texture);
                    }
                }
                else
                {
                    TraceLog(LOG_ERROR, "Failed reading enemy token for enemy[%d]!", i);
                    fclose(file);
                    return false;
                }
            }
        }
        else
        {
            // If there are no enemies in the level, free any previously allocated memory.
            if (*enemies)
            {
                arena_free(&gameArena, *enemies);
                *enemies = NULL;
            }
            *enemyCount = 0;
        }
    }
    else
    {
        if (*enemies)
        {
            arena_free(&gameArena, *enemies);
            *enemies = NULL;
        }
        *enemyCount = 0;
    }

    // Read boss data.
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "BOSS") == 0)
    {
        if (!bossEnemy)
        {
            bossEnemy = (Entity *)arena_alloc(&gameArena, sizeof(Entity));
            if (!bossEnemy)
            {
                TraceLog(LOG_ERROR, "Couldn't allocate memory for boss!");
                fclose(file);
                return false;
            }
        }
        Entity *b = bossEnemy;
        // BOSS <assetId> <kind> <physicsType> <pos.x> <pos.y> <leftBound> <rightBound> <health> <speed> <shootCooldown> <radius>
        if (fscanf(file, "%llu %d %d %f %f %f %f %d %f %f %f",
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
                   &b->radius) != 11)
        {
            TraceLog(LOG_ERROR, "Failed reading boss data!");
            fclose(file);
            return false;
        }
        b->state = ENTITY_STATE_IDLE;
        b->position = b->basePos;
        b->velocity = (Vector2){0, 0};
        b->direction = -1;
        b->shootTimer = 0.0f;

        EntityAsset *asset = GetEntityAssetById(b->assetId);
        if (asset)
        {
            InitEntityAnimation(&b->idle, &asset->idle, asset->texture);
            InitEntityAnimation(&b->walk, &asset->walk, asset->texture);
            InitEntityAnimation(&b->ascend,  &asset->ascend,  asset->texture);
            InitEntityAnimation(&b->fall,    &asset->fall,    asset->texture);
            InitEntityAnimation(&b->shoot, &asset->shoot, asset->texture);
            InitEntityAnimation(&b->die, &asset->die, asset->texture);
        }
    }

    // Read checkpoints.
    if (fscanf(file, "%s", token) == 1 && strcmp(token, "CHECKPOINT_COUNT") == 0)
    {
        int oldCount = *checkpointCount;
        if (fscanf(file, "%d", checkpointCount) != 1)
        {
            TraceLog(LOG_ERROR, "Failed reading checkpoint count!");
            fclose(file);
            return false;
        }
        if (*checkpointCount > 0)
        {
            if (*checkpoints == NULL)
            {
                *checkpoints = (Vector2 *)arena_alloc(&gameArena, sizeof(Vector2) * (*checkpointCount));
            }
            else if (oldCount != *checkpointCount)
            {
                *checkpoints = (Vector2 *)arena_realloc(&gameArena, *checkpoints, sizeof(Vector2) * (*checkpointCount));
            }
            if (!(*checkpoints))
            {
                TraceLog(LOG_ERROR, "Could not allocate memory for checkpoints!");
                fclose(file);
                return false;
            }
            for (int i = 0; i < (*checkpointCount); i++)
            {
                if (fscanf(file, "%s", token) == 1 && strcmp(token, "CHECKPOINT") == 0)
                {
                    if (fscanf(file, "%f %f", &(*checkpoints)[i].x, &(*checkpoints)[i].y) != 2)
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
            if (*checkpoints)
            {
                arena_free(&gameArena, *checkpoints);
                *checkpoints = NULL;
            }
            *checkpointCount = 0;
        }
    }
    else
    {
        if (*checkpoints)
        {
            arena_free(&gameArena, *checkpoints);
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

bool LoadCheckpointState(const char *filename, Entity *player, Entity **enemies, Entity *bossEnemy, Vector2 checkpoints[], int *checkpointCount)
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
               &player->position.x,
               &player->position.y,
               &player->health) != 3)
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
               &bossEnemy->position.x,
               &bossEnemy->position.y,
               &bossEnemy->health) != 3)
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