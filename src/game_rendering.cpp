#include "game_rendering.h"
#include "tile.h"
#include "memory_arena.h"
#include "game_state.h"
#include "game_storage.h"
#include <math.h>

// Global tilemap data.
int **mapTiles;
int currentMapWidth;
int currentMapHeight;

void DrawAnimation(Animation anim, Vector2 position, float scale, int direction) {
    // Get the current frame and compute source/destination rectangles.
    Rectangle srcRec = anim.framesData->frames[anim.currentFrame];
    Rectangle destRec = {
        position.x - (srcRec.width * scale) / 2,
        position.y - (srcRec.height * scale) / 2,
        srcRec.width * scale,
        srcRec.height * scale
    };

    srcRec.width *= direction;
    DrawTexturePro(anim.texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
}

int **InitializeTilemap(int width, int height) {
    currentMapWidth = width;
    currentMapHeight = height;
    int **tilemap = (int **)arena_alloc(&gameArena, height * sizeof(int *));
    for (int i = 0; i < height; i++) {
        tilemap[i] = (int *)arena_alloc(&gameArena, width * sizeof(int));
        memset(tilemap[i], 0, width * sizeof(int));
    }
    return tilemap;
}

void DrawTilemap(Camera2D *cam) {
    // Compute camera bounds in world space.
    float camWorldWidth = LEVEL_WIDTH / cam->zoom;
    float camWorldHeight = LEVEL_HEIGHT / cam->zoom;
    float camLeft   = cam->target.x - camWorldWidth * 0.5f;
    float camRight  = cam->target.x + camWorldWidth * 0.5f;
    float camTop    = cam->target.y - camWorldHeight * 0.5f;
    float camBottom = cam->target.y + camWorldHeight * 0.5f;

    // Determine visible tile range.
    int minTileX = (int)(camLeft / TILE_SIZE);
    int maxTileX = (int)(camRight / TILE_SIZE);
    int minTileY = (int)(camTop / TILE_SIZE);
    int maxTileY = (int)(camBottom / TILE_SIZE);

    if (minTileX < 0) minTileX = 0;
    if (maxTileX >= currentMapWidth) maxTileX = currentMapWidth - 1;
    if (minTileY < 0) minTileY = 0;
    if (maxTileY >= currentMapHeight) maxTileY = currentMapHeight - 1;

    // Draw visible tiles.
    for (int y = minTileY; y <= maxTileY; y++) {
        for (int x = minTileX; x <= maxTileX; x++) {
            int tileId = mapTiles[y][x];
            if (tileId > 0) {
                // Process composite tile IDs.
                if (tileId >= 0x100000) {
                    int tsIndex   = ((tileId >> 20) & 0xFFF) - 1;
                    int tilePhysics = (tileId >> 16) & 0xF;
                    int tileIndex = (tileId & 0xFFFF) - 1;
                    if (tsIndex >= 0 && tsIndex < tilesetCount) {
                        Tileset *ts = &tilesets[tsIndex];
                        int tileCol = tileIndex % ts->tilesPerRow;
                        int tileRow = tileIndex / ts->tilesPerRow;
                        Rectangle srcRec = {
                            (float)(tileCol * ts->tileWidth),
                            (float)(tileRow * ts->tileHeight),
                            (float)ts->tileWidth,
                            (float)ts->tileHeight
                        };
                        Rectangle destRec = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
                        DrawTexturePro(ts->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
                    }
                }
            } else {
                // Draw grid lines for empty tiles.
                DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, LIGHTGRAY);
            }
        }
    }
}

void DrawEntities(float deltaTime, Vector2 mouseScreenPos, Entity *player, Entity *enemies, int enemyCount,
                  Entity *boss, int *bossMeleeFlash, bool bossActive) {
    // Draw Player
    if (player != NULL && player->health > 0) {
        EntityAsset *asset = GetEntityAssetById(player->assetId);
        if (asset) {
            Animation *animToPlay = NULL;
            switch (player->state) {
                case ENTITY_STATE_IDLE:   animToPlay = &player->idle;   break;
                case ENTITY_STATE_WALK:   animToPlay = &player->walk;   break;
                case ENTITY_STATE_ASCEND: animToPlay = &player->ascend; break;
                case ENTITY_STATE_FALL:   animToPlay = &player->fall;   break;
                case ENTITY_STATE_DIE:    animToPlay = &player->die;    break;
                case ENTITY_STATE_SHOOT:  animToPlay = &player->shoot;  break;
            }
            UpdateAnimation(animToPlay, deltaTime);
            float scale = (player->radius * 2) / animToPlay->framesData->frames[0].height;
            DrawAnimation(*animToPlay, player->position, scale, player->direction);
            DrawLineV(player->position, mouseScreenPos, GRAY);
        } else {
            TraceLog(LOG_ERROR, "Failed to load asset for player");
        }
    }

    // Draw Enemies
    for (int i = 0; i < enemyCount; i++) {
        Entity *e = &enemies[i];
        if (e->health <= 0)
            continue;
        EntityAsset *asset = GetEntityAssetById(e->assetId);
        if (asset) {
            Animation *animToPlay = NULL;
            switch (e->state) {
                case ENTITY_STATE_IDLE:   animToPlay = &e->idle;   break;
                case ENTITY_STATE_WALK:   animToPlay = &e->walk;   break;
                case ENTITY_STATE_ASCEND: animToPlay = &e->ascend; break;
                case ENTITY_STATE_FALL:   animToPlay = &e->fall;   break;
                case ENTITY_STATE_DIE:    animToPlay = &e->die;    break;
                case ENTITY_STATE_SHOOT:  animToPlay = &e->shoot;  break;
            }
            UpdateAnimation(animToPlay, deltaTime);
            float scale = (e->radius * 2) / animToPlay->framesData->frames[0].height;
            DrawAnimation(*animToPlay, e->position, scale, e->direction);
        } else {
            TraceLog(LOG_ERROR, "Failed to load asset for enemy");
        }
    }

    // Draw Boss
    if (boss && bossActive && boss->health > 0) {
        EntityAsset *asset = GetEntityAssetById(boss->assetId);
        if (asset) {
            Animation *animToPlay = NULL;
            switch (boss->state) {
                case ENTITY_STATE_IDLE:   animToPlay = &boss->idle;   break;
                case ENTITY_STATE_WALK:   animToPlay = &boss->walk;   break;
                case ENTITY_STATE_ASCEND: animToPlay = &boss->ascend; break;
                case ENTITY_STATE_FALL:   animToPlay = &boss->fall;   break;
                case ENTITY_STATE_DIE:    animToPlay = &boss->die;    break;
                case ENTITY_STATE_SHOOT:  animToPlay = &boss->shoot;  break;
            }
            UpdateAnimation(animToPlay, deltaTime);
            float scale = (boss->radius * 2) / animToPlay->framesData->frames[0].height;
            DrawAnimation(*animToPlay, boss->position, scale, boss->direction);
        } else {
            TraceLog(LOG_ERROR, "Failed to load asset for boss");
        }
    }
}

static void InitParticle(Particle *p) {
    if (!p)
        return;
    p->position = (Vector2){ (float)GetRandomValue(0, SCREEN_WIDTH),
                             (float)GetRandomValue(0, SCREEN_HEIGHT / 2) };
    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    float speed = (float)GetRandomValue(1, 5);
    p->velocity = (Vector2){ cosf(angle) * speed, sinf(angle) * speed };
    p->life = (float)GetRandomValue(60, 180);
    p->color = (Color){ (unsigned char)GetRandomValue(100, 255),
                        (unsigned char)GetRandomValue(100, 255),
                        (unsigned char)GetRandomValue(100, 255),
                        255 };
}

void UpdateAndDrawFireworks(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *pt = &particles[i];
        pt->position.x += pt->velocity.x;
        pt->position.y += pt->velocity.y;
        pt->life -= 1.0f;
        if (pt->life <= 0)
            InitParticle(pt);
        pt->color.a = (unsigned char)(255.0f * (pt->life / 180.0f));  // Fade based on remaining life.
        DrawCircleV(pt->position, 2, pt->color);
    }
}
