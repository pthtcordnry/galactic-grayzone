#ifndef AI_H
#define AI_H

#include "entity.h"

#define PLAYER_SEEK_DISTANCE 400.0f

void GroundEnemyAI(Entity *enemy, const Entity *player, float dt);
void FlyingEnemyAI(Entity *enemy, const Entity *player, float dt, float totalTime);

#endif