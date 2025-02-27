#include "bullet.h"
#include "game_rendering.h"
#include <math.h>

void SpawnBullet(Bullet bullets[], int maxBullets, bool fromPlayer, Vector2 startPos, Vector2 targetPos, float bulletSpeed)
{
    Vector2 direction = {targetPos.x - startPos.x, targetPos.y - startPos.y};
    float len = sqrtf(direction.x * direction.x + direction.y * direction.y);
    if (len > 0.0f)
    {
        direction.x /= len;
        direction.y /= len;
    }
    for (int i = 0; i < maxBullets; i++)
    {
        if (!bullets[i].active)
        {
            bullets[i].active = true;
            bullets[i].fromPlayer = fromPlayer;
            bullets[i].position = startPos;
            bullets[i].velocity.x = direction.x * bulletSpeed;
            bullets[i].velocity.y = direction.y * bulletSpeed;
            break;
        }
    }
}

void UpdateBullets(Bullet bullets[], int maxBullets, float deltaTime)
{
    for (int i = 0; i < maxBullets; i++)
    {
        if (!bullets[i].active)
            continue;
        bullets[i].position.x += bullets[i].velocity.x * deltaTime;
        bullets[i].position.y += bullets[i].velocity.y * deltaTime;
        
        // Deactivate bullet if it goes off-screen.
        if (bullets[i].position.x < 0 || bullets[i].position.x > LEVEL_WIDTH ||
            bullets[i].position.y < 0 || bullets[i].position.y > LEVEL_HEIGHT)
        {
            bullets[i].active = false;
        }
    }
}

void HandleBulletCollisions(Bullet bullets[], int maxBullets, Entity *player, Entity *enemies, int enemyCount,
                            Entity *boss, bool *bossActive, float bulletRadius)
{
    for (int i = 0; i < maxBullets; i++)
    {
        if (!bullets[i].active)
            continue;
        float bX = bullets[i].position.x;
        float bY = bullets[i].position.y;

        if (bullets[i].fromPlayer)
        {
            // Check collision with each enemy.
            for (int e = 0; e < enemyCount; e++)
            {
                Entity *enemy = &enemies[e];
                if (enemy->health <= 0)
                    continue;
                float dx = bX - enemy->position.x;
                float dy = bY - enemy->position.y;
                float dist2 = dx * dx + dy * dy;
                float combined = bulletRadius + enemy->radius;
                if (dist2 <= combined * combined)
                {
                    enemy->health--;
                    bullets[i].active = false;
                    break;
                }
            }
            // Check collision with the boss.
            if (*bossActive && boss && boss->health > 0 && bullets[i].active)
            {
                float dx = bX - boss->position.x;
                float dy = bY - boss->position.y;
                float dist2 = dx * dx + dy * dy;
                float combined = bulletRadius + boss->radius;
                if (dist2 <= combined * combined)
                {
                    boss->health--;
                    bullets[i].active = false;
                    if (boss->health <= 0)
                    {
                        *bossActive = false;
                        // Optionally set game state to GAME_OVER here if needed.
                    }
                }
            }
        }
        else
        {
            // Bullet from enemy => check collision with the player.
            float dx = bX - player->position.x;
            float dy = bY - player->position.y;
            float dist2 = dx * dx + dy * dy;
            float combined = bulletRadius + player->radius;
            if (dist2 <= combined * combined)
            {
                bullets[i].active = false;
                player->health--;
            }
        }
    }
}
