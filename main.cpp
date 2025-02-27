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
#include "game_ui.h"

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
    SetExitKey(0);

    // Setup ImGui + docking
    rlImGuiSetup(true);
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    arena_init(&gameArena, GAME_ARENA_SIZE);
    arena_init(&assetArena, 2 * GAME_ARENA_SIZE);

    // Allocate and init the main game state.
    gameState = (GameState *)arena_alloc(&gameArena, sizeof(GameState));
    if (!gameState)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for gameState");
        return 1;
    }

    memset(gameState, 0, sizeof(GameState));
    if (!editorMode)
    {
        // If there’s no default level filename, start in level select mode.
        if (gameState->currentLevelFilename[0] != '\0')
            gameState->currentState = PLAY;
        else
            gameState->currentState = LEVEL_SELECT;
    }
    else
    {
        gameState->currentState = EDITOR;
    }

    // Initialize music and sound
    InitAudioDevice();
    Music music = LoadMusicStream("resources/music.mp3");
    Music victoryMusic = LoadMusicStream("resources/victory.mp3");
    Sound defeatSound = LoadSound("resources/defeat.mp3");
    Sound shotSound = LoadSound("resources/shot.mp3");

    Music *currentTrack = &music;

    Texture2D levelSelectBackground;
    levelSelectBackground = LoadTexture("../level_select_bg.png");
    if (levelSelectBackground.id == 0)
    {
        TraceLog(LOG_WARNING, "Failed to load level select background image!");
    }

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

    LoadLevelFiles();

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

    /// TODO: I don't think this is valid anymore.
    // If not in editor mode, load a level
    // if (!editorMode && gameState->currentLevelFilename[0] != '\0')
    // {
    //     if (!LoadLevel(gameState->currentLevelFilename,
    //                    &mapTiles,
    //                    &gameState->player,
    //                    &gameState->enemies,
    //                    &gameState->enemyCount,
    //                    &gameState->bossEnemy,
    //                    &gameState->checkpoints,
    //                    &gameState->checkpointCount))
    //     {
    //         TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
    //     }

    //     // Also load checkpoint state if exists
    //     if (!LoadCheckpointState(CHECKPOINT_FILE,
    //                              &gameState->player,
    //                              &gameState->enemies,
    //                              &gameState->bossEnemy,
    //                              gameState->checkpoints,
    //                              &gameState->checkpointCount))
    //     {
    //         TraceLog(LOG_WARNING, "No valid checkpoint state found.");
    //     }
    // }

    bool shouldExitWindow = false; 
    // Main loop
    while (!shouldExitWindow)
    {
        shouldExitWindow = WindowShouldClose();
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
        if (editorMode)
            DrawMainMenuBar();

        switch (gameState->currentState)
        {
        case EDITOR:
        {
            DrawEditor();
        }
        break;
        case LEVEL_SELECT:
        {
            // Draw the background image covering the entire screen.
            DrawTexturePro(levelSelectBackground,
                           (Rectangle){0, 0, (float)levelSelectBackground.width, (float)levelSelectBackground.height},
                           (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                           (Vector2){0, 0},
                           0.0f,
                           WHITE);

            // Draw the title text on top.
            DrawText("Select a Level", GetScreenWidth() / 2 - MeasureText("Select a Level", 30) / 2, 50, 30, DARKBLUE);

            int buttonWidth = 300, buttonHeight = 40, spacing = 10;
            int startX = GetScreenWidth() / 2 - buttonWidth / 2, startY = 100;
            for (int i = 0; i < levelFileCount; i++)
            {
                Rectangle btnRect = {(float)startX, (float)(startY + i * (buttonHeight + spacing)),
                                     (float)buttonWidth, (float)buttonHeight};
                if (DrawButton(levelFiles[i], btnRect, SKYBLUE, BLACK, 20))
                {
                    // Set the chosen level filename.
                    strcpy(gameState->currentLevelFilename, levelFiles[i]);
                    // Load level (LoadLevel will prepend "./levels/" as needed).
                    if (!LoadLevel(gameState->currentLevelFilename,
                                   &mapTiles,
                                   &gameState->player,
                                   &gameState->enemies,
                                   &gameState->enemyCount,
                                   &gameState->bossEnemy,
                                   &gameState->checkpoints,
                                   &gameState->checkpointCount))
                    {
                        TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
                    }
                    else
                    {
                        gameState->currentState = PLAY;
                    }
                }
            }
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
            UpdateMusicStream(*currentTrack);

            if (IsKeyPressed(KEY_ESCAPE))
            {
                gameState->currentState = PAUSE;
                break;
            }

            // Player logic
            if (player->health > 0)
            {
                // Set horizontal velocity from input.
                player->velocity.x = 0;
                if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
                {
                    player->direction = -1;
                    player->velocity.x = -player->speed;
                }
                else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
                {
                    player->direction = 1;
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
        case PAUSE:
        {
            // Draw a semi-transparent overlay.
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
            DrawText("PAUSED", GetScreenWidth() / 2 - MeasureText("PAUSED", 40) / 2, GetScreenHeight() / 2 - 120, 40, WHITE);

            // Allow ESC to resume while paused.
            if (IsKeyPressed(KEY_ESCAPE))
            {
                gameState->currentState = PLAY;
                break;
            }

            // Draw buttons for resume, level select, and quit.
            Rectangle resumeRect = {GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 - 20, 300, 50};
            Rectangle levelSelectRect = {GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 + 40, 300, 50};
            Rectangle quitRect = {GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 + 100, 300, 50};

            if (DrawButton("Resume", resumeRect, SKYBLUE, BLACK, 25))
            {
                gameState->currentState = PLAY;
            }
            if (DrawButton("Back to Level Select", levelSelectRect, ORANGE, BLACK, 25))
            {
                gameState->currentState = LEVEL_SELECT;
            }
            if (DrawButton("Quit", quitRect, RED, WHITE, 25))
            {
                shouldExitWindow = true;
            }
        }
        break;
        case GAME_OVER:
        {
            if (player->health <= 0) // Loss condition
            {
                // Draw a semi-transparent overlay.
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawText("YOU DIED!", GetScreenWidth() / 2 - MeasureText("YOU DIED!", 50) / 2,
                         GetScreenHeight() / 2 - 150, 50, RED);

                // Define button sizes and vertical spacing.
                int buttonWidth = 250, buttonHeight = 50, spacing = 20;
                int centerX = GetScreenWidth() / 2 - buttonWidth / 2;
                // Start Y depends on whether checkpoint is available.
                int startY = GetScreenHeight() / 2 - 50;

                // We'll always reserve space for 4 buttons.
                // Button 1: Respawn (only drawn if a checkpoint is active)
                bool respawnClicked = false;
                if (checkpointActivated[0])
                {
                    Rectangle respawnRect = {centerX, startY, buttonWidth, buttonHeight};
                    if (DrawButton("Respawn (Checkpoint)", respawnRect, GREEN, BLACK, 25))
                        respawnClicked = true;
                }
                // Move to next row (if no checkpoint, we still increment so other buttons are spaced evenly)
                startY += buttonHeight + spacing;

                // Button 2: New Game
                bool newGameClicked = false;
                Rectangle newGameRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("New Game", newGameRect, ORANGE, BLACK, 25))
                    newGameClicked = true;
                startY += buttonHeight + spacing;

                // Button 3: Level Select
                bool levelSelectClicked = false;
                Rectangle levelSelectRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("Level Select", levelSelectRect, SKYBLUE, BLACK, 25))
                    levelSelectClicked = true;
                startY += buttonHeight + spacing;

                // Button 4: Quit Game
                Rectangle quitRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("Quit Game", quitRect, RED, WHITE, 25))
                    shouldExitWindow = true;

                // Process button clicks.
                if (respawnClicked)
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
                        for (int i = 0; i < MAX_BULLETS; i++)
                            bullets[i].active = false;
                        player->health = 5;
                        player->velocity = (Vector2){0, 0};
                        for (int i = 0; i < gameState->enemyCount; i++)
                            enemies[i].velocity = (Vector2){0, 0};
                        camera.target = player->position;
                        bossActive = false;
                        ResumeMusicStream(music);
                        gameState->currentState = PLAY;
                    }
                }
                else if (newGameClicked)
                {
                    // Warn the player that starting a new game will erase checkpoint data.
                    DrawText("New game will erase checkpoint data!",
                             GetScreenWidth() / 2 - MeasureText("New game will erase checkpoint data!", 20) / 2,
                             GetScreenHeight() / 2 + 150, 20, DARKGRAY);
                    remove(CHECKPOINT_FILE);
                    if (!LoadLevel(gameState->currentLevelFilename,
                                   &mapTiles,
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
                }
                else if (levelSelectClicked)
                {
                    gameState->currentState = LEVEL_SELECT;
                }
            }
            else // Victory condition
            {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawText("YOU WON", GetScreenWidth() / 2 - MeasureText("YOU WON", 50) / 2,
                         GetScreenHeight() / 2 - 100, 50, YELLOW);
                bool levelSelectClicked = false, quitClicked = false;
                int buttonWidth = 250, buttonHeight = 50, spacing = 20;
                int centerX = GetScreenWidth() / 2 - buttonWidth / 2;
                int startY = GetScreenHeight() / 2;
                Rectangle levelSelectRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("Level Select", levelSelectRect, SKYBLUE, BLACK, 25))
                    levelSelectClicked = true;
                startY += buttonHeight + spacing;
                Rectangle quitRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("Quit Game", quitRect, RED, WHITE, 25))
                    shouldExitWindow = true;

                if (levelSelectClicked)
                {
                    gameState->currentState = LEVEL_SELECT;
                }
            }
        }
        break;
        case UNINITIALIZED:
        {
            DrawText("UH OH: Game Uninitialized!", GetScreenWidth() / 2 - MeasureText("UH OH: Game Uninitialized!", 30) / 2,
                     GetScreenHeight() / 2 - 20, 30, RED);
        }
        break;
        }

        rlImGuiEnd();
        EndDrawing();
    }

    // Shutdown
    rlImGuiShutdown();
    UnloadTexture(levelSelectBackground);

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
