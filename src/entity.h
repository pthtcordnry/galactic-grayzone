#ifndef ENTITY_H
#define ENTITY_H

#include <raylib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "animation.h"

// Physics types for entities.
typedef enum PhysicsType
{
    PHYS_NONE = 0,
    PHYS_GROUND,
    PHYS_FLYING
} PhysicsType;

// Animation states
typedef enum EntityState
{
    ENTITY_STATE_IDLE = 0,
    ENTITY_STATE_WALK,
    ENTITY_STATE_ASCEND,
    ENTITY_STATE_FALL,
} EntityState;

// Kinds of entities.
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
    char texturePath[128];
    Texture2D texture;
    // Animation frame definitions.
    AnimationFrames idle;
    AnimationFrames walk;
    AnimationFrames ascend;
    AnimationFrames fall;
} EntityAsset;

typedef struct Entity
{
    uint64_t assetId;
    EntityKind kind;
    PhysicsType physicsType;
    float radius;
    int health;
    float speed;
    float shootTimer;
    Vector2 basePos;
    Vector2 position;
    Vector2 velocity;
    float leftBound;
    float rightBound;
    int direction; // 1 for right, -1 for left.
    float shootCooldown;
    EntityState state;

    Animation idle;
    Animation walk;
    Animation ascend;
    Animation fall;
} Entity;

const char *GetEntityKindString(EntityKind kind);
char *EntityAssetToJSON(const EntityAsset *asset);
bool EntityAssetFromJSON(const char *json, EntityAsset *asset);
EntityAsset *GetEntityAssetById(uint64_t id);

#endif
