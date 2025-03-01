#include "physics.h"
#include "game_rendering.h"
#include <math.h>
#include <stdbool.h>
#include "tile.h"

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius)
{
    if (!pos || !vel || !health)
        return;

    // Compute bounding box in world coords
    float left = pos->x - radius;
    float right = pos->x + radius;
    float top = pos->y - radius;
    float bottom = pos->y + radius;

    // Convert to tile coordinates
    int minTileX = (int)floorf(left / TILE_SIZE);
    int maxTileX = (int)floorf(right / TILE_SIZE);
    int minTileY = (int)floorf(top / TILE_SIZE);
    int maxTileY = (int)floorf(bottom / TILE_SIZE);

    // Clamp within the map
    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= currentMapWidth)
        maxTileX = currentMapWidth - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= currentMapHeight)
        maxTileY = currentMapHeight - 1;

    // Check each tile in bounding box
    for (int ty = minTileY; ty <= maxTileY; ty++)
    {
        for (int tx = minTileX; tx <= maxTileX; tx++)
        {
            unsigned int tileId = mapTiles[ty][tx];
            if (tileId == 0)
                continue; // Empty tile, skip

            int tilePhysics = TILE_PHYS_NONE;
            if (tileId >= 0x100000)
            {
                tilePhysics = (tileId >> 16) & 0xF;
            }

            if (tilePhysics != TILE_PHYS_NONE)
            {
                // Build the tile’s bounding box in world coordinates
                Rectangle tileRect = {
                    (float)(tx * TILE_SIZE),
                    (float)(ty * TILE_SIZE),
                    (float)TILE_SIZE,
                    (float)TILE_SIZE};

                // Check collision with entity’s circle
                if (CheckCollisionCircleRec(*pos, radius, tileRect))
                {
                    if (tilePhysics == TILE_PHYS_GROUND)
                    {
                        // Calculate overlap in 4 directions
                        float overlapLeft = (tileRect.x + tileRect.width) - (pos->x - radius);
                        float overlapRight = (pos->x + radius) - tileRect.x;
                        float overlapTop = (tileRect.y + tileRect.height) - (pos->y - radius);
                        float overlapBottom = (pos->y + radius) - tileRect.y;

                        // Figure out which overlap is smallest
                        float minOverlap = overlapLeft;
                        char axis = 'x';
                        float sign = 1.0f;

                        if (overlapRight < minOverlap)
                        {
                            minOverlap = overlapRight;
                            axis = 'x';
                            sign = -1.0f;
                        }
                        if (overlapTop < minOverlap)
                        {
                            minOverlap = overlapTop;
                            axis = 'y';
                            sign = 1.0f;
                        }
                        if (overlapBottom < minOverlap)
                        {
                            minOverlap = overlapBottom;
                            axis = 'y';
                            sign = -1.0f;
                        }

                        // Resolve along whichever axis had minimal overlap
                        if (axis == 'x')
                        {
                            pos->x += sign * minOverlap;
                            vel->x = 0.0f;
                        }
                        else
                        {
                            pos->y += sign * minOverlap;
                            vel->y = 0.0f;
                        }
                    }
                    else if (tilePhysics == TILE_PHYS_DEATH)
                    {
                        // e.g. an instant-kill tile
                        *health = 0;
                    }
                }
            }
        }
    }
}

int GetTileAt(Vector2 pos)
{
    int tileX = (int)(pos.x / TILE_SIZE);
    int tileY = (int)(pos.y / TILE_SIZE);
    if (tileX < 0 || tileX >= currentMapWidth ||
        tileY < 0 || tileY >= currentMapHeight)
    {
        return 0;
    }
    return mapTiles[tileY][tileX];
}

bool CheckTileCollision(Vector2 pos, float radius)
{
    // Test the bottom center point of the entity’s circle
    Vector2 bottom = {pos.x, pos.y + radius};
    unsigned int tileId = GetTileAt(bottom);
    if (tileId == 0)
        return false;

    // Decode tile physics for that tile
    int tilePhysics = TILE_PHYS_NONE;
    if (tileId >= 0x100000)
    {
        tilePhysics = (tileId >> 16) & 0xF;
    }
    else
    {
        if (tileId == 1)
            tilePhysics = TILE_PHYS_GROUND;
        else if (tileId == 2)
            tilePhysics = TILE_PHYS_DEATH;
    }
    return (tilePhysics == TILE_PHYS_GROUND);
}

void UpdateEntityPhysics(Entity *e, float dt, float totalTime)
{
    switch (e->physicsType)
    {
    case PHYS_GROUND:
    {
        // Apply gravity.
        e->velocity.y += PHYSICS_GRAVITY * dt;
        // Move the entity.
        e->position.x += e->velocity.x * dt;
        e->position.y += e->velocity.y * dt;
        // Resolve collisions with tiles.
        ResolveCircleTileCollisions(&e->position, &e->velocity, &e->health, e->radius);

        // Update entity state (idle, walk, etc.)
        bool onGround = (fabsf(e->velocity.y) < 0.001f) || CheckTileCollision(e->position, e->radius);
        if (!onGround)
        {
            e->state = (e->velocity.y < 0.0f) ? ENTITY_STATE_ASCEND : ENTITY_STATE_FALL;
        }
        else if (fabsf(e->velocity.x) > 0.1f)
        {
            e->state = ENTITY_STATE_WALK;
        }
        else
        {
            e->state = ENTITY_STATE_IDLE;
        }
        break;
    }
    case PHYS_FLYING:
    {
        e->position.x += e->velocity.x * dt;
        e->position.y = e->basePos.y + PHYSICS_AMPLITUDE * sinf(totalTime * PHYSICS_FREQUENCY);
        e->state = (fabsf(e->velocity.x) > 0.1f) ? ENTITY_STATE_WALK : ENTITY_STATE_IDLE;
        break;
    }
    default:
        break;
    }

    // NEW: If the entity falls below the map, kill it.
    if (e->position.y - e->radius > currentMapHeight * TILE_SIZE)
    {
        e->health = 0;
    }
}

// Loop over your enemies array, etc.
void UpdateEntities(Entity *entities, int count, float dt, float totalTime)
{
    for (int i = 0; i < count; i++)
    {
        UpdateEntityPhysics(&entities[i], dt, totalTime);
    }
}
