#ifndef PHYSICS_H
#define PHYSICS_H

#include <raylib.h>
#include "entity.h"

#define PHYSICS_GRAVITY     1000.0f
#define PHYSICS_AMPLITUDE   30.0f
#define PHYSICS_FREQUENCY   2.0f
#define PLAYER_JUMP_VELOCITY -550.0f

// Resolve collisions between a circle and the tilemap.
void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius);

// Check collision at the bottom center of an entity.
bool CheckTileCollision(Vector2 pos, float radius);

// Update an entity's physics (position, velocity, and state).
void UpdateEntityPhysics(Entity *e, float dt, float totalTime);

// Update an array of entities.
void UpdateEntities(Entity *entities, int count, float dt, float totalTime);

#endif
