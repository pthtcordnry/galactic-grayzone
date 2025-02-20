/*******************************************************************************************
 * Raylib + dearImGui + rlImGui sample platformer
 ********************************************************************************************/

#include <imgui.h>
#include "main.h"
#include "game_storage.h"

// If built with EDITOR_BUILD, editorMode = true by default; else false.
#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

// Tilemap data: 0 = empty, 1 = solid, 2 = death
int mapTiles[MAP_ROWS][MAP_COLS] = {0};

// Global references for loaded assets / levels
int entityAssetCount = 0;
int levelFileCount = 0;
char (*levelFiles)[MAX_PATH_NAME] = NULL;
Camera2D camera;

EntityAsset *entityAssets = NULL;
GameState *gameState = NULL;

static bool checkpointActivated[MAX_CHECKPOINTS] = {false};

typedef struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    bool active;
    bool fromPlayer; // true = shot by player, false = shot by enemy
} Bullet;

typedef struct Particle
{
    Vector2 position;
    Vector2 velocity;
    float life; // frames remaining
    Color color;
} Particle;

static Particle particles[MAX_PARTICLES];

// Forward declarations for any static (local) functions.
static void InitParticle(Particle *p);
static void UpdateAndDrawFireworks(void);
static void DrawTilemap(const Camera2D &cam);
static void ResolveCircleTileCollisions(Vector2 *pos, Vector2 *vel, int *health, float radius);

int main(void)
{
    // Initialize the window.
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer Test");
    SetTargetFPS(60);

    // Setup ImGui + docking
    rlImGuiSetup(true);
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Allocate and init the main game state.
    gameState = (GameState *)malloc(sizeof(GameState));
    if (!gameState)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for gameState");
        return 1;
    }
    memset(gameState, 0, sizeof(GameState));
    arena_init(&gameState->gameArena, GAME_ARENA_SIZE);

    // Default the level filename
    strncpy(gameState->currentLevelFilename, "level1.txt", sizeof(gameState->currentLevelFilename));
    gameState->editorMode = &editorMode;

    // Initialize music and sound
    InitAudioDevice();
    Music music = LoadMusicStream("resources/music.mp3");
    Music victoryMusic = LoadMusicStream("resources/victory.mp3");
    Sound defeatSound = LoadSound("resources/defeat.mp3");
    Sound shotSound = LoadSound("resources/shot.mp3");

    Music *currentTrack = &music;

    // Editor defaults
    camera.target = (Vector2){LEVEL_WIDTH / 2.0f, LEVEL_HEIGHT / 2.0f};
    camera.offset = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Boss logic
    bool bossActive = false;
    bool gameWon = false;
    int bossMeleeFlash = 0;
    float enemyShootRange = 300.0f;

    // Bullets
    Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 10.0f;
    const float bulletRadius = 5.0f;

    // Load entity assets
    if (LoadEntityAssets("./assets", &entityAssets, &entityAssetCount))
    {
        TraceLog(LOG_INFO, "Successfully loaded %d entity assets.", entityAssetCount);
    }
    else
    {
        TraceLog(LOG_ERROR, "Failed to load entity assets from ./assets");
    }

    // If not in editor mode, load a level
    if (!editorMode && gameState->currentLevelFilename[0] != '\0')
    {
        if (!LoadLevel(gameState->currentLevelFilename,
                       mapTiles,
                       &gameState->player,
                       &gameState->enemies,
                       &gameState->enemyCount,
                       &gameState->bossEnemy,
                       &gameState->checkpoints,
                       &gameState->checkpointCount))
        {
            TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
        }

        // Also load checkpoint state if exists
        if (!LoadCheckpointState(CHECKPOINT_FILE,
                                 &gameState->player,
                                 &gameState->enemies,
                                 &gameState->bossEnemy,
                                 gameState->checkpoints,
                                 &gameState->checkpointCount))
        {
            TraceLog(LOG_WARNING, "No valid checkpoint state found.");
        }
    }

    // Main loop
    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();
        Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

        if (editorMode)
        {
            // Editor drawing
            BeginDrawing();
            ClearBackground(RAYWHITE);

            BeginMode2D(camera);
            DrawTilemap(camera);

            // Draw checkpoints, enemies, boss, player as needed in editor
            if (gameState->checkpoints)
            {
                for (int i = 0; i < gameState->checkpointCount; i++)
                {
                    DrawRectangle(gameState->checkpoints[i].x,
                                  gameState->checkpoints[i].y,
                                  TILE_SIZE, TILE_SIZE * 2,
                                  Fade(GREEN, 0.3f));
                }
            }
            if (gameState->enemies)
            {
                for (int i = 0; i < gameState->enemyCount; i++)
                {
                    if (gameState->enemies[i].asset->physicsType == PHYS_GROUND)
                    {
                        float halfSide = gameState->enemies[i].radius;
                        DrawRectangle((int)(gameState->enemies[i].position.x - halfSide),
                                      (int)(gameState->enemies[i].position.y - halfSide),
                                      (int)(halfSide * 2),
                                      (int)(halfSide * 2),
                                      RED);
                    }
                    else if (gameState->enemies[i].asset->physicsType == PHYS_FLYING)
                    {
                        DrawPoly(gameState->enemies[i].position,
                                 4,
                                 gameState->enemies[i].radius,
                                 45.0f,
                                 ORANGE);
                    }
                    // Draw left/right bounds lines for clarity
                    DrawLine((int)gameState->enemies[i].leftBound, 0,
                             (int)gameState->enemies[i].leftBound, 20, BLUE);
                    DrawLine((int)gameState->enemies[i].rightBound, 0,
                             (int)gameState->enemies[i].rightBound, 20, BLUE);
                }
            }
            if (gameState->bossEnemy)
            {
                DrawCircleV(gameState->bossEnemy->position,
                            gameState->bossEnemy->radius,
                            PURPLE);
            }
            if (gameState->player)
            {
                DrawCircleV(gameState->player->position,
                            gameState->player->radius,
                            BLUE);
                DrawText("PLAYER",
                         (int)(gameState->player->position.x - 20),
                         (int)(gameState->player->position.y -
                               gameState->player->radius - 20),
                         12,
                         BLACK);
            }

            EndMode2D();

            // Draw editor UI
            DrawEditor();

            EndDrawing();
            continue;
        }

        // GAME MODE ------------------------------------------------------------------

        if (gameState->player == NULL)
        {
            // If no player allocated, we can't continue.
            break;
        }

        // Update the camera
        camera.target = gameState->player->position;
        camera.rotation = 0.0f;
        camera.zoom = 0.66f;

        if (!IsMusicStreamPlaying(*currentTrack) && gameState->player->health > 0)
            PlayMusicStream(*currentTrack);

        // Update music
        UpdateMusicStream(*currentTrack);

        // Player logic
        if (gameState->player->health > 0)
        {
            // Horizontal movement
            gameState->player->velocity.x = 0;
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
                gameState->player->velocity.x = -4.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
                gameState->player->velocity.x = 4.0f;

            // Jump
            if (IsKeyPressed(KEY_SPACE))
            {
                // Basic check so we can't double-jump
                if (fabsf(gameState->player->velocity.y) < 0.001f)
                    gameState->player->velocity.y = -8.0f;
            }

            // Gravity
            gameState->player->velocity.y += 0.4f;

            // Integrate
            gameState->player->position.x += gameState->player->velocity.x;
            gameState->player->position.y += gameState->player->velocity.y;

            // Collisions with the tilemap
            ResolveCircleTileCollisions(&gameState->player->position,
                                        &gameState->player->velocity,
                                        &gameState->player->health,
                                        gameState->player->radius);
        }

        // Check checkpoint collisions
        for (int i = 0; i < gameState->checkpointCount; i++)
        {
            Rectangle cpRect = {gameState->checkpoints[i].x,
                                gameState->checkpoints[i].y,
                                TILE_SIZE,
                                TILE_SIZE * 2};
            if (!checkpointActivated[i] &&
                CheckCollisionPointRec(gameState->player->position, cpRect))
            {
                // Save checkpoint
                if (SaveCheckpointState(CHECKPOINT_FILE,
                                        *gameState->player,
                                        gameState->enemies,
                                        *gameState->bossEnemy,
                                        gameState->checkpoints,
                                        gameState->checkpointCount,
                                        i))
                {
                    TraceLog(LOG_INFO, "Checkpoint saved (index %d).", i);
                }
                checkpointActivated[i] = true;
                break;
            }
        }

        // Player shooting
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (!bullets[i].active)
                {
                    bullets[i].active = true;
                    bullets[i].fromPlayer = true;
                    bullets[i].position = gameState->player->position;

                    Vector2 dir = {screenPos.x - bullets[i].position.x,
                                   screenPos.y - bullets[i].position.y};
                    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if (len > 0.0f)
                    {
                        dir.x /= len;
                        dir.y /= len;
                    }
                    bullets[i].velocity.x = dir.x * bulletSpeed;
                    bullets[i].velocity.y = dir.y * bulletSpeed;
                    break;
                }
            }
            PlaySound(shotSound);
        }

        // Enemy logic
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            Entity *e = &gameState->enemies[i];
            if (e->health <= 0)
                continue;

            if (e->asset->physicsType == PHYS_GROUND)
            {
                e->velocity.x = e->direction * e->speed;
                e->velocity.y += 0.4f; // gravity
                e->position.x += e->velocity.x;
                e->position.y += e->velocity.y;

                ResolveCircleTileCollisions(&e->position,
                                            &e->velocity,
                                            &e->health,
                                            e->radius);

                // Patrol logic
                if ((e->leftBound != 0 || e->rightBound != 0))
                {
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
                }
            }
            else if (e->asset->physicsType == PHYS_FLYING)
            {
                // Horizontal movement
                if ((e->leftBound != 0 || e->rightBound != 0))
                {
                    e->position.x += e->direction * e->speed;
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
                }
                // Sine wave vertical
                e->waveOffset += e->waveSpeed;
                e->position.y = e->baseY +
                                sinf(e->waveOffset) * e->waveAmplitude;
            }

            // Enemy shooting
            e->shootTimer += 1.0f;
            if (gameState->player->health > 0)
            {
                float dx = gameState->player->position.x - e->position.x;
                float dy = gameState->player->position.y - e->position.y;
                float dist2 = dx * dx + dy * dy;

                if (dist2 < (enemyShootRange * enemyShootRange))
                {
                    if (e->shootTimer >= e->shootCooldown)
                    {
                        for (int b = 0; b < MAX_BULLETS; b++)
                        {
                            if (!bullets[b].active)
                            {
                                bullets[b].active = true;
                                bullets[b].fromPlayer = false;
                                bullets[b].position = e->position;
                                float len = sqrtf(dx * dx + dy * dy);
                                Vector2 dir = {0, 0};
                                if (len > 0.0f)
                                {
                                    dir.x = dx / len;
                                    dir.y = dy / len;
                                }
                                bullets[b].velocity.x = dir.x * bulletSpeed;
                                bullets[b].velocity.y = dir.y * bulletSpeed;
                                break;
                            }
                        }
                        PlaySound(shotSound);
                        e->shootTimer = 0.0f;
                    }
                }
            }
        }

        // Check if all normal enemies are dead => boss spawns
        bool anyEnemiesAlive = false;
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            if (gameState->enemies[i].health > 0)
            {
                anyEnemiesAlive = true;
                break;
            }
        }
        if (!anyEnemiesAlive && !bossActive && gameState->bossEnemy)
        {
            if (gameState->bossEnemy->health > 0)
            {
                bossActive = true;
                gameState->bossEnemy->shootTimer = 0;
            }
        }

        // Boss logic
        if (bossActive && gameState->bossEnemy)
        {
            Entity *boss = gameState->bossEnemy;
            boss->shootTimer += 1.0f;

            if (boss->health >= (BOSS_MAX_HEALTH * 0.5f))
            {
                // "Phase 1": ground
                boss->asset->physicsType = PHYS_GROUND;
                boss->speed = 2.0f;
                boss->velocity.x = boss->direction * boss->speed;
                boss->velocity.y += 0.4f;

                boss->position.x += boss->velocity.x;
                boss->position.y += boss->velocity.y;

                ResolveCircleTileCollisions(&boss->position,
                                            &boss->velocity,
                                            &boss->health,
                                            boss->radius);

                // Melee check
                float dx = gameState->player->position.x - boss->position.x;
                float dy = gameState->player->position.y - boss->position.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < boss->radius + gameState->player->radius + 10.0f)
                {
                    if (boss->shootTimer >= boss->shootCooldown)
                    {
                        gameState->player->health -= 1;
                        boss->shootTimer = 0;
                        bossMeleeFlash = 10;
                    }
                }
            }
            else if (boss->health >= (BOSS_MAX_HEALTH * 0.2f))
            {
                // "Phase 2": flying single shots
                boss->asset->physicsType = PHYS_FLYING;
                boss->speed = 4.0f;
                boss->waveOffset += boss->waveSpeed;
                boss->position.y = boss->baseY +
                                   sinf(boss->waveOffset) * boss->waveAmplitude;
                boss->position.x += boss->direction * boss->speed;

                // Ranged shot
                if (boss->shootTimer >= boss->shootCooldown)
                {
                    boss->shootTimer = 0;
                    float dx = gameState->player->position.x - boss->position.x;
                    float dy = gameState->player->position.y - boss->position.y;
                    float len = sqrtf(dx * dx + dy * dy);
                    Vector2 dir = {0, 0};
                    if (len > 0.0f)
                    {
                        dir.x = dx / len;
                        dir.y = dy / len;
                    }
                    for (int b = 0; b < MAX_BULLETS; b++)
                    {
                        if (!bullets[b].active)
                        {
                            bullets[b].active = true;
                            bullets[b].fromPlayer = false;
                            bullets[b].position = boss->position;
                            bullets[b].velocity.x = dir.x * bulletSpeed;
                            bullets[b].velocity.y = dir.y * bulletSpeed;
                            break;
                        }
                    }
                }
            }
            else
            {
                // "Phase 3": flying multi-shot
                boss->speed = 4.0f;
                boss->waveOffset += boss->waveSpeed;
                boss->position.y = boss->baseY +
                                   sinf(boss->waveOffset) * boss->waveAmplitude;
                boss->position.x += boss->direction * boss->speed;

                if (boss->shootTimer >= boss->shootCooldown)
                {
                    boss->shootTimer = 0;
                    float dx = gameState->player->position.x - boss->position.x;
                    float dy = gameState->player->position.y - boss->position.y;
                    float centerAngle = atan2f(dy, dx);
                    float fanSpread = 30.0f * DEG2RAD;
                    float spacing = fanSpread / 2.0f;

                    // 5 bullet fan
                    for (int i = -2; i <= 2; i++)
                    {
                        float angle = centerAngle + i * spacing;
                        Vector2 projDir = {cosf(angle), sinf(angle)};
                        for (int b = 0; b < MAX_BULLETS; b++)
                        {
                            if (!bullets[b].active)
                            {
                                bullets[b].active = true;
                                bullets[b].fromPlayer = false;
                                bullets[b].position = boss->position;
                                bullets[b].velocity.x = projDir.x * bulletSpeed;
                                bullets[b].velocity.y = projDir.y * bulletSpeed;
                                break;
                            }
                        }
                    }
                }
            }

            // Bounds
            if (boss->position.x < boss->leftBound)
            {
                boss->position.x = boss->leftBound;
                boss->direction = 1;
            }
            else if (boss->position.x > boss->rightBound)
            {
                boss->position.x = boss->rightBound;
                boss->direction = -1;
            }
        }

        // Update bullets
        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (!bullets[i].active)
                continue;

            bullets[i].position.x += bullets[i].velocity.x;
            bullets[i].position.y += bullets[i].velocity.y;

            // Off-screen => deactivate
            if (bullets[i].position.x < 0 || bullets[i].position.x > LEVEL_WIDTH ||
                bullets[i].position.y < 0 || bullets[i].position.y > LEVEL_HEIGHT)
            {
                bullets[i].active = false;
            }
        }

        // Bullet collisions
        for (int i = 0; i < MAX_BULLETS; i++)
        {
            if (!bullets[i].active)
                continue;
            float bX = bullets[i].position.x;
            float bY = bullets[i].position.y;

            if (bullets[i].fromPlayer)
            {
                // Check enemies
                for (int e = 0; e < gameState->enemyCount; e++)
                {
                    Entity *enemy = &gameState->enemies[e];
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
                // Check boss
                if (bossActive && gameState->bossEnemy &&
                    gameState->bossEnemy->health > 0)
                {
                    float dx = bX - gameState->bossEnemy->position.x;
                    float dy = bY - gameState->bossEnemy->position.y;
                    float dist2 = dx * dx + dy * dy;
                    float combined = bulletRadius + gameState->bossEnemy->radius;
                    if (dist2 <= combined * combined)
                    {
                        gameState->bossEnemy->health--;
                        bullets[i].active = false;
                        if (gameState->bossEnemy->health <= 0)
                        {
                            bossActive = false;
                            gameWon = true;
                        }
                    }
                }
            }
            else
            {
                // Enemy bullet => check player
                float dx = bX - gameState->player->position.x;
                float dy = bY - gameState->player->position.y;
                float dist2 = dx * dx + dy * dy;
                float combined = bulletRadius + gameState->player->radius;
                if (dist2 <= combined * combined)
                {
                    gameState->player->health--;
                    bullets[i].active = false;
                }
            }
        }

        // Drawing
        BeginDrawing();

        if (!gameWon)
        {
            ClearBackground(RAYWHITE);

            DrawText("GAME MODE: (ESC to exit)", 10, 10, 20, DARKGRAY);
            DrawText(TextFormat("Player Health: %d", gameState->player->health),
                     600, 10, 20, MAROON);

            BeginMode2D(camera);

            // Draw map + everything
            DrawTilemap(camera);

            if (gameState->player->health > 0)
            {
                DrawCircleV(gameState->player->position, gameState->player->radius, RED);
                DrawLineV(gameState->player->position, screenPos, GRAY);
            }
            else
            {
                // Player dead
                if (IsMusicStreamPlaying(music))
                {
                    PauseMusicStream(music);
                    PlaySound(defeatSound);
                }

                DrawCircleV(gameState->player->position, gameState->player->radius, DARKGRAY);
                DrawText("YOU DIED!", SCREEN_WIDTH / 2 - 50, 30, 30, RED);
                DrawText("Press SPACE for New Game", SCREEN_WIDTH / 2 - 100, 80, 20, DARKGRAY);

                if (checkpointActivated[0])
                {
                    DrawText("Press R to Respawn at Checkpoint",
                             SCREEN_WIDTH / 2 - 130, 110, 20, DARKGRAY);
                    if (IsKeyPressed(KEY_R))
                    {
                        if (!LoadCheckpointState(CHECKPOINT_FILE,
                                                 &gameState->player,
                                                 &gameState->enemies,
                                                 &gameState->bossEnemy,
                                                 gameState->checkpoints,
                                                 &gameState->checkpointCount))
                        {
                            TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                        }
                        else
                        {
                            TraceLog(LOG_INFO, "Checkpoint reloaded!");
                            // Clear bullets
                            for (int i = 0; i < MAX_BULLETS; i++)
                                bullets[i].active = false;
                            gameState->player->health = 5;
                            gameState->player->velocity = (Vector2){0, 0};

                            for (int e = 0; e < gameState->enemyCount; e++)
                                gameState->enemies[e].velocity = (Vector2){0, 0};

                            camera.target = gameState->player->position;
                            bossActive = false;
                            ResumeMusicStream(music);
                        }
                    }
                }
                else if (IsKeyPressed(KEY_SPACE))
                {
                    // Confirm new game
                    bool confirmNewGame = false;
                    while (!WindowShouldClose() && !confirmNewGame)
                    {
                        BeginDrawing();
                        ClearBackground(RAYWHITE);
                        DrawText("Are you sure you want to start a NEW GAME?",
                                 SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2 - 40,
                                 20, DARKGRAY);
                        DrawText("Press ENTER to confirm, or ESC to cancel.",
                                 SCREEN_WIDTH / 2 - 200, SCREEN_HEIGHT / 2,
                                 20, DARKGRAY);
                        EndDrawing();

                        if (IsKeyPressed(KEY_ENTER))
                            confirmNewGame = true;
                        else if (IsKeyPressed(KEY_ESCAPE))
                            break;
                    }
                    if (confirmNewGame)
                    {
                        remove(CHECKPOINT_FILE);
                        if (!LoadLevel(gameState->currentLevelFilename,
                                       mapTiles,
                                       &gameState->player,
                                       &gameState->enemies,
                                       &gameState->enemyCount,
                                       &gameState->bossEnemy,
                                       &gameState->checkpoints,
                                       &gameState->checkpointCount))
                        {
                            TraceLog(LOG_ERROR, "Failed to load level default state!");
                        }

                        // Reset
                        gameState->player->health = 5;
                        for (int i = 0; i < MAX_BULLETS; i++)
                            bullets[i].active = false;

                        gameState->player->velocity = (Vector2){0, 0};
                        for (int i = 0; i < gameState->enemyCount; i++)
                            gameState->enemies[i].velocity = (Vector2){0, 0};

                        camera.target = gameState->player->position;
                        bossActive = false;
                        ResumeMusicStream(music);
                    }
                }
            }

            // Draw enemies
            for (int i = 0; i < gameState->enemyCount; i++)
            {
                Entity *e = &gameState->enemies[i];
                if (e->health <= 0)
                    continue;
                if (e->asset->physicsType == PHYS_GROUND)
                {
                    float halfSide = e->radius;
                    DrawRectangle((int)(e->position.x - halfSide),
                                  (int)(e->position.y - halfSide),
                                  (int)(e->radius * 2),
                                  (int)(e->radius * 2),
                                  GREEN);
                }
                else if (e->asset->physicsType == PHYS_FLYING)
                {
                    DrawPoly(e->position, 4, e->radius, 45.0f, ORANGE);
                }
            }

            // Draw boss
            if (bossActive && gameState->bossEnemy)
            {
                Entity *boss = gameState->bossEnemy;
                DrawCircleV(boss->position, boss->radius, PURPLE);
                DrawText(TextFormat("Boss HP: %d", boss->health),
                         (int)(boss->position.x - 30),
                         (int)(boss->position.y - boss->radius - 20),
                         10, RED);

                if (bossMeleeFlash > 0)
                {
                    DrawCircleLines((int)boss->position.x,
                                    (int)boss->position.y,
                                    (int)(boss->radius + 5),
                                    RED);
                    bossMeleeFlash--;
                }
            }

            // Draw bullets
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (bullets[i].active)
                {
                    DrawCircle((int)bullets[i].position.x,
                               (int)bullets[i].position.y,
                               bulletRadius,
                               BLUE);
                }
            }

            EndMode2D();
        }
        else
        {
            // Victory screen
            if (IsMusicStreamPlaying(music))
            {
                StopMusicStream(music);
                PlayMusicStream(victoryMusic);
                currentTrack = &victoryMusic;
            }

            ClearBackground(BLACK);
            UpdateAndDrawFireworks();

            int textW = MeasureText("YOU WON", 60);
            DrawText("YOU WON",
                     SCREEN_WIDTH / 2 - textW / 2,
                     SCREEN_HEIGHT / 2 - 30,
                     60, YELLOW);
        }

        EndDrawing();
    }

    // Shutdown
    rlImGuiShutdown();

    StopMusicStream(music);
    StopSound(shotSound);
    UnloadMusicStream(music);
    UnloadSound(shotSound);

    CloseAudioDevice();

    arena_destroy(&gameState->gameArena);
    free(gameState);

    CloseWindow();
    return 0;
}

// ---------------------------------------------------------------------------
// Static Helper Functions
// ---------------------------------------------------------------------------

static void InitParticle(Particle *p)
{
    if (!p)
        return;

    p->position = (Vector2){
        (float)GetRandomValue(0, SCREEN_WIDTH),
        (float)GetRandomValue(0, SCREEN_HEIGHT / 2)};

    float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
    float speed = (float)GetRandomValue(1, 5);
    p->velocity = (Vector2){cosf(angle) * speed, sinf(angle) * speed};

    p->life = (float)GetRandomValue(60, 180);
    p->color = (Color){
        (unsigned char)GetRandomValue(100, 255),
        (unsigned char)GetRandomValue(100, 255),
        (unsigned char)GetRandomValue(100, 255),
        255};
}

static void UpdateAndDrawFireworks(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle *pt = &particles[i];

        pt->position.x += pt->velocity.x;
        pt->position.y += pt->velocity.y;
        pt->life -= 1.0f;

        if (pt->life <= 0)
            InitParticle(pt);

        // Fade out by alpha
        pt->color.a = (unsigned char)(255.0f * (pt->life / 180.0f));
        DrawCircleV(pt->position, 2, pt->color);
    }
}

static void DrawTilemap(const Camera2D &cam)
{
    float camWorldWidth = LEVEL_WIDTH / cam.zoom;
    float camWorldHeight = LEVEL_HEIGHT / cam.zoom;

    float camLeft = cam.target.x - camWorldWidth * 0.5f;
    float camRight = cam.target.x + camWorldWidth * 0.5f;
    float camTop = cam.target.y - camWorldHeight * 0.5f;
    float camBottom = cam.target.y + camWorldHeight * 0.5f;

    int minTileX = (int)(camLeft / TILE_SIZE);
    int maxTileX = (int)(camRight / TILE_SIZE);
    int minTileY = (int)(camTop / TILE_SIZE);
    int maxTileY = (int)(camBottom / TILE_SIZE);

    if (minTileX < 0)
        minTileX = 0;
    if (maxTileX >= MAP_COLS)
        maxTileX = MAP_COLS - 1;
    if (minTileY < 0)
        minTileY = 0;
    if (maxTileY >= MAP_ROWS)
        maxTileY = MAP_ROWS - 1;

    for (int y = minTileY; y <= maxTileY; y++)
    {
        for (int x = minTileX; x <= maxTileX; x++)
        {
            if (mapTiles[y][x] > 0)
            {
                DrawRectangle(x * TILE_SIZE,
                              y * TILE_SIZE,
                              TILE_SIZE,
                              TILE_SIZE,
                              (mapTiles[y][x] == 2 ? MAROON : BROWN));
            }
            else
            {
                DrawRectangleLines(x * TILE_SIZE,
                                   y * TILE_SIZE,
                                   TILE_SIZE,
                                   TILE_SIZE,
                                   LIGHTGRAY);
            }
        }
    }
}

static void ResolveCircleTileCollisions(Vector2 *pos,
                                        Vector2 *vel,
                                        int *health,
                                        float radius)
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
