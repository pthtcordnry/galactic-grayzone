#ifndef BULLET_H
#define BULLET_H

#include <raylib.h>
#include "entity.h"

#define MAX_PLAYER_BULLETS 50
#define MAX_ENEMY_BULLETS 50
#define MAX_BULLETS (MAX_PLAYER_BULLETS + MAX_ENEMY_BULLETS)

typedef struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool fromPlayer; // true = shot by player, false = shot by enemy
} Bullet;

void SpawnBullet(Bullet bullets[], int maxBullets, bool fromPlayer, Vector2 startPos, Vector2 targetPos, float bulletSpeed);
void UpdateBullets(Bullet bullets[], int maxBullets, float deltaTime);
void HandleBulletCollisions(Bullet bullets[], int maxBullets, Entity *player, Entity *enemies, int enemyCount,
                            Entity *boss, bool *bossActive, float bulletRadius);

#endif