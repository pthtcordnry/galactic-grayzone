#ifndef ENTITY_H
#define ENTITY_H

#include <raylib.h>
#include <string.h>
#include <stdint.h>

typedef enum PhysicsType
{
    PHYS_NONE = 0,
    PHYS_GROUND,
    PHYS_FLYING
} PhysicsType;

typedef enum EntityKind
{
    EMPTY = 0,
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_BOSS
} EntityKind;

typedef struct EntityAsset
{
    uint64_t id;  
    char name[64];
    EntityKind kind;
    PhysicsType physicsType;
    float baseRadius;
    int baseHp;
    float baseSpeed;
    float baseAttackSpeed;
    // New: path to the texture file to load (relative or absolute)
    char texturePath[128];
    // New: the texture loaded from the file
    Texture2D texture;
} EntityAsset;

typedef struct Entity
{
    // asset defined variables
    uint64_t assetId;
    EntityKind kind;
    PhysicsType physicsType;
    float radius;
    int health;
    float speed;
    float shootTimer;

    // runtime variables
    Vector2 basePos; 
    Vector2 position;
    Vector2 velocity;
    float leftBound;
    float rightBound;
    int direction; // 1 = right, -1 = left
    float shootCooldown;
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
