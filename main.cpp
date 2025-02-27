/*******************************************************************************************
 * Raylib + dearImGui + rlImGui sample platformer
 ********************************************************************************************/

#include <imgui.h>
#include "main.h"
#include "game_storage.h"
#include "game_rendering.h"
#include "physics.h"
#include "tile.h"
#include "ai.h"

// If built with EDITOR_BUILD, editorMode = true by default; else false.
#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

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

int main(void)
{
    // Initialize the window.
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer Test");
    SetTargetFPS(60);

    // Setup ImGui + docking
    rlImGuiSetup(true);
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    arena_init(&gameArena, GAME_ARENA_SIZE);
    arena_init(&assetArena, GAME_ARENA_SIZE);

    // Allocate and init the main game state.
    gameState = (GameState *)arena_alloc(&gameArena, sizeof(GameState));
    if (!gameState)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for gameState");
        return 1;
    }

    memset(gameState, 0, sizeof(GameState));
    InitializeTilemap(LEVEL_WIDTH / TILE_SIZE, LEVEL_HEIGHT / TILE_SIZE);

    gameState->currentState = !editorMode ? PLAY : EDITOR;

    // Default the level filename
    strncpy(gameState->currentLevelFilename, "level1.txt", sizeof(gameState->currentLevelFilename));

    // Initialize music and sound
    InitAudioDevice();
    Music music = LoadMusicStream("resources/music.mp3");
    Music victoryMusic = LoadMusicStream("resources/victory.mp3");
    Sound defeatSound = LoadSound("resources/defeat.mp3");
    Sound shotSound = LoadSound("resources/shot.mp3");

    Music *currentTrack = &music;

    // Editor Cam defaults
    camera.target = (Vector2){LEVEL_WIDTH / 2.0f, LEVEL_HEIGHT / 2.0f};
    camera.offset = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // Boss logic
    bool bossActive = false;
    int bossMeleeFlash = 0;
    float enemyShootRange = 300.0f;

    // Bullets
    Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 500.0f;
    const float bulletRadius = 5.0f;

    float totalTime = 0.0f;

    // Load entity assets
    if (!LoadEntityAssets("./assets", &entityAssets, &entityAssetCount))
    {
        TraceLog(LOG_ERROR, "Failed to load entity assets from ./assets");
    }
    else
    {
        for (int i = 0; i < entityAssetCount; i++)
        {
            TraceLog(LOG_INFO, "Loaded %llu asset id", entityAssets[i].id);
        }
    }

    if (!LoadAllTilesets("./tilesets", &tilesets, &tilesetCount))
    {
        TraceLog(LOG_WARNING, "No tilesets found in ./tilesets");
    }
    else
    {
        TraceLog(LOG_INFO, "Loaded %d tilesets successfully!", tilesetCount);
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
        float deltaTime = GetFrameTime();
        totalTime += deltaTime;

        Entity *player = &gameState->player;
        Entity *enemies = gameState->enemies;
        Entity *boss = &gameState->bossEnemy;

        Vector2 mousePos = GetMousePosition();
        Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        rlImGuiBegin();

        DrawMainMenuBar();

        switch (gameState->currentState)
        {
        case EDITOR:
        {
            DrawEditor();
        }
        break;
        case PLAY:
        {
            if (player == NULL)
            {
                TraceLog(LOG_FATAL, "No player allocated, we can't continue!");
                break;
            }

            // Update the camera
            camera.target = player->position;
            camera.rotation = 0.0f;
            camera.zoom = 0.66f;

            if (!IsMusicStreamPlaying(*currentTrack) && player->health > 0)
                PlayMusicStream(*currentTrack);

            // Update music
            UpdateMusicStream(*currentTrack);

            // Player logic
            if (player->health > 0)
            {
                // Set horizontal velocity from input.
                player->velocity.x = 0;
                if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
                {
                    player->velocity.x = -player->speed;
                }
                else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
                {
                    player->velocity.x = player->speed;
                }
                // Jump input: only allow if nearly on the ground.
                if (IsKeyPressed(KEY_SPACE) && fabsf(player->velocity.y) < 0.001f)
                {
                    player->velocity.y = PLAYER_JUMP_VELOCITY;
                }

                // Update physics (which also updates state)
                UpdateEntityPhysics(player, deltaTime, totalTime);
            }

            // Check checkpoint collisions
            for (int i = 0; i < gameState->checkpointCount; i++)
            {
                Rectangle cpRect = {gameState->checkpoints[i].x,
                                    gameState->checkpoints[i].y,
                                    TILE_SIZE,
                                    TILE_SIZE * 2};
                if (!checkpointActivated[i] &&
                    CheckCollisionPointRec(player->position, cpRect))
                {
                    // Save checkpoint
                    if (SaveCheckpointState(CHECKPOINT_FILE,
                                            *player,
                                            enemies,
                                            *boss,
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
                        bullets[i].position = player->position;

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
                Entity *e = &enemies[i];
                if (e->health <= 0)
                    continue;

                if (e->physicsType == PHYS_GROUND)
                {
                    GroundEnemyAI(e, player, deltaTime);
                }
                else if (e->physicsType == PHYS_FLYING)
                {
                    FlyingEnemyAI(e, player, deltaTime, totalTime);
                }

                // Now update the enemy's physics and state (this function uses the
                // current velocity to update position and then sets animation state).
                UpdateEntityPhysics(e, deltaTime, totalTime);

                // Enemy shooting
                e->shootTimer += deltaTime;
                if (player->health > 0)
                {
                    float dx = player->position.x - e->position.x;
                    float dy = player->position.y - e->position.y;
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
                if (enemies[i].health > 0)
                {
                    anyEnemiesAlive = true;
                    break;
                }
            }
            if (!anyEnemiesAlive && !bossActive && boss)
            {
                if (boss->health > 0)
                {
                    bossActive = true;
                    boss->shootTimer = 0;
                }
            }

            // Boss logic
            if (bossActive && boss)
            {
                boss->shootTimer += 1.0f;

                if (boss->health >= (BOSS_MAX_HEALTH * 0.5f))
                {
                    // "Phase 1": ground
                    boss->physicsType = PHYS_GROUND;
                    UpdateEntityPhysics(boss, deltaTime, totalTime);

                    // Melee check
                    float dx = player->position.x - boss->position.x;
                    float dy = player->position.y - boss->position.y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < boss->radius + player->radius + 10.0f)
                    {
                        if (boss->shootTimer >= boss->shootCooldown)
                        {
                            player->health -= 1;
                            boss->shootTimer = 0;
                            bossMeleeFlash = 10;
                        }
                    }
                }
                else if (boss->health >= (BOSS_MAX_HEALTH * 0.2f))
                {
                    // "Phase 2": flying single shots
                    boss->physicsType = PHYS_FLYING;
                    UpdateEntityPhysics(boss, deltaTime, totalTime);

                    // Ranged shot
                    if (boss->shootTimer >= boss->shootCooldown)
                    {
                        boss->shootTimer = 0;
                        float dx = player->position.x - boss->position.x;
                        float dy = player->position.y - boss->position.y;
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
                    UpdateEntityPhysics(boss, deltaTime, totalTime);
                    if (boss->shootTimer >= boss->shootCooldown)
                    {
                        boss->shootTimer = 0;
                        float dx = player->position.x - boss->position.x;
                        float dy = player->position.y - boss->position.y;
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
            }

            // Update bullets
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (!bullets[i].active)
                    continue;

                bullets[i].position.x += bullets[i].velocity.x * deltaTime;
                bullets[i].position.y += bullets[i].velocity.y * deltaTime;

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
                    // Check boss
                    if (bossActive && boss &&
                        boss->health > 0)
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
                                bossActive = false;
                                gameState->currentState = GAME_OVER;
                            }
                        }
                    }
                }
                else
                {
                    // Enemy bullet => check player
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

            // check if player alive after all physics and collision updates
            if (player->health <= 0)
                gameState->currentState = GAME_OVER;

            DrawText(TextFormat("Player Health: %d", player->health),
                     600, 10, 20, MAROON);

            BeginMode2D(camera);

            // Draw map + everything
            DrawTilemap(&camera);
            DrawEntities(deltaTime, screenPos, player, enemies, gameState->enemyCount, boss, &bossMeleeFlash, bossActive);

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
        break;
        case GAME_OVER:
        {
            if (player->health <= 0)
            {
                // Player dead
                if (IsMusicStreamPlaying(music))
                {
                    PauseMusicStream(music);
                    PlaySound(defeatSound);
                }

                // Open (or keep open) a modal popup for game over.
                ImGui::OpenPopup("Game Over");

                if (ImGui::BeginPopupModal("Game Over", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text("YOU DIED!");
                    ImGui::Separator();

                    // If a checkpoint is activated, offer a Respawn option.
                    if (checkpointActivated[0])
                    {
                        ImGui::Text("Press 'Respawn' to reload your last checkpoint.");
                        if (ImGui::Button("Respawn", ImVec2(120, 0)))
                        {
                            if (!LoadCheckpointState(CHECKPOINT_FILE,
                                                     player,
                                                     &enemies,
                                                     boss,
                                                     gameState->checkpoints,
                                                     &gameState->checkpointCount))
                            {
                                TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                            }
                            else
                            {
                                TraceLog(LOG_INFO, "Checkpoint reloaded!");
                                // Reset bullets and velocities, then update camera and resume music.
                                for (int i = 0; i < MAX_BULLETS; i++)
                                    bullets[i].active = false;
                                player->health = 5;
                                player->velocity = (Vector2){0, 0};
                                for (int i = 0; i < gameState->enemyCount; i++)
                                    enemies[i].velocity = (Vector2){0, 0};
                                camera.target = player->position;
                                bossActive = false;
                                ResumeMusicStream(music);
                                // Transition back to gameplay.
                                gameState->currentState = PLAY;
                            }
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                    }

                    // Option to start a new game regardless of checkpoint.
                    if (ImGui::Button("New Game", ImVec2(120, 0)))
                    {
                        remove(CHECKPOINT_FILE);
                        if (!LoadLevel(gameState->currentLevelFilename,
                                       mapTiles,
                                       player,
                                       &enemies,
                                       &gameState->enemyCount,
                                       boss,
                                       &gameState->checkpoints,
                                       &gameState->checkpointCount))
                        {
                            TraceLog(LOG_ERROR, "Failed to load level default state!");
                        }
                        else
                        {
                            player->health = 5;
                            for (int i = 0; i < MAX_BULLETS; i++)
                                bullets[i].active = false;
                            player->velocity = (Vector2){0, 0};
                            for (int i = 0; i < gameState->enemyCount; i++)
                                enemies[i].velocity = (Vector2){0, 0};
                            camera.target = player->position;
                            bossActive = false;
                            ResumeMusicStream(music);
                            gameState->currentState = PLAY;
                        }
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::Spacing();
                    if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        // If the player cancels, simply close the popup and remain in GAME_OVER state.
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
            else
            {
                // Victory state UI using ImGui.
                if (IsMusicStreamPlaying(music))
                {
                    StopMusicStream(music);
                    PlayMusicStream(victoryMusic);
                    currentTrack = &victoryMusic;
                }
                // Clear the background and draw fireworks.
                ClearBackground(BLACK);
                UpdateAndDrawFireworks();

                // Render a victory window overlay.
                ImGui::Begin("Victory", NULL,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
                ImGui::SetWindowPos(ImVec2(0, 0));
                ImGui::SetWindowSize(ImVec2(SCREEN_WIDTH, SCREEN_HEIGHT));
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(YELLOW.r, YELLOW.g, YELLOW.b, YELLOW.a));
                ImGui::SetCursorPos(ImVec2(SCREEN_WIDTH / 2.0f - 100, SCREEN_HEIGHT / 2.0f - 30));
                ImGui::Text("YOU WON");
                ImGui::PopStyleColor();
                ImGui::End();
            }
        }
        break;
        case UNINITIALIZED:
        {
            ClearBackground(RAYWHITE);
            // Use ImGui to display an overlay that says "UH OH"
            ImGui::Begin("Uh Oh", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::SetWindowPos(ImVec2(0, 0));
            ImGui::SetWindowSize(ImVec2(SCREEN_WIDTH, SCREEN_HEIGHT));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255)); // Red text for urgency
            ImGui::SetCursorPos(ImVec2(SCREEN_WIDTH / 2.0f - 80, SCREEN_HEIGHT / 2.0f - 20));
            ImGui::Text("UH OH: Game Uninitialized!");
            ImGui::PopStyleColor();
            ImGui::End();
        }
        break;
        }

        rlImGuiEnd();
        EndDrawing();
    }

    // Shutdown
    rlImGuiShutdown();

    StopMusicStream(music);
    StopSound(shotSound);
    UnloadMusicStream(music);
    UnloadSound(shotSound);

    CloseAudioDevice();

    arena_free(&gameArena, gameState);
    arena_destroy(&gameArena);
    arena_destroy(&assetArena);

    CloseWindow();
    return 0;
}

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
