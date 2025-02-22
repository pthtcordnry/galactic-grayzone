#ifndef GAME_RENDERING_H
#define GAME_RENDERING_H

#include <raylib.h>
#include "entity.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define LEVEL_WIDTH 3000
#define LEVEL_HEIGHT 800

#define TILE_SIZE 50
extern int **mapTiles;  
extern int currentMapWidth;
extern int currentMapHeight;


void DrawTilemap(Camera2D *cam);
void DrawEntities(Vector2 mouseScreenPos, Entity *player, Entity *enemies, int enemyCount, 
    Entity *boss, int *bossMeleeFlash, bool bossActive);
#endif