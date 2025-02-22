#include "physics.h"
#include "game_rendering.h"
#include <math.h>
#include <stdbool.h>
#include "tile.h"

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius)
{
    if (!pos || !vel || !health)
        return;

    float left = pos->x - radius;
    float right = pos->x + radius;
    float top = pos->y - radius;
    float bottom = pos->y + radius;

    int minTileX = (int)(left / TILE_SIZE);
    int maxTileX = (int)(right / TILE_SIZE);
    int minTileY = (int)(top / TILE_SIZE);
    int maxTileY = (int)(bottom / TILE_SIZE);

    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= MAP_COLS)
        maxTileX = MAP_COLS - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= MAP_ROWS)
        maxTileY = MAP_ROWS - 1;

    for (int ty = minTileY; ty <= maxTileY; ty++)
    {
        for (int tx = minTileX; tx <= maxTileX; tx++)
        {
            if (mapTiles[ty][tx] != 0)
            {
                // Determine the physics behavior for this tile.
                int tileId = mapTiles[ty][tx];
                int tilePhysics = 0; // Default: no special physics.

                if (tileId >= 0x100000)
                {
                    // New composite id: extract physics type.
                    tilePhysics = (tileId >> 16) & 0xF;
                }
                else
                {
                    // Legacy: assume 1 = ground, 2 = death.
                    if (tileId == 1)
                        tilePhysics = TILE_PHYS_GROUND;
                    else if (tileId == 2)
                        tilePhysics = TILE_PHYS_DEATH;
                }

                if (tilePhysics != TILE_PHYS_NONE)
                {
                    Rectangle tileRect = {
                        (float)(tx * TILE_SIZE),
                        (float)(ty * TILE_SIZE),
                        (float)TILE_SIZE,
                        (float)TILE_SIZE};

                    if (CheckCollisionCircleRec(*pos, radius, tileRect))
                    {
                        if (tilePhysics == TILE_PHYS_GROUND)
                        {
                            // Resolve collision: compute overlaps in each direction
                            float overlapLeft = (tileRect.x + tileRect.width) - (pos->x - radius);
                            float overlapRight = (pos->x + radius) - tileRect.x;
                            float overlapTop = (tileRect.y + tileRect.height) - (pos->y - radius);
                            float overlapBottom = (pos->y + radius) - tileRect.y;

                            float minOverlap = overlapLeft;
                            char axis = 'x';
                            int sign = 1;

                            if (overlapRight < minOverlap)
                            {
                                minOverlap = overlapRight;
                                axis = 'x';
                                sign = -1;
                            }
                            if (overlapTop < minOverlap)
                            {
                                minOverlap = overlapTop;
                                axis = 'y';
                                sign = 1;
                            }
                            if (overlapBottom < minOverlap)
                            {
                                minOverlap = overlapBottom;
                                axis = 'y';
                                sign = -1;
                            }

                            if (axis == 'x')
                            {
                                pos->x += sign * minOverlap;
                                vel->x = 0;
                            }
                            else
                            {
                                pos->y += sign * minOverlap;
                                vel->y = 0;
                            }
                        }
                        else if (tilePhysics == TILE_PHYS_DEATH)
                        {
                            *health = 0;
                        }
                    }
                }
            }
        }
    }
}

// A helper function to get the tile value at a given pixel position.
int GetTileAt(Vector2 pos)
{
    int tileX = (int)(pos.x / TILE_SIZE);
    int tileY = (int)(pos.y / TILE_SIZE);
    if (tileX < 0 || tileX >= MAP_COLS || tileY < 0 || tileY >= MAP_ROWS)
        return 0; // Out-of-bounds: treat as empty (or you might want to consider it solid)
    return mapTiles[tileY][tileX];
}

// Check for collision by sampling the entity’s “feet” (bottom center).
bool CheckTileCollision(Vector2 pos, float radius)
{
    // We test the bottom center point of the entity (adjust if your entity shape is different)
    Vector2 bottom = {pos.x, pos.y + radius};
    return (GetTileAt(bottom) == 1);
}

void UpdateEntityPhysics(Entity *e, float dt, float totalTime)
{
    switch (e->physicsType)
    {
    case PHYS_GROUND:
    {
        // Enforce patrol bounds on the runtime position.
        if (e->position.x < e->leftBound)
        {
            e->position.x = e->leftBound;
            e->direction = 1;
        }
        else if (e->position.x > e->rightBound)
        {
            e->position.x = e->rightBound;
            e->direction = -1;
        }

        // Update horizontal velocity and apply gravity.
        e->velocity.x = e->speed * e->direction;
        e->velocity.y += PHYSICS_GRAVITY * dt;

        // Predict new position and update.
        Vector2 newPos = {e->position.x + e->velocity.x * dt,
                          e->position.y + e->velocity.y * dt};
        e->position = newPos;

        // Resolve collisions for ground entities.
        ResolveCircleTileCollisions(&e->position, &e->velocity, &e->health, e->radius);
        break;
    }
    case PHYS_FLYING:
    {
        // Enforce patrol bounds on the runtime horizontal position.
        if (e->position.x < e->leftBound)
        {
            e->position.x = e->leftBound;
            e->direction = 1;
        }
        else if (e->position.x > e->rightBound)
        {
            e->position.x = e->rightBound;
            e->direction = -1;
        }

        // Update horizontal velocity and runtime position.x.
        e->velocity.x = e->speed * e->direction;
        e->velocity.y = PHYSICS_AMPLITUDE * PHYSICS_FREQUENCY * cosf(totalTime * PHYSICS_FREQUENCY);

        e->position.x += e->velocity.x * dt;
        e->position.y = e->basePos.y + PHYSICS_AMPLITUDE * sinf(totalTime * PHYSICS_FREQUENCY);
        break;
    }
    default:
    {
        // For PHYS_NONE, no movement is applied.
        break;
    }
    }
}

// Helper to update an array of entities.
void UpdateEntities(Entity *entities, int count, float dt, float totalTime)
{
    for (int i = 0; i < count; i++)
    {
        UpdateEntityPhysics(&entities[i], dt, totalTime);
    }
}