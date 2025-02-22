#include "game_rendering.h"
#include "tile.h"

int mapTiles[MAP_ROWS][MAP_COLS] = {0};

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
            int tileId = mapTiles[y][x];
            if (tileId > 0)
            {
                // If tileId is composite (we reserve values >= 0x10000 for that)
                if (tileId >= 0x10000)
                {
                    int tsIndex = (tileId >> 16) - 1;      // Adjusted for the offset added during tile painting
                    int tileIndex = (tileId & 0xFFFF) - 1; // Subtract one: 0 means empty
                    if (tsIndex >= 0 && tsIndex < tilesetCount)
                    {
                        Tileset *ts = &tilesets[tsIndex];
                        int tileCol = tileIndex % ts->tilesPerRow;
                        int tileRow = tileIndex / ts->tilesPerRow;
                        Rectangle srcRec = {(float)(tileCol * ts->tileWidth),
                                            (float)(tileRow * ts->tileHeight),
                                            (float)ts->tileWidth,
                                            (float)ts->tileHeight};
                        // Destination rectangle: position (x * TILE_SIZE, y * TILE_SIZE) and size TILE_SIZE x TILE_SIZE.
                        Rectangle destRec = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                        // Use DrawTexturePro to draw the source rectangle scaled to the destination rectangle.
                        DrawTexturePro(ts->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
                    }
                    else
                    {
                        DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
                    }
                }
                else
                {
                    TraceLog(LOG_ERROR, "Failed to get tile composite!");
                    // Legacy behavior for simple tiles (e.g. 1 = ground, 2 = death)
                    Color color = (tileId == 2 ? MAROON : BROWN);
                    DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, color);
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
    if (player->health > 0)
    {
        DrawCircleV(player->position, player->radius, RED);
        DrawLineV(player->position, mouseScreenPos, GRAY);
    }
    else
    {
        DrawCircleV(player->position, player->radius, DARKGRAY);
    }
    // Draw enemies
    for (int i = 0; i < enemyCount; i++)
    {
        Entity *e = &enemies[i];
        if (e->health <= 0)
            continue;
        if (e->physicsType == PHYS_GROUND)
        {
            float halfSide = e->radius;
            DrawRectangle((int)(e->position.x - halfSide),
                          (int)(e->position.y - halfSide),
                          (int)(e->radius * 2),
                          (int)(e->radius * 2),
                          GREEN);
        }
        else if (e->physicsType == PHYS_FLYING)
        {
            DrawPoly(e->position, 4, e->radius, 45.0f, ORANGE);
        }
    }

    // Draw boss
    if (bossActive && boss)
    {
        DrawCircleV(boss->position, boss->radius, PURPLE);
        DrawText(TextFormat("Boss HP: %d", boss->health),
                 (int)(boss->position.x - 30),
                 (int)(boss->position.y - boss->radius - 20),
                 10, RED);

        if (*bossMeleeFlash > 0)
        {
            DrawCircleLines((int)boss->position.x,
                            (int)boss->position.y,
                            (int)(boss->radius + 5),
                            RED);
            *bossMeleeFlash--;
        }
    }
}