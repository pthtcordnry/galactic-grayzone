#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <raylib.h>
#include "entity.h"
#include "memory_arena.h"
#include "file_io.h"


typedef enum GameStateType
{
    UNINITIALIZED = 0,
    EDITOR,
    LEVEL_SELECT,
    PLAY,
    PAUSE,
    GAME_OVER,
} GameStateType;

typedef struct GameState
{
    GameStateType currentState;
    char currentLevelFilename[256];

    Entity player;
    Entity *enemies;
    int enemyCount;
    Entity bossEnemy;

    Vector2 *checkpoints;
    int checkpointCount;
    int currentCheckpointIndex;
} GameState;

extern bool editorMode;
extern Camera2D camera;
extern int entityAssetCount;
extern int levelFileCount;
extern char (*levelFiles)[MAX_FILE_PATH];
extern GameState *gameState;
extern EntityAsset *entityAssets;

#endif
