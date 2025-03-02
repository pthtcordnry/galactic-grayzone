#ifndef PHYSICS_H
#define PHYSICS_H

#include <raylib.h>
#include "entity.h"

#define PHYSICS_GRAVITY 1000.0f
#define PHYSICS_AMPLITUDE 30.0f
#define PHYSICS_FREQUENCY 2.0f
#define PLAYER_JUMP_VELOCITY -550.0f

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius);
bool CheckTileCollision(Vector2 pos, float radius);
void UpdateEntityPhysics(Entity *e, float dt, float totalTime);
void UpdateEntities(Entity *entities, int count, float dt, float totalTime);

#endif
