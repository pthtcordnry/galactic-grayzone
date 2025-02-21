#ifndef PHYSICS_H
#define PHYSICS_H

#include <raylib.h>
#include "entity.h"

#define PHYSICS_GRAVITY 400.0f

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius);
void UpdateEntities(Entity *entities, int count, float dt, float totalTime);
#endif