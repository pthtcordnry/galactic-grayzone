#ifndef ENTITY_H
#define ENTITY_H

#include <raylib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "animation.h"

typedef enum PhysicsType
{
    PHYS_NONE = 0,
    PHYS_GROUND,
    PHYS_FLYING
} PhysicsType;

typedef enum EntityState
{
    ENTITY_STATE_IDLE = 0,
    ENTITY_STATE_WALK,
    ENTITY_STATE_ASCEND,   // New: entity is moving upward
    ENTITY_STATE_FALL,     // New: entity is moving downward (or not on ground)
    ENTITY_STATE_SHOOT,
    ENTITY_STATE_DIE
} EntityState;

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

    // Single sprite sheet for all animations
    char texturePath[128];
    Texture2D texture;

    // Frame definitions for animations
    AnimationFrames idle;
    AnimationFrames walk;
    AnimationFrames ascend;  
    AnimationFrames fall;     
    AnimationFrames shoot;
    AnimationFrames die;
} EntityAsset;

typedef struct Entity
{
    // Asset-defined properties
    uint64_t assetId;
    EntityKind kind;
    PhysicsType physicsType;
    float radius;
    int health;
    float speed;
    float shootTimer;

    // Runtime properties
    Vector2 basePos;
    Vector2 position;
    Vector2 velocity;
    float leftBound;
    float rightBound;
    int direction; // 1 = right, -1 = left
    float shootCooldown;

    // Current animation state (idle, walk, jump, shoot, die)
    EntityState state;

    // Runtime animation state
    Animation idle;
    Animation walk;
    Animation ascend;
    Animation fall;
    Animation shoot;
    Animation die;
} Entity;

void InitEntityAnimation(Animation *anim, AnimationFrames *frames, Texture2D texture);
const char *GetEntityKindString(EntityKind kind);
char *EntityAssetToJSON(const EntityAsset *asset);
bool EntityAssetFromJSON(const char *json, EntityAsset *asset);
EntityAsset *GetEntityAssetById(uint64_t id);
#endif
