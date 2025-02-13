#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "memory_arena.h"
#include <raylib.h>

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

typedef struct Player
{
    Vector2 position;
    Vector2 velocity;
    float radius;
    int health;
    float shootTimer;
    float shootCooldown;
} Player;

// Define enemy type so we can differentiate behavior.
typedef enum EnemyType
{
    ENEMY_NONE = -1, // Special value for an empty slot.
    ENEMY_GROUND = 0,
    ENEMY_FLYING = 1
} EnemyType;

typedef struct Enemy
{
    EnemyType type;
    Vector2 position;
    Vector2 velocity;
    float radius;
    int health;
    float speed;
    float leftBound;
    float rightBound;
    int direction; // 1 = right, -1 = left
    float shootTimer;
    float shootCooldown;
    // For flying enemy only (sine wave)
    float baseY;
    float waveOffset;
    float waveAmplitude;
    float waveSpeed;
} Enemy;

typedef struct GameState
{
    bool *editorMode;
    char currentLevelFilename[256] = "level1.txt";
    MemoryArena gameArena;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    Enemy bossEnemy;
    Vector2 checkpoints[MAX_CHECKPOINTS];
    int checkpointCount;
} GameState;

extern GameState *gameState;

bool SaveLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
    struct Player player, struct Enemy enemies[MAX_ENEMIES], struct Enemy bossEnemy,
    Vector2 checkpointPos);
bool LoadLevel(const char *filename, int mapTiles[MAP_ROWS][MAP_COLS],
    struct Player *player, struct Enemy enemies[MAX_ENEMIES], struct Enemy *bossEnemy,
    Vector2 checkpoints[], int *checkpointCount);
bool SaveCheckpointState(const char *filename, struct Player player,
    struct Enemy enemies[MAX_ENEMIES], struct Enemy bossEnemy,
    Vector2 checkpoints[], int checkpointCount, int currentIndex);
bool LoadCheckpointState(const char *filename, struct Player *player,
    struct Enemy enemies[MAX_ENEMIES], struct Enemy *bossEnemy,
    Vector2 checkpoints[], int *checkpointCount);
#endif