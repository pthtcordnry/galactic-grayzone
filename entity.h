#ifndef ENTITY_H
#define ENTITY_H

#include <raylib.h>
#include <string.h>

typedef enum EntityKind
{
    EMPTY = 0,
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_BOSS
} EntityKind;

typedef enum PhysicsType
{
    PHYS_NONE = 0,
    PHYS_GROUND,
    PHYS_FLYING
} PhysicsType;

typedef struct EntityAsset
{
    char name[64];
    EntityKind kind;
    PhysicsType physicsType;
    float radius;
    int baseHp;
    float baseSpeed;
    float baseAttackSpeed;
} EntityAsset;

typedef struct Entity
{
    EntityAsset *asset;
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

static const char *GetEntityKindString(EntityKind kind)
{
    switch (kind)
    {
    case ENTITY_PLAYER:
        return "Player";
    case ENTITY_ENEMY:
        return "Enemy";
    case ENTITY_BOSS:
        return "Boss";
    default:
        return "Unknown";
    }
}

#endif