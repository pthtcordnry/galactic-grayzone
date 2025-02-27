#include <math.h>
#include "ai.h"
#include "physics.h"

//-----------------------------------------------------------------------------
// GroundEnemyAI:
// For ground enemies, if the player is within seek distance, the enemy will
// adjust its horizontal direction to move toward the player. Otherwise, the enemy
// patrols. Additionally, the enemy checks a point at its front (at its feet)
// and turns around if that point collides with a solid tile.
//-----------------------------------------------------------------------------
void GroundEnemyAI(Entity *enemy, const Entity *player, float dt)
{
    // Compute horizontal distance from enemy to player.
    float dx = player->position.x - enemy->position.x;
    float absDx = fabsf(dx);

    if (absDx < PLAYER_SEEK_DISTANCE)
    {
        // Seek the player.
        enemy->direction = (dx > 0) ? 1 : -1;
        enemy->velocity.x = enemy->speed * enemy->direction;
    }
    else
    {
        // Patrol logic: Check the tile in front of the enemy's "feet".
        Vector2 frontPoint = enemy->position;
        // Shift horizontally in the direction of travel.
        frontPoint.x += enemy->direction * enemy->radius;
        // Lower the point to approximate the enemy's feet.
        frontPoint.y += enemy->radius * 0.5f;

        // If a collision is detected at the front point, reverse direction.
        if (CheckTileCollision(frontPoint, enemy->radius))
        {
            enemy->direction *= -1;
        }
        enemy->velocity.x = enemy->speed * enemy->direction;
    }
}

//-----------------------------------------------------------------------------
// FlyingEnemyAI:
// For flying enemies, if the player is close, they adjust their horizontal velocity
// to seek the player; otherwise, they patrol using a sinusoidal vertical pattern.
// The vertical position is set based on a sine wave using totalTime, an amplitude,
// and a frequency.
//-----------------------------------------------------------------------------
void FlyingEnemyAI(Entity *enemy, const Entity *player, float dt, float totalTime)
{
    // Compute horizontal distance from enemy to player.
    float dx = player->position.x - enemy->position.x;
    float absDx = fabsf(dx);

    if (absDx < PLAYER_SEEK_DISTANCE)
    {
        // Seek the player horizontally.
        enemy->direction = (dx > 0) ? 1 : -1;
        enemy->velocity.x = enemy->speed * enemy->direction;
    }
    else
    {
        // Patrol horizontally with current direction.
        enemy->velocity.x = enemy->speed * enemy->direction;
    }

    // Set vertical position using a sinusoidal pattern.
    // You can adjust these parameters as needed.
    float amplitude = 20.0f;       // vertical movement amplitude
    float frequency = 2.0f;        // how fast it oscillates
    enemy->position.y = enemy->basePos.y + amplitude * sinf(totalTime * frequency);
}
