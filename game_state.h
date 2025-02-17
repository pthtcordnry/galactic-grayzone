#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "memory_arena.h"
#include <raylib.h>
#include "entity.h"

#define GAME_ARENA_SIZE (5120 * 5120) // 1 MB arena
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define LEVEL_WIDTH 3000
#define LEVEL_HEIGHT 800
#define TILE_SIZE 50
#define MAP_COLS (LEVEL_WIDTH / TILE_SIZE)
#define MAP_ROWS (LEVEL_HEIGHT / TILE_SIZE)
#define MAX_ENEMIES 10
#define MAX_CHECKPOINTS 3

#define CHECKPOINT_FILE "checkpoint.txt"


typedef struct GameState
{
    bool *editorMode;
    char currentLevelFilename[256] = "level1.txt";
    MemoryArena gameArena;
    Entity player;
    Entity enemies[MAX_ENEMIES];
    Entity bossEnemy;
    Vector2 checkpoints[MAX_CHECKPOINTS];
    int checkpointCount;
} GameState;


extern GameState *gameState;
bool LoadEntityAssets(const char *filename, EntityAsset *assets, int *count);
bool SaveEntityAssets(const char *filename, EntityAsset *assets, int count);
bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
    struct Entity player, struct Entity enemies[MAX_ENEMIES], struct Entity bossEnemy);
bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
    struct Entity *player, struct Entity enemies[MAX_ENEMIES], struct Entity *bossEnemy,
    Vector2 checkpoints[], int *checkpointCount);
bool SaveCheckpointState(const char *filename, struct Entity player,
    struct Entity enemies[MAX_ENEMIES], struct Entity bossEnemy,
    Vector2 checkpoints[], int checkpointCount, int currentIndex);
bool LoadCheckpointState(const char *filename, struct Entity *player,
    struct Entity enemies[MAX_ENEMIES], struct Entity *bossEnemy,
    Vector2 checkpoints[], int *checkpointCount);
#endif