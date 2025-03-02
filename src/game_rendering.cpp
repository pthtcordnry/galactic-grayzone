#include "game_rendering.h"
#include "tile.h"
#include "memory_arena.h"
#include "game_state.h"
#include "game_storage.h"
#include <math.h>

unsigned int **mapTiles;
int currentMapWidth;
int currentMapHeight;

void DrawAnimation(Animation anim, Vector2 position, float scale, int direction)
{
    // Get the current frame and compute source/destination rectangles.
    Rectangle srcRec = anim.framesData->frames[anim.currentFrame];
    Rectangle destRec = {
        position.x - (srcRec.width * scale) / 2,
        position.y - (srcRec.height * scale) / 2,
        srcRec.width * scale,
        srcRec.height * scale};

    srcRec.width *= direction;
    DrawTexturePro(anim.texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
}

unsigned int **InitializeTilemap(int width, int height)
{
    currentMapWidth = width;
    currentMapHeight = height;
    unsigned int **tilemap = (unsigned int **)arena_alloc(&gameArena, height * sizeof(unsigned int *));
    for (int i = 0; i < height; i++)
    {
        tilemap[i] = (unsigned int *)arena_alloc(&gameArena, width * sizeof(unsigned int));
        memset(tilemap[i], 0, width * sizeof(int));
    }
    return tilemap;
}

void DrawTilemap(Camera2D *cam)
{
    // Compute camera bounds in world space.
    float mapPixelWidth = currentMapWidth * TILE_SIZE;
    float mapPixelHeight = currentMapHeight * TILE_SIZE;
    float camWorldWidth = mapPixelWidth / cam->zoom;
    float camWorldHeight = mapPixelHeight / cam->zoom;
    float camLeft = cam->target.x - camWorldWidth * 0.5f;
    float camRight = cam->target.x + camWorldWidth * 0.5f;
    float camTop = cam->target.y - camWorldHeight * 0.5f;
    float camBottom = cam->target.y + camWorldHeight * 0.5f;

    // Determine visible tile range.
    int minTileX = (int)(camLeft / TILE_SIZE);
    int maxTileX = (int)(camRight / TILE_SIZE);
    int minTileY = (int)(camTop / TILE_SIZE);
    int maxTileY = (int)(camBottom / TILE_SIZE);

    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= currentMapWidth)
        maxTileX = currentMapWidth - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= currentMapHeight)
        maxTileY = currentMapHeight - 1;

    // Draw visible tiles.
    for (int y = minTileY; y <= maxTileY; y++)
    {
        for (int x = minTileX; x <= maxTileX; x++)
        {
            unsigned int tileId = mapTiles[y][x];
            if (tileId != 0)
            {
                unsigned short tsId = (tileId >> 20) & 0xFFF;
                unsigned short tilePhys = (tileId >> 16) & 0xF;
                unsigned short tileIndex = (tileId & 0xFFFF) - 1;

                Tileset *ts = NULL;
                for (int i = 0; i < tilesetCount; i++)
                {
                    if (tilesets[i].uniqueId == tsId)
                    {
                        ts = &tilesets[i];
                        break;
                    }
                }

                if (ts)
                {
                    // Use the found tileset
                    int tileCol = tileIndex % ts->tilesPerRow;
                    int tileRow = tileIndex / ts->tilesPerRow;
                    Rectangle srcRec = {
                        (float)(tileCol * ts->tileWidth),
                        (float)(tileRow * ts->tileHeight),
                        (float)ts->tileWidth,
                        (float)ts->tileHeight};
                    Rectangle destRec = {
                        (float)(x * TILE_SIZE), 
                        (float)(y * TILE_SIZE), 
                        (float)TILE_SIZE, 
                        (float)TILE_SIZE};
                    DrawTexturePro(ts->texture, srcRec, destRec, (Vector2){0, 0}, 0.0f, WHITE);
                }
            }
            else if (gameState->currentState == EDITOR)
            {
                DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, LIGHTGRAY);
            }
        }
    }
}

// Helper: Update and draw an entity's animation if valid.
static void DrawEntityAnimationIfValid(Entity *e, float deltaTime)
{
    // Select the correct animation based on the entity state.
    Animation *anim = NULL;
    switch (e->state)
    {
        case ENTITY_STATE_IDLE:   anim = &e->idle;   break;
        case ENTITY_STATE_WALK:   anim = &e->walk;   break;
        case ENTITY_STATE_ASCEND: anim = &e->ascend; break;
        case ENTITY_STATE_FALL:   anim = &e->fall;   break;
        default: break;
    }

    // Only update and draw if the animation is valid.
    if (anim != NULL &&
        anim->framesData != NULL &&
        anim->framesData->frameCount > 0)
    {
        UpdateAnimation(anim, deltaTime);
        float scale = (e->radius * 2) / anim->framesData->frames[0].height;
        DrawAnimation(*anim, e->position, scale, e->direction);
    }
}


void DrawEntities(float deltaTime, Vector2 mouseScreenPos, Entity *player, Entity *enemies, int enemyCount,
                  Entity *boss, int *bossMeleeFlash, bool bossActive)
{
    // Draw Player
    if (player != NULL && player->health > 0)
    {
        EntityAsset *asset = GetEntityAssetById(player->assetId);
        if (asset)
        {
            DrawEntityAnimationIfValid(player, deltaTime);

            if (gameState->currentState == PLAY)
            {
                // Compute normalized aim direction.
                Vector2 aimDir = {mouseScreenPos.x - player->position.x, mouseScreenPos.y - player->position.y};
                float len = sqrtf(aimDir.x * aimDir.x + aimDir.y * aimDir.y);
                if (len != 0)
                {
                    aimDir.x /= len;
                    aimDir.y /= len;
                }

                Vector2 aimEnd = {player->position.x + aimDir.x * CROSSHAIR_DISTANCE,
                                  player->position.y + aimDir.y * CROSSHAIR_DISTANCE};
                DrawLineV(player->position, aimEnd, GRAY);
            }
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset for player");
        }
    }

    // Draw Enemies
    for (int i = 0; i < enemyCount; i++)
    {
        Entity *e = &enemies[i];
        if (e->health <= 0)
            continue;
        EntityAsset *asset = GetEntityAssetById(e->assetId);
        if (asset)
        {
            DrawEntityAnimationIfValid(e, deltaTime);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset for enemy");
        }
    }

    // Draw Boss
    if (boss && bossActive && boss->health > 0)
    {
        EntityAsset *asset = GetEntityAssetById(boss->assetId);
        if (asset)
        {
            DrawEntityAnimationIfValid(boss, deltaTime);
        }
        else
        {
            TraceLog(LOG_ERROR, "Failed to load asset for boss");
        }
    }
}

void DrawCheckpoints(Texture2D checkpointReady, Texture2D checkpointActivated, Vector2 *checkpoints, int checkpointCount, int currentIndex)
{
    for (int i = 0; i < checkpointCount; i++)
    {
        Rectangle srcRec = {
            0, 0,
            (float)checkpointReady.width,
            (float)checkpointReady.height};

        Rectangle destRec = {
            gameState->checkpoints[i].x,
            gameState->checkpoints[i].y,
            (float)TILE_SIZE,
            (float)(TILE_SIZE * 2)};

        bool activated = currentIndex >= i;
        Texture2D tx = activated ? checkpointActivated : checkpointReady;
        DrawTexturePro(
            tx,
            srcRec,
            destRec,
            (Vector2){0, 0},
            0.0f,
            WHITE);
    }
}

static void InitParticle(Particle *p)
{
    if (!p)
        return;
    p->position = (Vector2){(float)GetRandomValue(0, SCREEN_WIDTH),
                            (float)GetRandomValue(0, SCREEN_HEIGHT / 2)};
    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    float speed = (float)GetRandomValue(1, 5);
    p->velocity = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
    p->life = (float)GetRandomValue(60, 180);
    p->color = (Color){(unsigned char)GetRandomValue(100, 255),
                       (unsigned char)GetRandomValue(100, 255),
                       (unsigned char)GetRandomValue(100, 255),
                       255};
}

void UpdateAndDrawFireworks(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pt = &particles[i];
        pt->position.x += pt->velocity.x;
        pt->position.y += pt->velocity.y;
        pt->life -= 1.0f;
        if (pt->life <= 0)
            InitParticle(pt);
        pt->color.a = (unsigned char)(255.0f * (pt->life / 180.0f)); // Fade based on remaining life.
        DrawCircleV(pt->position, 2, pt->color);
    }
}
