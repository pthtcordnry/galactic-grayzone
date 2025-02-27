#include "game_rendering.h"
#include "tile.h"
#include "memory_arena.h"
#include "game_state.h"
#include "game_storage.h"

int **mapTiles;
int currentMapWidth;
int currentMapHeight;

void DrawAnimation(Animation anim, Vector2 position, float scale, int direction)
{
    // Get the current frame's rectangle from the static data.
    Rectangle srcRec = anim.framesData->frames[anim.currentFrame];
    Rectangle destRec = {
        position.x - (srcRec.width * scale) / 2,
        position.y - (srcRec.height * scale) / 2,
        srcRec.width * scale,
        srcRec.height * scale};

    srcRec.width = srcRec.width * direction;
    DrawTexturePro(anim.texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
}

void InitializeTilemap(int width, int height)
{
    currentMapWidth = width;
    currentMapHeight = height;

    // Allocate 'height' row pointers.
    mapTiles = (int **)arena_alloc(&gameArena, height * sizeof(int *));
    for (int i = 0; i < height; i++)
    {
        // Allocate each row with 'width' integers.
        mapTiles[i] = (int *)arena_alloc(&gameArena, width * sizeof(int));
        memset(mapTiles[i], 0, width * sizeof(int));
    }
}

void DrawTilemap(Camera2D *cam)
{
    float camWorldWidth = LEVEL_WIDTH / cam->zoom;
    float camWorldHeight = LEVEL_HEIGHT / cam->zoom;

    float camLeft = cam->target.x - camWorldWidth * 0.5f;
    float camRight = cam->target.x + camWorldWidth * 0.5f;
    float camTop = cam->target.y - camWorldHeight * 0.5f;
    float camBottom = cam->target.y + camWorldHeight * 0.5f;

    int minTileX = (int)(camLeft / TILE_SIZE);
    int maxTileX = (int)(camRight / TILE_SIZE);
    int minTileY = (int)(camTop / TILE_SIZE);
    int maxTileY = (int)(camBottom / TILE_SIZE);

    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= currentMapWidth)
        maxTileX = currentMapWidth - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= currentMapHeight)
        maxTileY = currentMapHeight - 1;

    for (int y = minTileY; y <= maxTileY; y++)
    {
        for (int x = minTileX; x <= maxTileX; x++)
        {
            int tileId = mapTiles[y][x];
            if (tileId > 0)
            {
                if (tileId >= 0x100000)
                {
                    int tsIndex = ((tileId >> 20) & 0xFFF) - 1; // extract tileset index
                    int tilePhysics = (tileId >> 16) & 0xF;     // extract physics flag
                    int tileIndex = (tileId & 0xFFFF) - 1;      // extract tile index

                    if (tsIndex >= 0 && tsIndex < tilesetCount)
                    {
                        Tileset *ts = &tilesets[tsIndex];
                        int tileCol = tileIndex % ts->tilesPerRow;
                        int tileRow = tileIndex / ts->tilesPerRow;
                        Rectangle srcRec = {(float)(tileCol * ts->tileWidth),
                                            (float)(tileRow * ts->tileHeight),
                                            (float)ts->tileWidth,
                                            (float)ts->tileHeight};

                        // Optionally, if you want the tile to always fit a world tile,
                        // define the destination rectangle with TILE_SIZE width & height.
                        Rectangle destRec = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                        DrawTexturePro(ts->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
                    }
                }
            }
            else
            {
                DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, LIGHTGRAY);
            }
        }
    }
}

void DrawEntities(float deltaTime, Vector2 mouseScreenPos, Entity *player, Entity *enemies, int enemyCount,
                  Entity *boss, int *bossMeleeFlash, bool bossActive)
{
    // --- Draw Player ---
    if (player != NULL && player->health > 0)
    {
        EntityAsset *asset = GetEntityAssetById(player->assetId);
        if (asset)
        {
            Animation *animToPlay;
            switch (player->state)
            {
                case ENTITY_STATE_IDLE:
                animToPlay = &player->idle;
                break;
                case ENTITY_STATE_WALK:
                animToPlay = &player->walk;
                break;
                case ENTITY_STATE_JUMP:
                animToPlay = &player->jump;
                break;
                case ENTITY_STATE_DIE:
                animToPlay = &player->jump;
                break;
                case ENTITY_STATE_SHOOT:
                animToPlay = &player->shoot;
                break;
            }
            
            UpdateAnimation(animToPlay, deltaTime);

            float scale = (player->radius * 2) / animToPlay->framesData->frames[0].height;
            DrawAnimation(*animToPlay, player->position, scale, player->direction);

            // Additional rendering (like drawing debug lines)
            DrawLineV(player->position, mouseScreenPos, GRAY);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset for player");
        }
    }

    // --- Draw Enemies ---
    for (int i = 0; i < enemyCount; i++)
    {
        Entity *e = &enemies[i];
        if (e->health <= 0)
            continue;

        EntityAsset *asset = GetEntityAssetById(e->assetId);
        if (asset)
        {
            UpdateAnimation(&e->idle, deltaTime);
            float scale = (e->radius * 2) / e->idle.framesData->frames[0].height;
            DrawAnimation(e->idle, e->position, scale, e->direction);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset for enemy");
        }
    }

    // --- Draw Boss ---
    if (boss && boss->health > 0)
    {
        EntityAsset *asset = GetEntityAssetById(boss->assetId);
        if (asset)
        {
            UpdateAnimation(&boss->idle, deltaTime);
            float scale = (boss->radius * 2) / boss->idle.framesData->frames[0].height;
            DrawAnimation(boss->idle, boss->position, scale, boss->direction);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset for boss");
        }
    }
}
