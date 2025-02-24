#include "game_rendering.h"
#include "tile.h"
#include "memory_arena.h"
#include "game_state.h"
#include "game_storage.h"

int **mapTiles;
int currentMapWidth;
int currentMapHeight;

void InitializeTilemap(int width, int height)
{
    currentMapWidth = width;
    currentMapHeight = height;

    // Allocate 'height' row pointers.
    mapTiles = (int **)arena_alloc(&gameState->gameArena, height * sizeof(int *));
    for (int i = 0; i < height; i++)
    {
        // Allocate each row with 'width' integers.
        mapTiles[i] = (int *)arena_alloc(&gameState->gameArena, width * sizeof(int));
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

void DrawEntities(Vector2 mouseScreenPos, Entity *player, Entity *enemies, int enemyCount,
                  Entity *boss, int *bossMeleeFlash, bool bossActive)
{
    if (player != NULL && player->health > 0)
    {
        EntityAsset *asset = GetEntityAssetById(player->assetId);
        if (asset && asset->texture.id != 0)
        {
            // Scale the texture so its height fits the entity's diameter.
            float scale = (player->radius * 2) / asset->texture.height;
            Rectangle srcRec = {0, 0, asset->texture.width, asset->texture.height};
            Rectangle destRec = {player->position.x - player->radius,
                                 player->position.y - player->radius,
                                 asset->texture.width * scale,
                                 asset->texture.height * scale};
            DrawTexturePro(asset->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset texture: %s", asset->name);
        }
        // Draw a line from the player to the mouse position.
        DrawLineV(player->position, mouseScreenPos, GRAY);
    }

    // --- Draw Enemies ---
    for (int i = 0; i < enemyCount; i++)
    {
        Entity *e = &enemies[i];
        if (e->health <= 0)
            continue;

        EntityAsset *asset = GetEntityAssetById(e->assetId);
        if (asset && asset->texture.id != 0)
        {
            float scale = (e->radius * 2) / asset->texture.height;
            Rectangle srcRec = {0, 0, asset->texture.width, asset->texture.height};
            Rectangle destRec = {e->position.x - e->radius,
                                 e->position.y - e->radius,
                                 asset->texture.width * scale,
                                 asset->texture.height * scale};
            DrawTexturePro(asset->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset texture: %s", asset->name);
        }
    }

    // --- Draw Boss ---
    if (boss && boss->health > 0)
    {
        EntityAsset *asset = GetEntityAssetById(boss->assetId);
        if (asset && asset->texture.id != 0)
        {
            float scale = (boss->radius * 2) / asset->texture.height;
            Rectangle srcRec = {0, 0, asset->texture.width, asset->texture.height};
            Rectangle destRec = {boss->position.x - boss->radius,
                                 boss->position.y - boss->radius,
                                 asset->texture.width * scale,
                                 asset->texture.height * scale};
            DrawTexturePro(asset->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset texture: %s", asset->name);
        }
    }
}