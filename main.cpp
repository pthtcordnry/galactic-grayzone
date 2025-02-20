/*******************************************************************************************
 *
 *   Raylib - 2D Platformer with Editor Mode
 *   Compile (Windows + MinGW):
 *   gcc -o platformer_with_editor platformer_with_editor.c -I C:\raylib\include -L C:\raylib\lib -lraylib -lopengl32 -lgdi32 -lwinmm
 *
 ********************************************************************************************/

#include "main.h"
#include <imgui.h>

#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

int mapTiles[MAP_ROWS][MAP_COLS] = {0}; // 0 = empty, 1 = solid, 2 = death

int entityAssetCount = 0;
int levelFileCount = 0;
char (*levelFiles)[MAX_PATH_NAME] = NULL;
Camera2D camera;

EntityAsset *entityAssets = NULL;
GameState *gameState = NULL;

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
        particles[i].position.x += particles[i].velocity.x;
        particles[i].position.y += particles[i].velocity.y;
        particles[i].life -= 1.0f;
        if (particles[i].life <= 0)
            InitParticle(&particles[i]);
        particles[i].color.a = (unsigned char)(255 * (particles[i].life / 180.0f));
        DrawCircleV(particles[i].position, 2, particles[i].color);
    }
}

void DrawTilemap(Camera2D camera)
{
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

//
// SAVE / LOAD ASSETS
//

bool SaveEntityAssetToJson(const char *directory, const char *filename, const EntityAsset *asset, bool allowOverwrite)
{
    if (!allowOverwrite)
    {
        FILE *checkFile = fopen(filename, "r");
        if (checkFile != NULL)
        {
            TraceLog(LOG_ERROR, "File %s already exists with no overwrite!", filename);
            fclose(checkFile);
            return false;
        }
    }
    if (!EnsureDirectoryExists(directory))
    {
        TraceLog(LOG_ERROR, "Directory %s doesn't exist!", directory);
        return false;
    }
    TraceLog(LOG_INFO, "SAVE_ENTITY- ensured directory %s, opening file %s for writing...", directory, filename);
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open %s for writing!", filename);
        return false;
    }
    TraceLog(LOG_INFO, "SAVE_ENTITY- opened file %s for writing successfully", filename);
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
    TraceLog(LOG_INFO, "SAVE_ENTITY- file saved successfully!");
    fclose(file);
    return true;
}

bool SaveAllEntityAssets(const char *directory, EntityAsset *assets, int count, bool allowOverwrite)
{
    bool error = false;
    for (int i = 0; i < count; i++)
    {
        char filename[256];
        int len = strlen(directory);
        const char *sep = (len > 0 && (directory[len - 1] == '/' || directory[len - 1] == '\\')) ? "" : "/";
        snprintf(filename, sizeof(filename), "%s%s%s.ent", directory, sep, assets[i].name);
        TraceLog(LOG_INFO, "SAVE_ALL_ENTITY- compiled filename, writing %s", filename);
        if (!SaveEntityAssetToJson(directory, filename, &assets[i], allowOverwrite))
        {
            TraceLog(LOG_ERROR, "Failed to save entity: %s", assets[i].name);
            error = true;
        }
    }
    return !error;
}

// Simple asset lookup by physics type and radius.
// (In a real game you might use a more robust method, e.g. storing an asset ID.)
EntityAsset *FindEntityAsset(int physicsType, float radius)
{
    for (int i = 0; i < entityAssetCount; i++)
    {
        if (entityAssets[i].physicsType == physicsType &&
            fabsf(entityAssets[i].radius - radius) < 0.01f)
            return &entityAssets[i];
    }
    return NULL;
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
    if (ret != 7)
        return false;
    return true;
}


bool LoadEntityAssets(const char *directory, EntityAsset **assets, int *count)
{
    char fileList[256][256];
    int numFiles = ListFilesInDirectory(directory, "*.ent", fileList, 256);
    int assetCount = 0;
    if (*assets == NULL)
    {
        *assets = (EntityAsset *)arena_alloc(&gameState->gameArena, sizeof(EntityAsset) * numFiles);
    }
    else if (*count != numFiles)
    {
        *assets = (EntityAsset *)arena_realloc(&gameState->gameArena, *assets, sizeof(EntityAsset) * numFiles);
    }
    if (*assets == NULL)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for entity assets pointer with size %i!", numFiles);
        return false;
    }
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

//
// LEVEL SAVE / LOAD (Instance Data)
//

// When saving enemy instances we now output some asset info (e.g. physicsType and asset radius)
// so that during load we can “reconnect” the instance with an asset.
bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Entity *player, Entity *enemies, Entity *bossEnemy)
{
    char fullPath[256];
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
    fprintf(file, "%d %d\n", MAP_ROWS, MAP_COLS);
    for (int y = 0; y < MAP_ROWS; y++)
    {
        for (int x = 0; x < MAP_COLS; x++)
            fprintf(file, "%d ", mapTiles[y][x]);
        fprintf(file, "\n");
    }
    if (player != NULL)
    {
        fprintf(file, "PLAYER %.2f %.2f %.2f\n",
                player->position.x, player->position.y, player->radius);
    }
    fprintf(file, "ENEMY_COUNT %d\n", gameState->enemyCount);
    if (gameState->enemyCount > 0 && enemies != NULL)
    {
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            // Save: asset physicsType, instance position, bounds, health, speed, shootCooldown, and asset radius.
            fprintf(file, "ENEMY %d %.2f %.2f %.2f %.2f %d %.2f %.2f %.2f\n",
                    enemies[i].asset->physicsType,
                    enemies[i].position.x, enemies[i].position.y,
                    enemies[i].leftBound, enemies[i].rightBound,
                    enemies[i].health, enemies[i].speed, enemies[i].shootCooldown,
                    enemies[i].asset->radius);
        }
    }
    if (bossEnemy != NULL)
    {
        fprintf(file, "BOSS %.2f %.2f %.2f %.2f %d %.2f %.2f\n",
                bossEnemy->position.x, bossEnemy->position.y,
                bossEnemy->leftBound, bossEnemy->rightBound,
                bossEnemy->health, bossEnemy->speed, bossEnemy->shootCooldown);
    }
    fprintf(file, "CHECKPOINT_COUNT %d\n", gameState->checkpointCount);
    for (int i = 0; i < gameState->checkpointCount; i++)
    {
        fprintf(file, "CHECKPOINT %.2f %.2f\n",
                gameState->checkpoints[i].x, gameState->checkpoints[i].y);
    }
    fclose(file);
    return true;
}

// In LoadLevel we read instance data and then “reconnect” enemy instances to an asset
// by looking up an asset with matching physicsType and radius.
bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
               Entity **player, Entity **enemies, int *enemyCount, Entity **bossEnemy,
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
                int physType;
                float assetRadius;
                if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
                {
                    TraceLog(LOG_ERROR, "Failed to find enemy array data!");
                    fclose(file);
                    return false;
                }
                if (fscanf(file, "%d %f %f %f %f %d %f %f %f", &physType,
                           &(*enemies)[i].position.x, &(*enemies)[i].position.y,
                           &(*enemies)[i].leftBound, &(*enemies)[i].rightBound,
                           &(*enemies)[i].health, &(*enemies)[i].speed, &(*enemies)[i].shootCooldown,
                           &assetRadius) != 9)
                {
                    TraceLog(LOG_ERROR, "Failed to load enemy [%i] data!", i);
                    fclose(file);
                    return false;
                }
                (*enemies)[i].velocity = (Vector2){0, 0};
                (*enemies)[i].direction = -1;
                (*enemies)[i].shootTimer = 0;
                (*enemies)[i].baseY = (*enemies)[i].position.y;
                (*enemies)[i].waveOffset = 0;
                (*enemies)[i].waveAmplitude = 0;
                (*enemies)[i].waveSpeed = 0;
                // Look up an asset matching the saved physics type and radius.
                (*enemies)[i].asset = FindEntityAsset(physType, assetRadius);
                if ((*enemies)[i].asset == NULL)
                {
                    // Fallback: use a default enemy asset.
                    static EntityAsset defaultEnemyAsset = {"Default Enemy", ENTITY_ENEMY, GROUND, 20.0f};
                    (*enemies)[i].asset = &defaultEnemyAsset;
                }
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
            (*bossEnemy)->asset->physicsType = FLYING;
            (*bossEnemy)->baseY = (*bossEnemy)->position.y - 200;
            (*bossEnemy)->radius = 40.0f;
            (*bossEnemy)->health = BOSS_MAX_HEALTH;
            (*bossEnemy)->speed = 2.0f;
            (*bossEnemy)->leftBound = 100;
            (*bossEnemy)->rightBound = LEVEL_WIDTH - 100;
            (*bossEnemy)->direction = 1;
            (*bossEnemy)->shootTimer = 0;
            (*bossEnemy)->shootCooldown = 120.0f;
            (*bossEnemy)->waveOffset = 0;
            (*bossEnemy)->waveAmplitude = 20.0f;
            (*bossEnemy)->waveSpeed = 0.02f;
            // For boss, you might assign a default asset as needed.
        }
    }
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
            *checkpointCount = 0;
            *checkpoints = NULL;
            TraceLog(LOG_ERROR, "Failed to load checkpoint count!");
            fclose(file);
            return false;
        }
        if (*checkpoints == NULL)
        {
            *checkpoints = (Vector2 *)arena_alloc(&gameState->gameArena, sizeof(Vector2) * (*checkpointCount));
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
    fprintf(file, "PLAYER %.2f %.2f %d\n",
            player.position.x, player.position.y, player.health);
    for (int i = 0; i < gameState->enemyCount; i++)
    {
        fprintf(file, "ENEMY %d %.2f %.2f %d\n",
                enemies[i].asset->physicsType,
                enemies[i].position.x, enemies[i].position.y,
                enemies[i].health);
    }
    fprintf(file, "BOSS %.2f %.2f %d\n",
            bossEnemy.position.x, bossEnemy.position.y,
            bossEnemy.health);
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
    for (int i = 0; i < gameState->enemyCount; i++)
    {
        if (fscanf(file, "%s", token) != 1 || strcmp(token, "ENEMY") != 0)
            break;
        if (fscanf(file, "%d %f %f %d",
                   (int *)&(*enemies[i]).asset->physicsType,
                   &(*enemies[i]).position.x, &(*enemies[i]).position.y,
                   &(*enemies[i]).health) != 4)
            break;
    }
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
    ImGuiIO io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    gameState = (GameState *)malloc(sizeof(GameState));
    if (gameState == NULL)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for gameState.\n");
        return 1;
    }
    memset(gameState, 0, sizeof(GameState));
    arena_init(&gameState->gameArena, GAME_ARENA_SIZE);
    mapTiles[MAP_ROWS][MAP_COLS] = {0};
    gameState->editorMode = &editorMode;
    InitAudioDevice();
    Music music = LoadMusicStream("resources/music.mp3");
    Music victoryMusic = LoadMusicStream("resources/victory.mp3");
    Music *currentTrack = &music;
    Sound defeatMusic = LoadSound("resources/defeat.mp3");
    Sound shotSound = LoadSound("resources/shot.mp3");
    if (!editorMode)
        PlayMusicStream(music);
    camera = (Camera2D){0};
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
    if (LoadEntityAssets("./assets", &entityAssets, &entityAssetCount))
    {
        TraceLog(LOG_INFO, "Successfully loaded %i assets.", entityAssetCount);
    }
    else
    {
        TraceLog(LOG_ERROR, "Failed to load assets on init!");
    }
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
                for (int i = 0; i < gameState->enemyCount; i++)
                {
                    if (gameState->enemies[i].asset->physicsType == GROUND)
                    {
                        float halfSide = gameState->enemies[i].radius;
                        DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                      (int)(gameState->enemies[i].position.y - halfSide),
                                      (int)(gameState->enemies[i].radius * 2),
                                      (int)(gameState->enemies[i].radius * 2), RED);
                    }
                    else if (gameState->enemies[i].asset->physicsType == FLYING)
                    {
                        DrawPoly(gameState->enemies[i].position, 4, gameState->enemies[i].radius, 45.0f, ORANGE);
                    }
                    DrawLine((int)gameState->enemies[i].leftBound, 0, (int)gameState->enemies[i].leftBound, 20, BLUE);
                    DrawLine((int)gameState->enemies[i].rightBound, 0, (int)gameState->enemies[i].rightBound, 20, BLUE);
                }
            }
            if (gameState->bossEnemy != NULL)
            {
                DrawCircleV(gameState->bossEnemy->position, gameState->bossEnemy->radius, PURPLE);
            }
            if (gameState->player != NULL)
            {
                DrawCircleV(gameState->player->position, gameState->player->radius, BLUE);
                DrawText("PLAYER", gameState->player->position.x - 20, gameState->player->position.y - gameState->player->radius - 20, 12, BLACK);
            }
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
        for (int i = 0; i < gameState->checkpointCount; i++)
        {
            Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2};
            if (!checkpointActivated[i] && CheckCollisionPointRec(gameState->player->position, cpRect))
            {
                if (SaveCheckpointState(CHECKPOINT_FILE, *gameState->player, gameState->enemies, *gameState->bossEnemy, gameState->checkpoints, gameState->checkpointCount, i))
                {
                    TraceLog(LOG_INFO, "Checkpoint saved!");
                }
                checkpointActivated[i] = true;
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
                if (gameState->enemies[i].asset->physicsType == GROUND)
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
                else if (gameState->enemies[i].asset->physicsType == FLYING)
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
            gameState->bossEnemy->shootTimer = 0;
        }
        if (bossActive)
        {
            gameState->bossEnemy->shootTimer += 1.0f;
            if (gameState->bossEnemy->health >= (BOSS_MAX_HEALTH * 0.5f))
            {
                gameState->bossEnemy->asset->physicsType = GROUND;
                gameState->bossEnemy->speed = 2.0f;
                gameState->bossEnemy->velocity.x = gameState->bossEnemy->direction * gameState->bossEnemy->speed;
                gameState->bossEnemy->velocity.y += 0.4f;
                gameState->bossEnemy->position.x += gameState->bossEnemy->velocity.x;
                gameState->bossEnemy->position.y += gameState->bossEnemy->velocity.y;
                ResolveCircleTileCollisions(&gameState->bossEnemy->position, &gameState->bossEnemy->velocity, &gameState->bossEnemy->health, gameState->bossEnemy->radius);
                float dx = gameState->player->position.x - gameState->bossEnemy->position.x;
                float dy = gameState->player->position.y - gameState->bossEnemy->position.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < gameState->bossEnemy->radius + gameState->player->radius + 10.0f)
                {
                    if (gameState->bossEnemy->shootTimer >= gameState->bossEnemy->shootCooldown)
                    {
                        gameState->player->health -= 1;
                        gameState->bossEnemy->shootTimer = 0;
                        bossMeleeFlashTimer = 10;
                    }
                }
            }
            else if (gameState->bossEnemy->health >= (BOSS_MAX_HEALTH * 0.2f))
            {
                gameState->bossEnemy->asset->physicsType = FLYING;
                gameState->bossEnemy->speed = 4.0f;
                gameState->bossEnemy->waveOffset += gameState->bossEnemy->waveSpeed;
                gameState->bossEnemy->position.y = gameState->bossEnemy->baseY + sinf(gameState->bossEnemy->waveOffset) * gameState->bossEnemy->waveAmplitude;
                gameState->bossEnemy->position.x += gameState->bossEnemy->direction * gameState->bossEnemy->speed;
                if (gameState->bossEnemy->shootTimer >= gameState->bossEnemy->shootCooldown)
                {
                    gameState->bossEnemy->shootTimer = 0;
                    float dx = gameState->player->position.x - gameState->bossEnemy->position.x;
                    float dy = gameState->player->position.y - gameState->bossEnemy->position.y;
                    float len = sqrtf(dx * dx + dy * dy);
                    Vector2 dir = {0, 0};
                    if (len > 0.0f)
                    {
                        dir.x = dx / len;
                        dir.y = dy / len;
                    }
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
            else
            {
                gameState->bossEnemy->speed = 4.0f;
                gameState->bossEnemy->waveOffset += gameState->bossEnemy->waveSpeed;
                gameState->bossEnemy->position.y = gameState->bossEnemy->baseY + sinf(gameState->bossEnemy->waveOffset) * gameState->bossEnemy->waveAmplitude;
                gameState->bossEnemy->position.x += gameState->bossEnemy->direction * gameState->bossEnemy->speed;
                if (gameState->bossEnemy->shootTimer >= gameState->bossEnemy->shootCooldown)
                {
                    gameState->bossEnemy->shootTimer = 0;
                    float dx = gameState->player->position.x - gameState->bossEnemy->position.x;
                    float dy = gameState->player->position.y - gameState->bossEnemy->position.y;
                    float centerAngle = atan2f(dy, dx);
                    float fanSpread = 30.0f * DEG2RAD;
                    int numProjectiles = 5;
                    float spacing = fanSpread / 2.0f;
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
                                break;
                            }
                        }
                    }
                }
            }
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
                         (int)screenPos.x, (int)screenPos.y, GRAY);
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
                    if (gameState->enemies[i].asset->physicsType == GROUND)
                    {
                        float halfSide = gameState->enemies[i].radius;
                        DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                      (int)(gameState->enemies[i].position.y - halfSide),
                                      (int)(gameState->enemies[i].radius * 2),
                                      (int)(gameState->enemies[i].radius * 2), GREEN);
                    }
                    else if (gameState->enemies[i].asset->physicsType == FLYING)
                    {
                        DrawPoly(gameState->enemies[i].position, 4, gameState->enemies[i].radius, 45.0f, ORANGE);
                    }
                }
            }
            if (bossActive)
            {
                DrawCircleV(gameState->bossEnemy->position, gameState->bossEnemy->radius, PURPLE);
                DrawText(TextFormat("Boss HP: %d", gameState->bossEnemy->health), gameState->bossEnemy->position.x - 30, gameState->bossEnemy->position.y - gameState->bossEnemy->radius - 20, 10, RED);
                if (bossMeleeFlashTimer > 0)
                {
                    DrawCircleLines(gameState->bossEnemy->position.x, gameState->bossEnemy->position.y, gameState->bossEnemy->radius + 5, RED);
                    bossMeleeFlashTimer--;
                }
            }
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (bullets[i].active)
                {
                    DrawCircle((int)bullets[i].position.x, (int)bullets[i].position.y, bulletRadius, BLUE);
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
