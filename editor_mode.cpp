#include "raylib.h"
#include <string.h>
#include "editor_mode.h"
#include "game_ui.h"
#include "game_state.h"
#include "file_io.h"
#include "main.h"

// State variables for menu status
bool fileMenuOpen = false;
bool toolsMenuOpen = false;
bool tilemapSubmenuOpen = false;
bool entitiesSubmenuOpen = false;
bool showFileList = false;
int selectedFileIndex = -1;

int draggedCheckpointIndex = -1;
Vector2 dragOffset = {0};

int draggedBoundEnemy = -1;  // index of enemy being bound-dragged
int boundType = -1;          // 0 = left bound, 1 = right bound
int enemyPlacementType = -1; // -1 = none; otherwise ENEMY_GROUND or ENEMY_FLYING.
int selectedEntityIndex = -1;
int enemyInspectorIndex = -1;
bool bossSelected = false; // is the boss currently selected?
bool isPainting = false;

// Menu item labels
const char *fileItems[3] = {"New", "Open", "Save"};
const char *tilemapItems[3] = {"Ground", "Death", "Eraser"};
const char *entitiesItems[3] = {"Add Ground Enemy", "Add Flying Enemy", "Add Boss Enemy"};

enum TileTool
{
    TOOL_GROUND = 0,
    TOOL_DEATH = 1,
    TOOL_ERASER = 2
};

TileTool currentTool = TOOL_GROUND;
// Allocate a contiguous block for fileCount paths.
char (*levelFiles)[256];
int levelFileCount;

void DrawEditor()
{
    Vector2 mousePos = GetMousePosition();
    Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);
    bool isOverUi = false;

    // Draw top menu bar
    Rectangle header = {0, 0, GetScreenWidth(), 30};
    DrawRectangle(header.x, header.y, header.width, header.height, LIGHTGRAY);
    Rectangle fileButton = {10, header.y, 50, header.height};
    Rectangle toolButton = {60, header.y, 75, header.height};
    if (DrawButton("File", fileButton, LIGHTGRAY, BLACK, 20))
        fileMenuOpen = !fileMenuOpen;
    if (DrawButton("Tools", toolButton, LIGHTGRAY, BLACK, 20))
        toolsMenuOpen = !toolsMenuOpen;

    isOverUi = CheckCollisionPointRec(mousePos, header);

    // Draw File dropdown if open
    if (fileMenuOpen)
    {
        Rectangle fileRect = {10, 30, 100, 3 * 30};
        if (!isOverUi)
        {
            isOverUi = CheckCollisionPointRec(mousePos, fileRect);
        }
        DrawRectangleRec(fileRect, RAYWHITE);
        DrawRectangleLines(fileRect.x, fileRect.y, fileRect.width, fileRect.height, BLACK);
        for (int i = 0; i < 3; i++)
        {
            Rectangle itemRect = {fileRect.x, fileRect.y + i * 30, fileRect.width, 30};
            // Highlight if mouse over
            if (CheckCollisionPointRec(mousePos, itemRect))
            {
                DrawRectangleRec(itemRect, Fade(BLUE, 0.5f));
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    // Process file item selection
                    if (i == 0)
                    {
                        /* New */
                        //     // Create a new blank/default tilemap.
                        for (int y = 0; y < MAP_ROWS; y++)
                        {
                            for (int x = 0; x < MAP_COLS; x++)
                            {
                                mapTiles[y][x] = 0; // Clear all tiles.
                            }
                        }
                        // Clear any existing enemy and boss data.
                        for (int i = 0; i < MAX_ENEMIES; i++)
                        {
                            gameState->enemies[i].type = ENEMY_NONE;
                            gameState->enemies[i].health = 0;
                        }
                        gameState->bossEnemy.type = ENEMY_NONE;
                    }
                    else if (i == 1)
                    {
                        /* Open */
                        const char *levelsDir = "levels";      // Adjust to your levels directory.
                        const char *levelExtension = ".level"; // For example, files ending with .level.

                        // Count how many files match.
                        levelFileCount = CountFilesWithExtension(levelsDir, levelExtension);
                        if (levelFileCount <= 0)
                        {
                            TraceLog(LOG_WARNING, "No level files found in %s", levelsDir);
                        }
                        else
                        {
                            // Allocate a contiguous block for fileCount paths.
                            levelFiles = (char(*)[256])arena_alloc(&gameState->gameArena, levelFileCount * sizeof(*levelFiles));
                            if (levelFiles == NULL)
                            {
                                TraceLog(LOG_ERROR, "Failed to allocate memory for level file list!");
                            }
                            else
                            {
                                int index = 0;
                                FillFilesWithExtensionContiguous(levelsDir, levelExtension, levelFiles, &index);

                                // Now, levelFiles[0..index-1] hold the file paths.
                                // For example, draw each file path on screen:
                                for (int i = 0; i < index; i++)
                                {
                                    TraceLog(LOG_INFO, levelFiles[i]);
                                    DrawText(levelFiles[i], 50, 100 + i * 20, 10, BLACK);
                                }
                            }
                        }

                        int currentCount = CountFilesWithExtension(levelsDir, levelExtension);

                        // Check if the file count has changed or if we haven't allocated yet:
                        if (levelFiles == NULL || currentCount != levelFileCount)
                        {
                            if (levelFiles != NULL)
                            {
                                arena_free_last(&gameState->gameArena, currentCount * sizeof(levelFiles));
                            }

                            levelFileCount = currentCount;
                            levelFiles = (char(*)[256])arena_alloc(&gameState->gameArena,
                                                                   currentCount * sizeof(levelFiles));
                        }

                        if (levelFiles == NULL)
                        {
                            TraceLog(LOG_ERROR, "Failed to allocate memory for level file list!");
                        }
                        else
                        {
                            // Fill in the file list (assuming your FillFilesWithExtensionContiguous function does that):
                            int index = 0;
                            FillFilesWithExtensionContiguous(levelsDir, levelExtension, levelFiles, &index);
                        }

                        showFileList = !showFileList;
                    }
                    else if (i == 2)
                    {
                        /* Save */
                        if (SaveLevel(gameState->currentLevelFilename, mapTiles, gameState->player, gameState->enemies, gameState->bossEnemy))
                            TraceLog(LOG_INFO, "Level saved successfully!");
                        else
                            TraceLog(LOG_ERROR, "Failed to save Level!");
                    }
                    fileMenuOpen = false;
                }
            }
            DrawText(fileItems[i], itemRect.x + 5, itemRect.y + 5, 20, BLACK);
        }
    }

    // Draw Tools dropdown if open
    if (toolsMenuOpen)
    {
        Rectangle toolsRect = {70, 30, 150, 2 * 30};
        if (!isOverUi)
        {
            isOverUi = CheckCollisionPointRec(mousePos, toolsRect);
        }

        DrawRectangleRec(toolsRect, RAYWHITE);
        DrawRectangleLines(toolsRect.x, toolsRect.y, toolsRect.width, toolsRect.height, BLACK);
        // Tools menu items: "Tilemap" and "Entities"
        if (CheckCollisionPointRec(mousePos, (Rectangle){toolsRect.x, toolsRect.y, toolsRect.width, 30}))
        {
            DrawRectangleRec((Rectangle){toolsRect.x, toolsRect.y, toolsRect.width, 30}, Fade(BLUE, 0.5f));
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                tilemapSubmenuOpen = !tilemapSubmenuOpen;
        }
        DrawText("Tilemap", toolsRect.x + 5, toolsRect.y + 5, 20, BLACK);

        if (CheckCollisionPointRec(mousePos, (Rectangle){toolsRect.x, toolsRect.y + 30, toolsRect.width, 30}))
        {
            DrawRectangleRec((Rectangle){toolsRect.x, toolsRect.y + 30, toolsRect.width, 30}, Fade(BLUE, 0.5f));
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                entitiesSubmenuOpen = !entitiesSubmenuOpen;
        }
        DrawText("Entities", toolsRect.x + 5, toolsRect.y + 35, 20, BLACK);

        // Draw Tilemap submenu
        if (tilemapSubmenuOpen)
        {
            Rectangle tmRect = {toolsRect.x + toolsRect.width, toolsRect.y, 120, 4 * 30};
            if (!isOverUi)
            {
                isOverUi = CheckCollisionPointRec(mousePos, tmRect);
            }

            DrawRectangleRec(tmRect, RAYWHITE);
            DrawRectangleLines(tmRect.x, tmRect.y, tmRect.width, tmRect.height, BLACK);
            for (int i = 0; i < 3; i++)
            {
                Rectangle itemRect = {tmRect.x, tmRect.y + i * 30, tmRect.width, 30};
                if (CheckCollisionPointRec(mousePos, itemRect))
                {
                    DrawRectangleRec(itemRect, Fade(BLUE, 0.5f));
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    {
                        // Process tilemap action for tilemapItems[i]
                        if (i == 0)
                        {
                            currentTool = TOOL_GROUND;
                            enemyPlacementType = -1;
                        }
                        else if (i == 1)
                        {
                            currentTool = TOOL_DEATH;
                            enemyPlacementType = -1;
                        }
                        else if (i == 2)
                        {
                            currentTool = TOOL_ERASER;
                            enemyPlacementType = -1;
                        }

                        tilemapSubmenuOpen = false;
                        toolsMenuOpen = false;
                    }
                }
                DrawText(tilemapItems[i], itemRect.x + 5, itemRect.y + 5, 20, BLACK);
            }
        }

        // Draw Entities submenu
        if (entitiesSubmenuOpen)
        {
            Rectangle entRect = {toolsRect.x + toolsRect.width, toolsRect.y + 30, 180, 4 * 30};
            if (!isOverUi)
            {
                isOverUi = CheckCollisionPointRec(mousePos, entRect);
            }

            DrawRectangleRec(entRect, RAYWHITE);
            DrawRectangleLines(entRect.x, entRect.y, entRect.width, entRect.height, BLACK);
            for (int i = 0; i < 3; i++)
            {
                Rectangle itemRect = {entRect.x, entRect.y + i * 30, entRect.width, 30};
                if (CheckCollisionPointRec(mousePos, itemRect))
                {
                    DrawRectangleRec(itemRect, Fade(BLUE, 0.5f));
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                    {
                        // Process entities action for entitiesItems[i]
                        if (i == 0)
                        {
                            enemyPlacementType = ENEMY_GROUND;
                        }
                        else if (i == 1)
                        {
                            enemyPlacementType = ENEMY_FLYING;
                        }
                        else if (i == 2)
                        {
                            enemyPlacementType = -2; // special mode for boss placement
                            bossSelected = false;
                        }

                        entitiesSubmenuOpen = false;
                        toolsMenuOpen = false;
                    }
                }
                DrawText(entitiesItems[i], itemRect.x + 5, itemRect.y + 5, 20, BLACK);
            }
        }
    }

    Rectangle playButton = {1280 - 90, 0, 80, 30};
    if (DrawButton("Play", playButton, BLUE, BLACK, 20))
    {
        // On Play, load checkpoint state for game mode.
        if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
        {
            TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
        }

        *gameState->editorMode = false;
    }

    if (showFileList)
    {
        // Define a window rectangle (adjust position/size as desired):
        int rowHeight = 20;
        int windowWidth = 400;
        int windowHeight = levelFileCount * rowHeight + 80;
        Rectangle fileListWindow = {200, 100, windowWidth, windowHeight};
        if (!isOverUi)
        {
            isOverUi = CheckCollisionPointRec(mousePos, fileListWindow);
        }

        // Draw window background and border.
        DrawRectangleRec(fileListWindow, Fade(LIGHTGRAY, 0.9f));
        DrawRectangleLines(fileListWindow.x, fileListWindow.y, fileListWindow.width, fileListWindow.height, BLACK);
        DrawText("Select a Level File", fileListWindow.x + 10, fileListWindow.y + 10, rowHeight, BLACK);

        // Draw each file name in a row.
        for (int i = 0; i < levelFileCount; i++)
        {
            Rectangle fileRow = {fileListWindow.x + 10, fileListWindow.y + 40 + i * rowHeight, windowWidth - 20, rowHeight};

            // Check if the mouse is over this file's rectangle.
            if (CheckCollisionPointRec(GetMousePosition(), fileRow))
            {
                // Draw a highlight behind the text.
                DrawRectangleRec(fileRow, Fade(GREEN, 0.3f));
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    selectedFileIndex = i;
                }
            }

            // If this file is selected, draw a border.
            if (i == selectedFileIndex)
            {
                DrawRectangleLines(fileRow.x, fileRow.y, fileRow.width, fileRow.height, RED);
            }

            // Draw the file name text.
            DrawText(levelFiles[i], fileRow.x + 5, fileRow.y + 2, 15, BLACK);
        }
        // Define Load and Cancel buttons within the window.
        Rectangle loadButton = {fileListWindow.x + 10, fileListWindow.y + windowHeight - 30, 100, rowHeight};
        Rectangle cancelButton = {fileListWindow.x + 120, fileListWindow.y + windowHeight - 30, 100, rowHeight};

        if (DrawButton("Load", loadButton, LIGHTGRAY, BLACK, 20))
        {
            if (selectedFileIndex >= 0 && selectedFileIndex < levelFileCount)
            {
                // Copy the selected file path into your currentLevelFilename buffer.
                strcpy(gameState->currentLevelFilename, levelFiles[selectedFileIndex]);
                // Optionally, call your LoadLevel function to load the file.
                if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, gameState->enemies, &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
                {
                    TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
                }
                else
                {
                    TraceLog(LOG_INFO, "Loaded level: %s", gameState->currentLevelFilename);
                }
                // Hide the file selection window.
                showFileList = false;
            }
        }
        if (DrawButton("Cancel", cancelButton, LIGHTGRAY, BLACK, 20))
        {
            showFileList = false;
        }
    }

    if (selectedEntityIndex != -1)
    {
        Rectangle enemyInspectorPanel = {SCREEN_WIDTH - 210, 40, 200, 200};
        if (!isOverUi)
        {
            isOverUi = CheckCollisionPointRec(mousePos, enemyInspectorPanel);
        }

        DrawRectangleRec(enemyInspectorPanel, LIGHTGRAY);
        DrawText("Enemy Inspector", enemyInspectorPanel.x + 5, enemyInspectorPanel.y + 5, 10, BLACK);

        // If an enemy is selected, show its data and provide editing buttons.
        if (selectedEntityIndex >= 0)
        {
            // Buttons to adjust health and toggle type.
            Rectangle btnHealthUp = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 75, 40, 20};
            Rectangle btnHealthDown = {enemyInspectorPanel.x + 150, enemyInspectorPanel.y + 75, 40, 20};
            Rectangle btnToggleType = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 50, 90, 20};
            Rectangle btnDeleteEnemy = {enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 90, 20};

            char info[128];
            sprintf(info, "Type: %s", (gameState->enemies[selectedEntityIndex].type));
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);

            if (DrawButton("Toggle Type", btnToggleType, WHITE, BLACK, 10))
            {
                gameState->enemies[selectedEntityIndex].type =
                    (gameState->enemies[selectedEntityIndex].type == ENEMY_GROUND) ? ENEMY_FLYING : ENEMY_GROUND;
            }

            sprintf(info, "Health: %d", gameState->enemies[selectedEntityIndex].health);
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 75, 10, BLACK);

            if (DrawButton("+", btnHealthUp, WHITE, BLACK, 10))
            {
                gameState->enemies[selectedEntityIndex].health++;
            }
            if (DrawButton("-", btnHealthDown, WHITE, BLACK, 10))
            {
                if (gameState->enemies[selectedEntityIndex].health > 0)
                    gameState->enemies[selectedEntityIndex].health--;
            }

            sprintf(info, "Pos: %.0f, %.0f", gameState->enemies[selectedEntityIndex].position.x, gameState->enemies[selectedEntityIndex].position.y);
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 10, BLACK);

            if (DrawButton("Delete", btnDeleteEnemy, RED, WHITE, 10))
            {
                gameState->enemies[selectedEntityIndex].health = 0;
                gameState->enemies[selectedEntityIndex].type = ENEMY_NONE;
                selectedEntityIndex = -1;
            }
        }
        else if (selectedEntityIndex == -2)
        {
            Rectangle btnHealthUp = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 75, 40, 20};
            Rectangle btnHealthDown = {enemyInspectorPanel.x + 150, enemyInspectorPanel.y + 75, 40, 20};
            // For the boss, we display its health and position.
            char info[128];
            sprintf(info, "Boss HP: %d", gameState->bossEnemy.health);
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
            if (DrawButton("+", btnHealthUp, WHITE, BLACK, 10))
            {
                gameState->bossEnemy.health++;
            }
            if (DrawButton("-", btnHealthDown, WHITE, BLACK, 10))
            {
                if (gameState->bossEnemy.health > 0)
                    gameState->bossEnemy.health--;
            }
            sprintf(info, "Pos: %.0f, %.0f", gameState->bossEnemy.position.x, gameState->bossEnemy.position.y);
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 10, BLACK);
        }
        else if (selectedEntityIndex == -3)
        {
            // If no enemy is selected but a placement tool is active, show a placement message.
            Rectangle btnHealthUp = {enemyInspectorPanel.x + 100, enemyInspectorPanel.y + 75, 40, 20};
            Rectangle btnHealthDown = {enemyInspectorPanel.x + 150, enemyInspectorPanel.y + 75, 40, 20};
            // For the boss, we display its health and position.
            char info[128];
            sprintf(info, "Player HP: %d", gameState->player.health);
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 50, 10, BLACK);
            if (DrawButton("+", btnHealthUp, WHITE, BLACK, 10))
            {
                gameState->player.health++;
            }
            if (DrawButton("-", btnHealthDown, WHITE, BLACK, 10))
            {
                if (gameState->player.health > 0)
                    gameState->player.health--;
            }
            sprintf(info, "Pos: %.0f, %.0f", gameState->player.position.x, gameState->player.position.y);
            DrawText(info, enemyInspectorPanel.x + 10, enemyInspectorPanel.y + 100, 10, BLACK);
        }
    }

    /////////////////////////////////////////////////////////

    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))
    {
        Vector2 delta = GetMouseDelta();
        camera.target.x -= delta.x / camera.zoom;
        camera.target.y -= delta.y / camera.zoom;
    }

    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0)
    {
        camera.zoom += wheelMove * 0.05f;
        if (camera.zoom < 0.1f)
            camera.zoom = 0.1f;
        if (camera.zoom > 3.0f)
            camera.zoom = 3.0f;
    }

    // --- World Click Processing for Enemy Placement / Selection ---
    if (enemyPlacementType == -2 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isOverUi)
    {
        // Place the boss at the clicked position.
        gameState->bossEnemy.type = ENEMY_GROUND;
        gameState->bossEnemy.position = screenPos;
        gameState->bossEnemy.baseY = screenPos.y;
        gameState->bossEnemy.radius = 40.0f;
        gameState->bossEnemy.health = BOSS_MAX_HEALTH;
        gameState->bossEnemy.speed = 2.0f; // initial phase 1 speed
        gameState->bossEnemy.leftBound = 100;
        gameState->bossEnemy.rightBound = LEVEL_WIDTH - 100;
        gameState->bossEnemy.direction = 1;
        gameState->bossEnemy.shootTimer = 0;
        gameState->bossEnemy.shootCooldown = 120.0f;
        gameState->bossEnemy.waveOffset = 0;
        gameState->bossEnemy.waveAmplitude = 20.0f;
        gameState->bossEnemy.waveSpeed = 0.02f;
        bossSelected = true;     // select boss so its inspector shows
        enemyPlacementType = -1; // exit boss placement mode
    }
    // Otherwise, if not in a special placement mode, process regular enemy selection.
    if (enemyPlacementType != -2 && enemyPlacementType != -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isOverUi)
    {
        // (Regular enemy placement code – unchanged)
        bool clickedOnEnemy = false;
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            float dx = screenPos.x - gameState->enemies[i].position.x;
            float dy = screenPos.y - gameState->enemies[i].position.y;
            if ((dx * dx + dy * dy) <= (gameState->enemies[i].radius * gameState->enemies[i].radius))
            {
                clickedOnEnemy = true;
                selectedEntityIndex = i;
                enemyPlacementType = -1;
                break;
            }
        }
        float pdx = screenPos.x - gameState->player.position.x;
        float pdy = screenPos.y - gameState->player.position.y;
        if ((pdx * pdx + pdy * pdy) <= (gameState->player.radius * gameState->player.radius))
        {
            clickedOnEnemy = true;
            selectedEntityIndex = -3;
            enemyPlacementType = -1;
        }
        if (!clickedOnEnemy)
        {
            int newIndex = -1;
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (gameState->enemies[i].health <= 0)
                {
                    newIndex = i;
                    break;
                }
            }
            if (newIndex != -1)
            {
                gameState->enemies[newIndex].type = (EnemyType)enemyPlacementType;
                gameState->enemies[newIndex].position = screenPos;
                gameState->enemies[newIndex].velocity = (Vector2){0, 0};
                gameState->enemies[newIndex].radius = 20.0f;
                gameState->enemies[newIndex].health = (enemyPlacementType == ENEMY_GROUND) ? 3 : 2;
                gameState->enemies[newIndex].speed = (enemyPlacementType == ENEMY_GROUND) ? 2.0f : 1.5f;
                gameState->enemies[newIndex].leftBound = screenPos.x - 50;
                gameState->enemies[newIndex].rightBound = screenPos.x + 50;
                gameState->enemies[newIndex].direction = 1;
                gameState->enemies[newIndex].shootTimer = 0.0f;
                gameState->enemies[newIndex].shootCooldown = (enemyPlacementType == ENEMY_GROUND) ? 60.0f : 90.0f;
                gameState->enemies[newIndex].baseY = screenPos.y;
                gameState->enemies[newIndex].waveOffset = 0.0f;
                gameState->enemies[newIndex].waveAmplitude = (enemyPlacementType == ENEMY_FLYING) ? 40.0f : 0.0f;
                gameState->enemies[newIndex].waveSpeed = (enemyPlacementType == ENEMY_FLYING) ? 0.04f : 0.0f;
                selectedEntityIndex = newIndex;
            }
        }
    }

    if (enemyPlacementType == -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        bool hitEntity = false;
        // Check for enemy selection:
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (gameState->enemies[i].health > 0)
            {
                float dx = screenPos.x - gameState->enemies[i].position.x;
                float dy = screenPos.y - gameState->enemies[i].position.y;
                if ((dx * dx + dy * dy) <= (gameState->enemies[i].radius * gameState->enemies[i].radius))
                {
                    selectedEntityIndex = i;
                    enemyInspectorIndex = i; // update the inspector to show this enemy
                    hitEntity = true;
                    break;
                }
            }
        }
        // Check for boss selection if no enemy was hit:
        if (!hitEntity)
        {
            float dx = screenPos.x - gameState->bossEnemy.position.x;
            float dy = screenPos.y - gameState->bossEnemy.position.y;
            if ((dx * dx + dy * dy) <= (gameState->bossEnemy.radius * gameState->bossEnemy.radius))
            {
                selectedEntityIndex = -2; // use -2 for boss dragging/selection
                enemyInspectorIndex = -2; // update the inspector to show the boss
                hitEntity = true;
            }
        }
        // Check for player selection if still nothing was hit:
        if (!hitEntity)
        {
            float dx = screenPos.x - gameState->player.position.x;
            float dy = screenPos.y - gameState->player.position.y;
            if ((dx * dx + dy * dy) <= (gameState->player.radius * gameState->player.radius))
            {
                selectedEntityIndex = -3; // use -3 for the player
                enemyInspectorIndex = -1; // do not show enemy inspector for the player
                hitEntity = true;
            }
        }
        // If nothing was hit, clear selection and inspector data:
        if (!hitEntity || isOverUi)
        {
            selectedEntityIndex = -1;
            enemyInspectorIndex = -1;
        }
    }

    // -- Player Dragging --
    if (selectedEntityIndex >= 0)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->enemies[selectedEntityIndex].position.x = screenPos.x - dragOffset.x;
            gameState->enemies[selectedEntityIndex].position.y = screenPos.y - dragOffset.y;
            if (gameState->enemies[selectedEntityIndex].type == ENEMY_FLYING)
                gameState->enemies[selectedEntityIndex].baseY = gameState->enemies[selectedEntityIndex].position.y;
        }
    }
    else if (selectedEntityIndex == -2)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->bossEnemy.position.x = screenPos.x - dragOffset.x;
            gameState->bossEnemy.position.y = screenPos.y - dragOffset.y;
            gameState->bossEnemy.baseY = gameState->bossEnemy.position.y;
        }
    }
    else if (selectedEntityIndex == -3)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->player.position.x = screenPos.x - dragOffset.x;
            gameState->player.position.y = screenPos.y - dragOffset.y;
        }
    }
    // -- Checkpoint Dragging --
    // In editor mode, update checkpoint dragging:
    if (gameState->checkpointCount > 0)
    {
        // Loop over each checkpoint.
        for (int i = 0; i < gameState->checkpointCount; i++)
        {
            // Define the rectangle for this checkpoint.
            Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2};

            // If not already dragging something and the mouse is over this checkpoint,
            // start dragging it.
            if (draggedCheckpointIndex == -1 && selectedEntityIndex == -1 && draggedBoundEnemy == -1 && !isOverUi &&
                CheckCollisionPointRec(screenPos, cpRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                draggedCheckpointIndex = i;
                dragOffset.x = screenPos.x - gameState->checkpoints[i].x;
                dragOffset.y = screenPos.y - gameState->checkpoints[i].y;
                break; // Only one checkpoint can be dragged at a time.
            }
        }
    }

    // If a checkpoint is being dragged, update its position.
    if (draggedCheckpointIndex != -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->checkpoints[draggedCheckpointIndex].x = screenPos.x - dragOffset.x;
            gameState->checkpoints[draggedCheckpointIndex].y = screenPos.y - dragOffset.y;
        }
        else
        {
            // Once the mouse button is released, stop dragging.
            draggedCheckpointIndex = -1;
        }
    }

    // --- Place/Remove Tiles Based on the Current Tool ---
    // We allow tile placement if not dragging any enemies or bounds:
    bool placementEditing = (selectedEntityIndex != -1 || draggedBoundEnemy != -1 || draggedCheckpointIndex != -1);
    if (enemyPlacementType == -1 && !placementEditing && !isOverUi)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPainting)
                isPainting = true;

            if (isPainting)
            {
                // Use worldPos (converted via GetScreenToWorld2D) for tile placement.
                int tileX = (int)(screenPos.x / TILE_SIZE);
                int tileY = (int)(screenPos.y / TILE_SIZE);
                if (tileX >= 0 && tileX < MAP_COLS && tileY >= 0 && tileY < MAP_ROWS)
                {
                    // Depending on the current tool, set the tile value:
                    if (currentTool == TOOL_GROUND)
                    {
                        mapTiles[tileY][tileX] = 1;
                    }
                    else if (currentTool == TOOL_DEATH)
                    {
                        mapTiles[tileY][tileX] = 2;
                    }
                    else if (currentTool == TOOL_ERASER)
                    {
                        // Eraser: remove tile regardless of mouse button (or require RMB)
                        mapTiles[tileY][tileX] = 0;
                    }
                }
            }
        }
        else
        {
            isPainting = false;
        }
    }
}
