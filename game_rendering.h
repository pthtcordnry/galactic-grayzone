#ifndef GAME_RENDERING_H
#define GAME_RENDERING_H

#include <raylib.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define LEVEL_WIDTH 3000
#define LEVEL_HEIGHT 800

#define TILE_SIZE 50
#define MAP_COLS (LEVEL_WIDTH / TILE_SIZE)
#define MAP_ROWS (LEVEL_HEIGHT / TILE_SIZE)

// Tilemap data: 0 = empty, 1 = solid, 2 = death
extern int mapTiles[MAP_ROWS][MAP_COLS];

void DrawTilemap(const Camera2D &cam);
#endif