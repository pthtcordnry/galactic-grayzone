#ifndef ENTITY_H
#define ENTITY_H

#include <raylib.h>
#include <string.h>

// New entity asset types and globals:
typedef enum EntityKind {
    EMPTY = 0,
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_BOSS
} EntityKind;

// Define enemy type so we can differentiate behavior.
typedef enum PhysicsType
{
    NONE = -1, // Special value for an empty slot.
    GROUND = 0,
    FLYING = 1
} PhysicsType;

typedef struct Entity
{
    char name[64];
    EntityKind kind;
    PhysicsType physicsType;
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

static const char* GetEntityKindString(int kind) {
    switch (kind) {
        case ENTITY_ENEMY:  return "Enemy";
        case ENTITY_PLAYER: return "Player";
        case ENTITY_BOSS:   return "Boss";
        default:            return "Unknown";
    }
}

static EntityKind GetEntityKindFromString(const char *kindStr) {
    if (strcmp(kindStr, "Enemy") == 0)
        return ENTITY_ENEMY;
    else if (strcmp(kindStr, "Player") == 0)
        return ENTITY_PLAYER;
    else if (strcmp(kindStr, "Boss") == 0)
        return ENTITY_BOSS;
    return ENTITY_ENEMY; // default value
}

#endif