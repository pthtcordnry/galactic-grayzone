#ifndef GAME_RENDERING_H
#define GAME_RENDERING_H

#include <raylib.h>
#include "entity.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define TILE_SIZE 50
#define MAX_PARTICLES 200
#define CROSSHAIR_DISTANCE 50

// Particle system for fireworks.
typedef struct Particle
{
    Vector2 position;
    Vector2 velocity;
    float life; // Frames remaining.
    Color color;
} Particle;
static Particle particles[MAX_PARTICLES];

extern unsigned int **mapTiles;
extern int currentMapWidth;
extern int currentMapHeight;

// Initialize the tilemap with the given width and height.
unsigned int **InitializeTilemap(int width, int height);

// Draw the tilemap using the specified camera.
void DrawTilemap(Camera2D *cam);

// Draw game entities (player, enemies, boss) with the given parameters.
void DrawEntities(float deltaTime, Vector2 mouseScreenPos, Entity *player,
                  Entity *enemies, int enemyCount, Entity *boss,
                  int *bossMeleeFlash, bool bossActive);

// Draw an animated sprite at the specified position.
void DrawAnimation(Animation anim, Vector2 position, float scale, int direction);
void DrawCheckpoints(Texture2D checkpointReady, Texture2D checkpointActivated, Vector2 *checkpoints, int checkpointCount, int currentIndex);

void UpdateAndDrawFireworks(void);

#endif
