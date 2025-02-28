#include <math.h>
#include "ai.h"
#include "physics.h"
#include "game_rendering.h"

// GroundEnemyAI:
// For ground enemies, if the player is within seek distance, the enemy will
// adjust its horizontal direction to move toward the player. Otherwise, the enemy
// patrols. Additionally, the enemy checks a point at its front (at its feet)
// and turns around if that point collides with a solid tile.
void GroundEnemyAI(Entity *enemy, const Entity *player, float dt)
{
    // Compute horizontal distance from enemy to player.
    float dx = player->position.x - enemy->position.x;
    float absDx = fabsf(dx);

    // If the player is too close, stop horizontal movement.
    if (absDx < PLAYER_STOP_DISTANCE) {
        enemy->velocity.x = 0;
        return;
    }

    if (absDx < PLAYER_SEEK_DISTANCE)
    {
        // Seek the player.
        enemy->direction = (dx > 0) ? 1 : -1;
        enemy->velocity.x = enemy->speed * enemy->direction;
    }
    else
    {
        // Patrol logic.
        Vector2 frontPoint = enemy->position;
        frontPoint.x += enemy->direction * enemy->radius;
        frontPoint.y += enemy->radius * 0.5f;

        if (!CheckTileCollision(frontPoint, enemy->radius))
        {
            enemy->direction *= -1;
        }

        if (fabsf(enemy->velocity.x) < 0.1f)
        {
            enemy->direction *= -1;
        }

        if (enemy->position.x <= enemy->leftBound || enemy->position.x >= enemy->rightBound)
        {
            enemy->direction *= -1;
        }

        enemy->velocity.x = enemy->speed * enemy->direction;
    }
}



// FlyingEnemyAI:
// For flying enemies, if the player is close, they adjust their horizontal velocity
// to seek the player; otherwise, they patrol using a sinusoidal vertical pattern.
// The vertical position is set based on a sine wave using totalTime, an amplitude,
// and a frequency.
void FlyingEnemyAI(Entity *enemy, const Entity *player, float dt, float totalTime)
{
    float dx = player->position.x - enemy->position.x;
    float absDx = fabsf(dx);

    // If the player is very close, stop horizontal movement.
    if (absDx < PLAYER_STOP_DISTANCE)
    {
        enemy->velocity.x = 0;
    }
    else if (absDx < PLAYER_SEEK_DISTANCE)
    {
        enemy->direction = (dx > 0) ? 1 : -1;
        enemy->velocity.x = enemy->speed * enemy->direction;
    }
    else
    {
        enemy->velocity.x = enemy->speed * enemy->direction;
    }

    // Prevent flying enemy from leaving level bounds.
    if (enemy->position.x < 0 || enemy->position.x > LEVEL_WIDTH)
    {
        enemy->direction *= -1;
        enemy->velocity.x = enemy->speed * enemy->direction;
    }

    // Vertical sinusoidal patrol.
    float amplitude = 20.0f;
    float frequency = 2.0f;
    enemy->position.y = enemy->basePos.y + amplitude * sinf(totalTime * frequency);
}


