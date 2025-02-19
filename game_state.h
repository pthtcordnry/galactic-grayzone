#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <raylib.h>
#include "entity.h"
#include "memory_arena.h"

#define GAME_ARENA_SIZE (1024 * 1024) // 1 MB arena

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define LEVEL_WIDTH 3000
#define LEVEL_HEIGHT 800

#define TILE_SIZE 50
#define MAP_COLS (LEVEL_WIDTH / TILE_SIZE)
#define MAP_ROWS (LEVEL_HEIGHT / TILE_SIZE)

#define MAX_CHECKPOINTS 3
#define MAX_ENTITY_ASSETS 64

#define MAX_PATH_NAME 256
#define CHECKPOINT_FILE "checkpoint.txt"

typedef struct GameState
{
    bool *editorMode;
    char currentLevelFilename[256] = "level1.txt";
    MemoryArena gameArena;
    Entity *player;
    Entity *enemies;
    int enemyCount;  
    Entity *bossEnemy;
    Vector2 *checkpoints;
    int checkpointCount;
} GameState;


extern GameState *gameState;
extern int entityAssetCount;
extern int levelFileCount;
extern char (*levelFiles)[MAX_PATH_NAME];
extern Entity *entityAssets;


bool LoadEntityAssetFromJson(const char *filename, Entity *asset);
bool LoadEntityAssets(const char *directory, Entity **assets, int *count);
bool SaveEntityAssetToJson(const char *filename, const Entity *asset, bool allowOverride);
bool SaveAllEntityAssets(const char *directory, Entity *assets, int count, bool allowOverride);
bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
    struct Entity *player, struct Entity *enemies, struct Entity *bossEnemy);
bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
    struct Entity **player, struct Entity **enemies, int *enemyCount, struct Entity **bossEnemy,
    Vector2 **checkpoints, int *checkpointCount);
bool SaveCheckpointState(const char *filename, struct Entity player,
    struct Entity *enemies, struct Entity bossEnemy,
    Vector2 checkpoints[], int checkpointCount, int currentIndex);
bool LoadCheckpointState(const char *filename, struct Entity **player,
    struct Entity **enemies, struct Entity **bossEnemy,
    Vector2 checkpoints[], int *checkpointCount);
#endif