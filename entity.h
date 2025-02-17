#ifndef ENTITY_H
#define ENTITY_H

#include <raylib.h>

// New entity asset types and globals:
typedef enum EntityKind {
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_BOSS
} EntityKind;

// Define enemy type so we can differentiate behavior.
typedef enum EnemyType
{
    ENEMY_NONE = -1, // Special value for an empty slot.
    ENEMY_GROUND = 0,
    ENEMY_FLYING = 1
} EnemyType;

typedef struct Entity
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
} Entity;

typedef struct EntityAsset {
    char name[64];
    EntityKind kind;
    // For enemy (or boss) assets:
    EnemyType enemyType; // e.g. ENEMY_GROUND, ENEMY_FLYING; ignored for player assets.
    int health;
    float speed;
    float radius;
    float shootCooldown;
    float leftBound;
    float rightBound;
    float baseY;
    float waveAmplitude;
    float waveSpeed;
} EntityAsset;

#endif