#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <raylib.h>
#include "entity.h"
#include "memory_arena.h"

// A 1 MB arena by default.
#define GAME_ARENA_SIZE (1024 * 1024)

#define MAX_CHECKPOINTS 3
#define MAX_ENTITY_ASSETS 64
#define MAX_PATH_NAME 256
#define CHECKPOINT_FILE "checkpoint.txt"

typedef struct GameState
{
    bool *editorMode;

    // The "active" level file name (e.g. "level1.txt").
    char currentLevelFilename[256];

    // The main memory arena for game allocations.
    MemoryArena gameArena;

    Entity *player;
    Entity *enemies;
    int enemyCount;
    Entity *bossEnemy;

    Vector2 *checkpoints;
    int checkpointCount;

} GameState;

// Externals (shared global-like data).
extern int entityAssetCount;
extern int levelFileCount;
extern char (*levelFiles)[MAX_PATH_NAME];
extern GameState *gameState;
extern EntityAsset *entityAssets;
#endif
