#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <raylib.h>
#include "entity.h"
#include "memory_arena.h"

#define MAX_ENTITY_ASSETS 64
#define MAX_PATH_NAME 256
#define MAX_PLAYER_BULLETS 50
#define MAX_ENEMY_BULLETS 50
#define MAX_BULLETS (MAX_PLAYER_BULLETS + MAX_ENEMY_BULLETS)
#define BOSS_MAX_HEALTH 100
#define MAX_PLAYER_HEALTH 10
#define MAX_CHECKPOINTS 3

// Different game state types.
typedef enum GameStateType {
    UNINITIALIZED = 0,
    EDITOR,
    LEVEL_SELECT,
    PLAY,
    PAUSE,
    GAME_OVER,
} GameStateType;

typedef struct GameState {
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
extern char (*levelFiles)[MAX_PATH_NAME];
extern GameState *gameState;
extern EntityAsset *entityAssets;

#endif
