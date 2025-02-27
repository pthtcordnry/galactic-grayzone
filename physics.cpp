#include "physics.h"
#include "game_rendering.h"
#include <math.h>
#include <stdbool.h>
#include "tile.h"

void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius) {
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

    if (minTileX < 0) minTileX = 0;
    if (maxTileX >= currentMapWidth) maxTileX = currentMapWidth - 1;
    if (minTileY < 0) minTileY = 0;
    if (maxTileY >= currentMapHeight) maxTileY = currentMapHeight - 1;

    for (int ty = minTileY; ty <= maxTileY; ty++) {
        for (int tx = minTileX; tx <= maxTileX; tx++) {
            if (mapTiles[ty][tx] != 0) {
                int tileId = mapTiles[ty][tx];
                int tilePhysics = 0;

                if (tileId >= 0x100000) {
                    // Extract physics type from composite tile id.
                    tilePhysics = (tileId >> 16) & 0xF;
                } else {
                    // Legacy: 1 = ground, 2 = death.
                    if (tileId == 1)
                        tilePhysics = TILE_PHYS_GROUND;
                    else if (tileId == 2)
                        tilePhysics = TILE_PHYS_DEATH;
                }

                if (tilePhysics != TILE_PHYS_NONE) {
                    Rectangle tileRect = {
                        (float)(tx * TILE_SIZE),
                        (float)(ty * TILE_SIZE),
                        (float)TILE_SIZE,
                        (float)TILE_SIZE
                    };

                    if (CheckCollisionCircleRec(*pos, radius, tileRect)) {
                        if (tilePhysics == TILE_PHYS_GROUND) {
                            // Calculate overlaps in all directions.
                            float overlapLeft = (tileRect.x + tileRect.width) - (pos->x - radius);
                            float overlapRight = (pos->x + radius) - tileRect.x;
                            float overlapTop = (tileRect.y + tileRect.height) - (pos->y - radius);
                            float overlapBottom = (pos->y + radius) - tileRect.y;

                            float minOverlap = overlapLeft;
                            char axis = 'x';
                            int sign = 1;

                            if (overlapRight < minOverlap) {
                                minOverlap = overlapRight;
                                axis = 'x';
                                sign = -1;
                            }
                            if (overlapTop < minOverlap) {
                                minOverlap = overlapTop;
                                axis = 'y';
                                sign = 1;
                            }
                            if (overlapBottom < minOverlap) {
                                minOverlap = overlapBottom;
                                axis = 'y';
                                sign = -1;
                            }

                            if (axis == 'x') {
                                pos->x += sign * minOverlap;
                                vel->x = 0;
                            } else {
                                pos->y += sign * minOverlap;
                                vel->y = 0;
                            }
                        } else if (tilePhysics == TILE_PHYS_DEATH) {
                            *health = 0;
                        }
                    }
                }
            }
        }
    }
}

int GetTileAt(Vector2 pos) {
    int tileX = (int)(pos.x / TILE_SIZE);
    int tileY = (int)(pos.y / TILE_SIZE);
    if (tileX < 0 || tileX >= currentMapWidth || tileY < 0 || tileY >= currentMapHeight)
        return 0; // Out-of-bounds
    return mapTiles[tileY][tileX];
}

bool CheckTileCollision(Vector2 pos, float radius) {
    // Test the bottom center point of the entity.
    Vector2 bottom = { pos.x, pos.y + radius };
    int tileId = GetTileAt(bottom);
    if (tileId >= 0x100000) {
        int tilePhysics = (tileId >> 16) & 0xF;
        return (tilePhysics == TILE_PHYS_GROUND);
    }
    return false;
}

void UpdateEntityPhysics(Entity *e, float dt, float totalTime) {
    switch (e->physicsType) {
        case PHYS_GROUND: {
            e->velocity.y += PHYSICS_GRAVITY * dt;
            e->position.x += e->velocity.x * dt;
            e->position.y += e->velocity.y * dt;
            ResolveCircleTileCollisions(&e->position, &e->velocity, &e->health, e->radius);

            bool onGround = (fabsf(e->velocity.y) < 0.001f) || CheckTileCollision(e->position, e->radius);
            if (!onGround) {
                e->state = (e->velocity.y < 0) ? ENTITY_STATE_ASCEND : ENTITY_STATE_FALL;
            } else if (fabsf(e->velocity.x) > 0.1f) {
                e->state = ENTITY_STATE_WALK;
            } else {
                e->state = ENTITY_STATE_IDLE;
            }
            break;
        }
        case PHYS_FLYING: {
            e->position.x += e->velocity.x * dt;
            e->position.y = e->basePos.y + PHYSICS_AMPLITUDE * sinf(totalTime * PHYSICS_FREQUENCY);
            e->state = (fabsf(e->velocity.x) > 0.1f) ? ENTITY_STATE_WALK : ENTITY_STATE_IDLE;
            break;
        }
        default:
            break;
    }
}

void UpdateEntities(Entity *entities, int count, float dt, float totalTime) {
    for (int i = 0; i < count; i++) {
        UpdateEntityPhysics(&entities[i], dt, totalTime);
    }
}
