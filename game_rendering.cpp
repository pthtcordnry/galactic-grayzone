#include "game_rendering.h"

int mapTiles[MAP_ROWS][MAP_COLS] = {0};

void DrawTilemap(const Camera2D &cam)
{
    float camWorldWidth = LEVEL_WIDTH / cam.zoom;
    float camWorldHeight = LEVEL_HEIGHT / cam.zoom;

    float camLeft = cam.target.x - camWorldWidth * 0.5f;
    float camRight = cam.target.x + camWorldWidth * 0.5f;
    float camTop = cam.target.y - camWorldHeight * 0.5f;
    float camBottom = cam.target.y + camWorldHeight * 0.5f;

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
                DrawRectangle(x * TILE_SIZE,
                              y * TILE_SIZE,
                              TILE_SIZE,
                              TILE_SIZE,
                              (mapTiles[y][x] == 2 ? MAROON : BROWN));
            }
            else
            {
                DrawRectangleLines(x * TILE_SIZE,
                                   y * TILE_SIZE,
                                   TILE_SIZE,
                                   TILE_SIZE,
                                   LIGHTGRAY);
            }
        }
    }
}