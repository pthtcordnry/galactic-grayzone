#include "physics.h"
#include "game_rendering.h"
#include <math.h>
#include <stdbool.h>

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
                Rectangle tileRect = {
                    (float)(tx * TILE_SIZE),
                    (float)(ty * TILE_SIZE),
                    (float)TILE_SIZE,
                    (float)TILE_SIZE};

                if (CheckCollisionCircleRec(*pos, radius, tileRect))
                {
                    // Solid tile
                    if (mapTiles[ty][tx] == 1)
                    {
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
                    // Death tile
                    else if (mapTiles[ty][tx] == 2)
                    {
                        *health = 0;
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
    // Enforce patrol bounds (both for ground and flying)
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

    switch (e->physicsType)
    {
    case PHYS_GROUND:
    {
        // Set horizontal velocity based on the current speed and direction.
        e->velocity.x = e->speed * e->direction;
        // Apply gravity to vertical velocity.
        e->velocity.y += PHYSICS_GRAVITY * dt;
        break;
    }
    case PHYS_FLYING:
    {
        // For flying entities, no gravity or tile collisions.
        e->velocity.x = e->speed * e->direction;
        // Sinusoidal vertical movement:
        e->position.y += 20.0f * sinf(totalTime * 2.0f) * dt;
        break;
    }
    default:
        // For PHYS_NONE, no movement is applied.
        break;
    }

    // Predict a new position.
    Vector2 newPos = {e->position.x + e->velocity.x * dt,
                      e->position.y + e->velocity.y * dt};

    e->position = newPos;
    ResolveCircleTileCollisions(&e->position, &e->velocity, &e->health, e->radius);
}

// Helper to update an array of entities.
void UpdateEntities(Entity *entities, int count, float dt, float totalTime)
{
    for (int i = 0; i < count; i++)
    {
        UpdateEntityPhysics(&entities[i], dt, totalTime);
    }
}