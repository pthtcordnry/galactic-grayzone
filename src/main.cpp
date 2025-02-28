/*******************************************************************************************
 * Raylib + dearImGui + rlImGui Sample Platformer
 *******************************************************************************************/

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "game_storage.h"
#include "game_rendering.h"
#include "physics.h"
#include "tile.h"
#include "ai.h"
#include "game_ui.h"
#include "bullet.h"
#include "editor_mode.h"

// Editor mode flag: true if built with EDITOR_BUILD.
#ifdef EDITOR_BUILD
bool editorMode = true;
#else
bool editorMode = false;
#endif

int entityAssetCount = 0;
int levelFileCount = 0;
char (*levelFiles)[MAX_PATH_NAME] = NULL;
Camera2D camera;

EntityAsset *entityAssets = NULL;
GameState *gameState = NULL;

int main(void)
{
    // Initialize window and set target FPS.
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Platformer Test");
    SetTargetFPS(60);
    SetExitKey(0);

    // Setup ImGui with docking.
    rlImGuiSetup(true);
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Initialize memory arenas.
    arena_init(&gameArena, GAME_ARENA_SIZE);
    arena_init(&assetArena, 2 * GAME_ARENA_SIZE);

    // Allocate and initialize game state.
    gameState = (GameState *)arena_alloc(&gameArena, sizeof(GameState));
    if (!gameState)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for gameState");
        return 1;
    }
    memset(gameState, 0, sizeof(GameState));
    gameState->currentState = (editorMode) ? EDITOR : (gameState->currentLevelFilename[0] != '\0') ? PLAY
                                                                                                   : LEVEL_SELECT;

    gameState->currentCheckpointIndex = -1;

    // Initialize audio.
    InitAudioDevice();
    Music music = LoadMusicStream("res/audio/music.mp3");
    Music victoryMusic = LoadMusicStream("res/audio/victory.mp3");
    Sound defeatSound = LoadSound("res/audio/defeat.mp3");
    Sound shotSound = LoadSound("res/audio/shot.mp3");
    Music *currentTrack = &music;

    // Load level select background image.
    Texture2D levelSelectBackground = LoadTexture("./res/sprites/level_select_bg.png");
    if (levelSelectBackground.id == 0)
        TraceLog(LOG_WARNING, "Failed to load level select background image!");

    // Set default camera parameters.
    camera.target = (Vector2){LEVEL_WIDTH / 2.0f, LEVEL_HEIGHT / 2.0f};
    camera.offset = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    int jumpCount = 0;

    // Boss and bullet variables.
    bool bossActive = false;
    int bossMeleeFlash = 0;
    float enemyShootRange = 300.0f;
    Bullet bullets[MAX_BULLETS] = {0};
    const float bulletSpeed = 500.0f;
    const float bulletRadius = 5.0f;
    float totalTime = 0.0f;
    bool newGameConfirm = false;

    // Load level file list.
    LoadLevelFiles();

    // Load entity assets.
    if (!LoadEntityAssets("./res/entities/", &entityAssets, &entityAssetCount))
    {
        TraceLog(LOG_ERROR, "MAIN: Failed to load entity assets from ./res/entities");
    }
    else
    {
        for (int i = 0; i < entityAssetCount; i++)
        {
            TraceLog(LOG_INFO, "Loaded %llu asset id", entityAssets[i].id);
        }
    }

    // Load tilesets.
    if (!LoadAllTilesets("./res/tiles/", &tilesets, &tilesetCount))
        TraceLog(LOG_WARNING, "No tilesets found in ./res/tiles");
    else
        TraceLog(LOG_INFO, "Loaded %d tilesets successfully!", tilesetCount);

    bool shouldExitWindow = false;
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

        // Display UI elements based on mode.
        if (editorMode)
            DrawMainMenuBar();

        switch (gameState->currentState)
        {
        case EDITOR:
            DrawEditor();
            break;
        case LEVEL_SELECT:
        {
            // Draw level select background and title.
            DrawTexturePro(levelSelectBackground,
                           (Rectangle){0, 0, (float)levelSelectBackground.width, (float)levelSelectBackground.height},
                           (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                           (Vector2){0, 0}, 0.0f, WHITE);
            DrawText("Select a Level", GetScreenWidth() / 2 - MeasureText("Select a Level", 30) / 2, 50, 30, WHITE);

            // Display level selection buttons.
            int buttonWidth = 300, buttonHeight = 40, spacing = 10;
            int startX = GetScreenWidth() / 2 - buttonWidth / 2, startY = 100;
            for (int i = 0; i < levelFileCount; i++)
            {
                Rectangle btnRect = {(float)startX, (float)(startY + i * (buttonHeight + spacing)),
                                     (float)buttonWidth, (float)buttonHeight};
                if (DrawButton(levelFiles[i], btnRect, GRAY, BLACK, 20))
                {
                    strcpy(gameState->currentLevelFilename, levelFiles[i]);
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

                        char checkpointFile[256];
                        snprintf(checkpointFile, sizeof(checkpointFile), "%s.checkpoint", gameState->currentLevelFilename);
                        if (!LoadCheckpointState(checkpointFile, &gameState->player,
                                                 &gameState->enemies, &gameState->bossEnemy,
                                                 gameState->checkpoints, &gameState->checkpointCount, &gameState->currentCheckpointIndex))
                            TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
                        gameState->currentState = PLAY;
                    }
                }
            }
            break;
        }
        case PLAY:
        {
            if (!player)
            {
                TraceLog(LOG_FATAL, "No player allocated, we can't continue!");
                break;
            }

            // Update camera based on player position.
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

            // Player movement and jumping.
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

            // Determine if the player is on the ground.
            bool grounded = CheckTileCollision(player->position, player->radius);
            if (grounded && jumpCount > 0)
            {
                jumpCount = 0;
            }

            if (IsKeyPressed(KEY_SPACE) && jumpCount < 2)
            {
                // Allow a double jump if not grounded and jumpCount is less than 1.
                player->velocity.y = PLAYER_JUMP_VELOCITY;
                jumpCount++;
            }

            UpdateEntityPhysics(player, deltaTime, totalTime);

            // Check for checkpoint collisions.
            for (int i = 0; i < gameState->checkpointCount; i++)
            {
                Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y,
                                    TILE_SIZE, TILE_SIZE * 2};
                if (gameState->currentCheckpointIndex < i && CheckCollisionPointRec(player->position, cpRect))
                {
                    char checkpointFile[256];
                    snprintf(checkpointFile, sizeof(checkpointFile), "%s.checkpoint", gameState->currentLevelFilename);
                    if (SaveCheckpointState(checkpointFile, *player, enemies, *boss, gameState->checkpoints, gameState->checkpointCount, i))
                        TraceLog(LOG_INFO, "Checkpoint saved (index %d).", i);
                    gameState->currentCheckpointIndex = i;
                    break;
                }
            }

            // Player shooting.
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                SpawnBullet(bullets, MAX_BULLETS, true, player->position, screenPos, bulletSpeed);
                PlaySound(shotSound);
            }

            // Enemy logic.
            for (int i = 0; i < gameState->enemyCount; i++)
            {
                Entity *e = &enemies[i];
                if (e->health <= 0)
                    continue;
                if (e->physicsType == PHYS_GROUND)
                    GroundEnemyAI(e, player, deltaTime);
                else if (e->physicsType == PHYS_FLYING)
                    FlyingEnemyAI(e, player, deltaTime, totalTime);
                UpdateEntityPhysics(e, deltaTime, totalTime);
                e->shootTimer += deltaTime;
                if (player->health > 0)
                {
                    float dx = player->position.x - e->position.x;
                    float dy = player->position.y - e->position.y;
                    if ((dx * dx + dy * dy) < (enemyShootRange * enemyShootRange))
                    {
                        if (e->shootTimer >= e->shootCooldown)
                        {
                            SpawnBullet(bullets, MAX_BULLETS, false, e->position, player->position, bulletSpeed);
                            PlaySound(shotSound);
                            e->shootTimer = 0.0f;
                        }
                    }
                }
            }

            // Boss spawning logic.
            bool anyEnemiesAlive = false;
            for (int i = 0; i < gameState->enemyCount; i++)
            {
                if (enemies[i].health > 0)
                {
                    anyEnemiesAlive = true;
                    break;
                }
            }

            bossActive = !anyEnemiesAlive && boss;

            // Boss behavior.
            if (bossActive && boss->health > 0)
            {
                boss->shootTimer += deltaTime;
                if (boss->health >= (BOSS_MAX_HEALTH * 0.5f))
                {
                    boss->physicsType = PHYS_GROUND;
                    GroundEnemyAI(boss, player, deltaTime);
                    UpdateEntityPhysics(boss, deltaTime, totalTime);
                    float dx = player->position.x - boss->position.x;
                    float dy = player->position.y - boss->position.y;
                    if (sqrtf(dx * dx + dy * dy) < boss->radius + player->radius + 10.0f)
                    {
                        if (boss->shootTimer >= boss->shootCooldown * 2)
                        {
                            player->health -= 1;
                            boss->shootTimer = 0;
                            bossMeleeFlash = 10;
                        }
                    }
                }
                else if (boss->health >= (BOSS_MAX_HEALTH * 0.2f))
                {
                    boss->physicsType = PHYS_FLYING;
                    FlyingEnemyAI(boss, player, deltaTime, totalTime);
                    UpdateEntityPhysics(boss, deltaTime, totalTime);
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
                        SpawnBullet(bullets, MAX_BULLETS, false, boss->position, player->position, bulletSpeed);
                        PlaySound(shotSound);
                    }
                }
                else
                {
                    boss->physicsType = PHYS_FLYING;
                    FlyingEnemyAI(boss, player, deltaTime, totalTime);
                    UpdateEntityPhysics(boss, deltaTime, totalTime);
                    if (boss->shootTimer >= boss->shootCooldown / 2)
                    {
                        boss->shootTimer = 0;
                        float centerAngle = atan2f(player->position.y - boss->position.y,
                                                   player->position.x - boss->position.x);
                        float fanSpread = 30.0f * DEG2RAD;
                        float spacing = fanSpread / 2.0f;
                        // Determine a target distance (arbitrary; adjust as needed)
                        float targetDistance = 100.0f;
                        for (int i = -2; i <= 2; i++)
                        {
                            float angle = centerAngle + i * spacing;
                            Vector2 target;
                            target.x = boss->position.x + cosf(angle) * targetDistance;
                            target.y = boss->position.y + sinf(angle) * targetDistance;
                            SpawnBullet(bullets, MAX_BULLETS, false, boss->position, target, bulletSpeed);
                            PlaySound(shotSound);
                        }
                    }
                }
            }

            UpdateBullets(bullets, MAX_BULLETS, deltaTime);
            HandleBulletCollisions(bullets, MAX_BULLETS, player, enemies, gameState->enemyCount, boss, &bossActive, bulletRadius);

            if (player->health <= 0)
                gameState->currentState = GAME_OVER;

            BeginMode2D(camera);
            DrawTilemap(&camera);
            DrawEntities(deltaTime, screenPos, player, enemies, gameState->enemyCount, boss, &bossMeleeFlash, bossActive);
            for (int i = 0; i < MAX_BULLETS; i++)
            {
                if (bullets[i].active)
                    DrawCircle((int)bullets[i].position.x, (int)bullets[i].position.y, bulletRadius, BLUE);
            }

            EndMode2D();
            DrawText("Health", 10, 30, 10, BLACK);
            DrawFilledBar((Vector2){(20 + MeasureText("Health", 10)), 30}, 200, 15, ((float)player->health) / MAX_PLAYER_HEALTH, BLACK, LIGHTGRAY);

            if (bossActive && boss->health > 0)
            {
                int bossBarWidth = 300;
                int bossBarHeight = 20;
                int bossBarX = GetScreenWidth() / 2 - bossBarWidth / 2;
                int bossBarY = 50; // Positioned near top-center; adjust as desired.
                DrawFilledBar((Vector2){bossBarX, bossBarY}, bossBarWidth, bossBarHeight, ((float)boss->health) / BOSS_MAX_HEALTH, DARKGRAY, RED);
                DrawText(TextFormat("Boss HP: %d", boss->health), bossBarX, bossBarY - 25, 20, BLACK);
            }
            break;
        }
        case PAUSE:
        {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
            DrawText("PAUSED", GetScreenWidth() / 2 - MeasureText("PAUSED", 40) / 2,
                     GetScreenHeight() / 2 - 120, 40, WHITE);
            if (IsKeyPressed(KEY_ESCAPE))
            {
                gameState->currentState = PLAY;
                break;
            }
            Rectangle resumeRect = {GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 - 20, 300, 50};
            Rectangle levelSelectRect = {GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 + 40, 300, 50};
            Rectangle quitRect = {GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 + 100, 300, 50};
            if (DrawButton("Resume", resumeRect, SKYBLUE, BLACK, 25))
                gameState->currentState = PLAY;
            if (DrawButton("Back to Level Select", levelSelectRect, ORANGE, BLACK, 25))
                gameState->currentState = LEVEL_SELECT;
            if (DrawButton("Quit", quitRect, RED, WHITE, 25))
                shouldExitWindow = true;
            break;
        }
        case GAME_OVER:
        {
            if (player->health <= 0)
            {
                if (!newGameConfirm)
                {
                    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                    DrawText("YOU DIED!", GetScreenWidth() / 2 - MeasureText("YOU DIED!", 50) / 2,
                             GetScreenHeight() / 2 - 150, 50, RED);
                    int buttonWidth = 250, buttonHeight = 50, spacing = 20;
                    int centerX = GetScreenWidth() / 2 - buttonWidth / 2;
                    int startY = GetScreenHeight() / 2 - 50;
                    if (gameState->currentCheckpointIndex >= 0)
                    {
                        Rectangle respawnRect = {centerX, startY, buttonWidth, buttonHeight};
                        if (DrawButton("Respawn (Checkpoint)", respawnRect, GREEN, BLACK, 25))
                        {
                            char checkpointFile[256];
                            snprintf(checkpointFile, sizeof(checkpointFile), "%s.checkpoint", gameState->currentLevelFilename);
                            if (!LoadCheckpointState(checkpointFile, player, &enemies, boss,
                                                     gameState->checkpoints, &gameState->checkpointCount, &gameState->currentCheckpointIndex))
                            {
                                TraceLog(LOG_ERROR, "Failed to load checkpoint state!");
                            }
                            else
                            {
                                for (int i = 0; i < MAX_BULLETS; i++)
                                    bullets[i].active = false;
                                player->health = MAX_PLAYER_HEALTH;
                                player->velocity = (Vector2){0, 0};
                                for (int i = 0; i < gameState->enemyCount; i++)
                                    enemies[i].velocity = (Vector2){0, 0};
                                camera.target = player->position;
                                bossActive = false;
                                ResumeMusicStream(music);
                                gameState->currentState = PLAY;
                            }
                        }
                    }
                    startY += buttonHeight + spacing;
                    Rectangle newGameRect = {centerX, startY, buttonWidth, buttonHeight};
                    if (DrawButton("New Game", newGameRect, ORANGE, BLACK, 25))
                    {
                        if (gameState->currentCheckpointIndex >= 0)
                            newGameConfirm = true;
                        else
                        {
                            char checkpointFile[256];
                            snprintf(checkpointFile, sizeof(checkpointFile), "%s.checkpoint", gameState->currentLevelFilename);
                            remove(checkpointFile);
                            char levelName[256];
                            strcpy(levelName, gameState->currentLevelFilename);
                            arena_reset(&gameArena);
                            gameState = (GameState *)arena_alloc(&gameArena, sizeof(GameState));
                            memset(gameState, 0, sizeof(GameState));
                            strcpy(gameState->currentLevelFilename, levelName);
                            gameState->currentCheckpointIndex = -1;
                            if (!LoadLevel(gameState->currentLevelFilename, &mapTiles,
                                           &gameState->player, &gameState->enemies, &gameState->enemyCount,
                                           &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                            {
                                TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
                            }
                            else
                            {
                                gameState->currentState = PLAY;
                            }
                        }
                    }
                    startY += buttonHeight + spacing;
                    Rectangle levelSelectRect = {centerX, startY, buttonWidth, buttonHeight};
                    if (DrawButton("Level Select", levelSelectRect, LIGHTGRAY, BLACK, 25))
                        gameState->currentState = LEVEL_SELECT;
                    startY += buttonHeight + spacing;
                    Rectangle quitRect = {centerX, startY, buttonWidth, buttonHeight};
                    if (DrawButton("Quit Game", quitRect, RED, WHITE, 25))
                        shouldExitWindow = true;
                }
                else
                {
                    // Draw a semi-transparent overlay covering the screen.
                    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));

                    const char *confirmText = "New game will erase checkpoint data! Press Y to confirm or N to cancel.";
                    int textWidth = MeasureText(confirmText, 20);
                    DrawText(confirmText, GetScreenWidth() / 2 - textWidth / 2, GetScreenHeight() / 2, 20, WHITE);

                    // Wait for confirmation keys.
                    if (IsKeyPressed(KEY_Y))
                    {
                        newGameConfirm = false;
                        char checkpointFile[256];
                        snprintf(checkpointFile, sizeof(checkpointFile), "%s.checkpoint", gameState->currentLevelFilename);
                        remove(checkpointFile);
                        char levelName[256];
                        strcpy(levelName, gameState->currentLevelFilename);
                        arena_reset(&gameArena);
                        gameState = (GameState *)arena_alloc(&gameArena, sizeof(GameState));
                        memset(gameState, 0, sizeof(GameState));
                        strcpy(gameState->currentLevelFilename, levelName);
                        gameState->currentCheckpointIndex = -1;
                        if (!LoadLevel(gameState->currentLevelFilename, &mapTiles,
                                       &gameState->player, &gameState->enemies, &gameState->enemyCount,
                                       &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                        {
                            TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
                        }
                        else
                        {
                            gameState->currentState = PLAY;
                        }
                    }
                    else if (IsKeyPressed(KEY_N))
                    {
                        // Cancel: clear the confirmation flag.
                        newGameConfirm = false;
                    }
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

                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                DrawText("YOU WON", GetScreenWidth() / 2 - MeasureText("YOU WON", 50) / 2,
                         GetScreenHeight() / 2 - 100, 50, YELLOW);
                int buttonWidth = 250, buttonHeight = 50, spacing = 20;
                int centerX = GetScreenWidth() / 2 - buttonWidth / 2;
                int startY = GetScreenHeight() / 2;
                Rectangle levelSelectRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("Level Select", levelSelectRect, LIGHTGRAY, BLACK, 25))
                    gameState->currentState = LEVEL_SELECT;
                startY += buttonHeight + spacing;
                Rectangle quitRect = {centerX, startY, buttonWidth, buttonHeight};
                if (DrawButton("Quit Game", quitRect, RED, WHITE, 25))
                    shouldExitWindow = true;
            }
            break;
        }
        case UNINITIALIZED:
            DrawText("UH OH: Game Uninitialized!", GetScreenWidth() / 2 - MeasureText("UH OH: Game Uninitialized!", 30) / 2,
                     GetScreenHeight() / 2 - 20, 30, RED);
            break;
        }

        rlImGuiEnd();
        EndDrawing();
    }

    // Shutdown and cleanup.
    rlImGuiShutdown();
    UnloadTexture(levelSelectBackground);
    ClearTextureCache();
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
