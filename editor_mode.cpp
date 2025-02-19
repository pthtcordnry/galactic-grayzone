#include "editor_mode.h"
#include <string.h>
#include <raylib.h>
#include "file_io.h"
#include "main.h"
#include <imgui.h>
#include <rlImGui.h>

enum TileTool
{
    TOOL_GROUND = 0,
    TOOL_DEATH = 1,
    TOOL_ERASER = 2
};

// Menu item labels
const char *fileItems[3] = {"New", "Open", "Save"};
const char *tilemapItems[3] = {"Ground", "Death", "Eraser"};
const char *entitiesItems[3] = {"New Asset", "Load Assets", "Save Assets"};

// State variables for menu status
bool fileMenuOpen = false;
bool toolsMenuOpen = false;
bool tilemapSubmenuOpen = false;
bool entitiesSubmenuOpen = false;
bool showFileList = false;
bool showAssetList = true;
bool isPainting = false;

int selectedFileIndex = -1;
int selectedAssetIndex = -1;
int selectedEntityIndex = -1;
int enemyInspectorIndex = -1;
int selectedCheckpointIndex = -1;
int boundType = -1; // 0 = left bound, 1 = right bound

Vector2 dragOffset = {0};
TileTool currentTool = TOOL_GROUND;
bool showNewLevelPopup = false;

static bool IsLevelLoaded(void)
{
    return (gameState->currentLevelFilename[0] != '\0');
}

void TickInput()
{
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
}

void DoEntityPicking(Vector2 screenPos)
{
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        bool hitObject = false;
        if (gameState->enemies != NULL)
        {
            for (int i = 0; i < gameState->enemyCount; i++)
            {

                float dx = screenPos.x - gameState->enemies[i].position.x;
                float dy = screenPos.y - gameState->enemies[i].position.y;
                if ((dx * dx + dy * dy) <= (gameState->enemies[i].radius * gameState->enemies[i].radius))
                {
                    selectedEntityIndex = i;
                    enemyInspectorIndex = i; // update inspector to show this enemy
                    hitObject = true;
                    break;
                }
            }
        }

        if (!hitObject && gameState->bossEnemy != NULL)
        {
            float dx = screenPos.x - gameState->bossEnemy->position.x;
            float dy = screenPos.y - gameState->bossEnemy->position.y;
            if ((dx * dx + dy * dy) <= (gameState->bossEnemy->radius * gameState->bossEnemy->radius))
            {
                selectedEntityIndex = -2; // boss
                enemyInspectorIndex = -2;
                hitObject = true;
            }
        }

        if (!hitObject && gameState->player != NULL)
        {
            float dx = screenPos.x - gameState->player->position.x;
            float dy = screenPos.y - gameState->player->position.y;
            if ((dx * dx + dy * dy) <= (gameState->player->radius * gameState->player->radius))
            {
                selectedEntityIndex = -3; // player
                enemyInspectorIndex = -1;
                hitObject = true;
            }
        }

        if (!hitObject && gameState->checkpoints != NULL)
        {
            // Loop over each checkpoint.
            for (int i = 0; i < gameState->checkpointCount; i++)
            {
                // Define the rectangle for this checkpoint.
                Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2};

                // If not already dragging something and the mouse is over this checkpoint,
                // start dragging it.
                if (selectedCheckpointIndex == -1 && selectedEntityIndex == -1 &&
                    CheckCollisionPointRec(screenPos, cpRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    selectedCheckpointIndex = i;
                    hitObject = true;
                    break;
                }
            }
        }

        // If nothing was hit
        if (!hitObject)
        {
            // No asset selected and nothing hit: clear selection.
            selectedEntityIndex = -1;
            enemyInspectorIndex = -1;
            selectedCheckpointIndex = -1;
        }
    }
}

void DoEntityDrag(Vector2 screenPos)
{
    // -- Player Dragging --
    if (selectedEntityIndex >= 0)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->enemies[selectedEntityIndex].position.x = screenPos.x - dragOffset.x;
            gameState->enemies[selectedEntityIndex].position.y = screenPos.y - dragOffset.y;
            gameState->enemies[selectedEntityIndex].baseY = gameState->enemies[selectedEntityIndex].position.y;
        }
    }
    else if (selectedEntityIndex == -2)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->bossEnemy->position.x = screenPos.x - dragOffset.x;
            gameState->bossEnemy->position.y = screenPos.y - dragOffset.y;
            gameState->bossEnemy->baseY = gameState->bossEnemy->position.y;
        }
    }
    else if (selectedEntityIndex == -3)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->player->position.x = screenPos.x - dragOffset.x;
            gameState->player->position.y = screenPos.y - dragOffset.y;
        }
    }

    // If a checkpoint is being dragged, update its position.
    if (selectedCheckpointIndex != -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->checkpoints[selectedCheckpointIndex].x = screenPos.x - dragOffset.x;
            gameState->checkpoints[selectedCheckpointIndex].y = screenPos.y - dragOffset.y;
        }
        else
        {
            // Once the mouse button is released, stop dragging.
            selectedCheckpointIndex = -1;
        }
    }
}

void DoEntityCreation(Vector2 screenPos)
{
    // Place the selected asset in the world when clicking in the editor world
    if (selectedAssetIndex != -1 && selectedEntityIndex == -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        TraceLog(LOG_INFO, "Creating entity!");
        Entity *asset = &entityAssets[selectedAssetIndex];

        if (asset->kind == ENTITY_ENEMY)
        {
            // Allocate enemies array if needed.
            if (gameState->enemies == NULL)
            {
                gameState->enemyCount++;
                gameState->enemies = (Entity *)arena_alloc(&gameState->gameArena, gameState->enemyCount * sizeof(Entity));
                gameState->enemies[0] =
                    {
                        .physicsType = asset->physicsType,
                        .position = screenPos,
                        .radius = asset->radius,
                        .health = asset->health,
                        .speed = asset->speed,
                        .leftBound = asset->leftBound,
                        .rightBound = asset->rightBound,
                        .shootCooldown = asset->shootCooldown,
                        .baseY = screenPos.y,
                        .waveAmplitude = asset->waveAmplitude,
                        .waveSpeed = asset->waveSpeed};

                selectedEntityIndex = 0;
            }
        }
        else if (asset->kind == ENTITY_BOSS)
        {
            Entity bossEnemy =
                {
                    .physicsType = asset->physicsType,
                    .position = screenPos,
                    .radius = asset->radius,
                    .health = asset->health,
                    .speed = asset->speed,
                    .leftBound = asset->leftBound,
                    .rightBound = asset->rightBound,
                    .shootCooldown = asset->shootCooldown,
                    .baseY = screenPos.y,
                    .waveAmplitude = asset->waveAmplitude,
                    .waveSpeed = asset->waveSpeed};

            // If bossEnemy pointer is NULL, allocate memory for it.
            if (gameState->bossEnemy == NULL)
            {
                gameState->bossEnemy = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            }

            *gameState->bossEnemy = bossEnemy;
            selectedEntityIndex = -2;
        }
        else if (asset->kind == ENTITY_PLAYER)
        {
            Entity newPlayer =
                {
                    .kind = ENTITY_PLAYER,
                    .position = screenPos,
                    .radius = asset->radius,
                    .health = asset->health,
                };

            // If the player pointer is NULL, allocate memory for it.
            if (gameState->player == NULL)
            {
                gameState->player = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            }

            *gameState->player = newPlayer;
            selectedEntityIndex = -3; // -3 indicates the player is selected.
        }
    }
}

void DoTilePaint(Vector2 screenPos)
{
    // We allow tile placement if not dragging any enemies or bounds:
    bool placementEditing = (selectedEntityIndex != -1 || selectedCheckpointIndex != -1);
    if (selectedAssetIndex == -1 && !placementEditing)
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

void DrawEditorUI()
{
    rlImGuiBegin();

    // --- Main Menu Bar ---
    if (ImGui::BeginMainMenuBar())
    {
        // File Menu
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {
                // reset gameState pointers
                gameState->player = NULL;
                gameState->enemies = NULL;
                gameState->bossEnemy = NULL;
                gameState->enemyCount = 0;
                gameState->checkpointCount = 0;

                // Open a modal to prompt for a level name.
                showNewLevelPopup = true;
                ImGui::OpenPopup("New Level");
            }
            if (ImGui::MenuItem("Open"))
            {
                const char *levelsDir = "levels";      // Levels directory.
                const char *levelExtension = ".level"; // Files ending with .level

                // Count how many files match.
                int currentCount = CountFilesWithExtension(levelsDir, levelExtension);
                if (currentCount <= 0)
                {
                    TraceLog(LOG_WARNING, "No level files found in %s", levelsDir);
                }
                else
                {
                    // If levelFiles hasn't been allocated yet, allocate it.
                    if (levelFiles == NULL)
                    {
                        levelFiles = (char(*)[256])arena_alloc(&gameState->gameArena, currentCount * sizeof(*levelFiles));
                        if (levelFiles == NULL)
                        {
                            TraceLog(LOG_ERROR, "Failed to allocate memory for level file list!");
                        }
                    }
                    else if (currentCount != levelFileCount)
                    {
                        // Try to reallocate the existing block to the new size.
                        levelFiles = (char(*)[256])arena_realloc(&gameState->gameArena, levelFiles, currentCount * sizeof(*levelFiles));
                    }
                    // Update our stored file count.
                    levelFileCount = currentCount;

                    // Now fill in the file list.
                    int index = 0;
                    FillFilesWithExtensionContiguous(levelsDir, levelExtension, levelFiles, &index);

                    // For debugging: print each found level file and draw it on screen.
                    for (int i = 0; i < index; i++)
                    {
                        TraceLog(LOG_INFO, levelFiles[i]);
                        DrawText(levelFiles[i], 50, 100 + i * 20, 10, BLACK);
                    }
                }
                showFileList = !showFileList;
            }
            if (ImGui::MenuItem("Save"))
            {
                if (IsLevelLoaded())
                {
                    if (SaveLevel(gameState->currentLevelFilename, mapTiles, gameState->player,
                                  gameState->enemies, gameState->bossEnemy))
                        TraceLog(LOG_INFO, "Level saved successfully!");
                    else
                        TraceLog(LOG_ERROR, "Failed to save Level!");
                }
                else
                {
                    TraceLog(LOG_WARNING, "No level loaded to save!");
                }
            }
            ImGui::EndMenu();
        }
        // Tools Menu (Tilemap and Entities as before)
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::BeginMenu("Tilemap"))
            {
                if (ImGui::MenuItem("Ground"))
                    currentTool = TOOL_GROUND;
                if (ImGui::MenuItem("Death"))
                    currentTool = TOOL_DEATH;
                if (ImGui::MenuItem("Eraser"))
                    currentTool = TOOL_ERASER;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Entities"))
            {
                if (ImGui::MenuItem("New Asset"))
                {
                    if (entityAssetCount < MAX_ENTITY_ASSETS)
                    {
                        Entity newAsset = {
                            .kind = ENTITY_ENEMY,
                            .physicsType = GROUND,
                            .radius = 20.0f,
                            .health = 3,
                            .speed = 2.0f,
                            .leftBound = 0,
                            .rightBound = 100,
                            .shootCooldown = 60.0f,
                            .baseY = 0};
                        strcpy(newAsset.name, "New Enemy");

                        if(entityAssets == NULL)
                        {
                            entityAssets = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity) * entityAssetCount);
                        }
                        entityAssets[entityAssetCount] = newAsset;
                        selectedAssetIndex = entityAssetCount;
                        entityAssetCount++;
                    }
                }
                if (ImGui::MenuItem("Load Assets"))
                {
                    if (LoadEntityAssets("./assets/", entityAssets, &entityAssetCount))
                        TraceLog(LOG_INFO, "Entity assets loaded");
                    else
                        TraceLog(LOG_ERROR, "Failed to load entity assets");
                }
                if (ImGui::MenuItem("Save Assets"))
                {
                    if (SaveAllEntityAssets("./assets/", entityAssets, entityAssetCount, false))
                        TraceLog(LOG_INFO, "Entity assets saved");
                    else
                        TraceLog(LOG_ERROR, "Failed to save entity assets");
                }
                if (ImGui::MenuItem("Show Asset List")) 
                {
                    showAssetList = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        // Play and Stop buttons.
        if (ImGui::Button("Play"))
        {
            if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, &gameState->enemies,
                                     &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
            {
                TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
            }
            *gameState->editorMode = false;
        }
        if (ImGui::Button("Stop"))
        {
            if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, &gameState->enemies, &gameState->enemyCount, &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                TraceLog(LOG_ERROR, "Failed to reload level for editor mode!");
            *gameState->editorMode = true;
        }
        ImGui::EndMainMenuBar();
    }

    if (showNewLevelPopup)
    {
        ImGui::OpenPopup("New Level");
    }

    // --- New Level Popup ---
    if (ImGui::BeginPopupModal("New Level", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetWindowPos(ImVec2(10, 10));
        ImGui::SetWindowSize(ImVec2(250, 75));
        static char tempLevelName[128] = "";
        ImGui::InputText(".level", tempLevelName, sizeof(tempLevelName));
        if (ImGui::Button("Create"))
        {
            char fixedName[256] = "";
            // Look for the last '.' in tempLevelName.
            const char *ext = strrchr(tempLevelName, '.');
            if (ext == NULL)
            {
                // No extension found; append ".level"
                snprintf(fixedName, sizeof(fixedName), "%s.level", tempLevelName);
            }
            else if (strcmp(ext, ".level") != 0)
            {
                // Found an extension, but it's not ".level".
                size_t baseLen = ext - tempLevelName;
                if (baseLen > 0 && baseLen < sizeof(fixedName))
                {
                    strncpy(fixedName, tempLevelName, baseLen);
                    fixedName[baseLen] = '\0';
                    strcat(fixedName, ".level");
                }
                else
                {
                    // In case of error, just use the original name appended with .level
                    snprintf(fixedName, sizeof(fixedName), "%s.level", tempLevelName);
                }
            }
            else
            {
                // Extension is already ".level"
                strcpy(fixedName, tempLevelName);
            }

            strcpy(gameState->currentLevelFilename, fixedName);

            // Clear map and reset state.
            for (int y = 0; y < MAP_ROWS; y++)
                for (int x = 0; x < MAP_COLS; x++)
                    mapTiles[y][x] = 0;

            ImGui::CloseCurrentPopup();
            showNewLevelPopup = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
            showNewLevelPopup = false;
        }
        ImGui::EndPopup();
    }

    // --- If no level loaded, show a placeholder ---
    if (!IsLevelLoaded())
    {
        ImGui::Begin("No Level Loaded", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::SetWindowPos(ImVec2(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 - 50));
        ImGui::SetWindowSize(ImVec2(300, 100));
        ImGui::Text("No level loaded.");
        ImGui::Text("Create or open a file.");
        ImGui::End();
    }
    else
    {
        // --- Asset List Panel ---
        if (showAssetList)
        {
            ImGui::Begin("Asset List", &showAssetList); // <-- Pass &showAssetList
            ImGui::SetWindowPos(ImVec2(10, SCREEN_HEIGHT / 2 - 50));
            ImGui::SetWindowSize(ImVec2(250, 300));
            for (int i = 0; i < entityAssetCount; i++)
            {
                if (ImGui::Selectable(entityAssets[i].name, selectedAssetIndex == i))
                    selectedAssetIndex = i;
            }
            ImGui::End();
        }

        // --- Asset Inspector Panel ---
        if (selectedAssetIndex != -1)
        {
            ImGui::Begin("Asset Inspector");
            ImGui::SetWindowPos(ImVec2(260, SCREEN_HEIGHT / 2 - 50));
            ImGui::SetWindowSize(ImVec2(250, 250));
            Entity *asset = &entityAssets[selectedAssetIndex];
            char nameBuffer[64];
            strcpy(nameBuffer, asset->name);
            if (ImGui::InputText("Name", nameBuffer, 64))
                strcpy(asset->name, nameBuffer);
            const char *kinds[] = {"Enemy", "Player", "Boss"};
            int currentKind = (int)asset->kind;
            if (ImGui::Combo("Kind", &currentKind, kinds, IM_ARRAYSIZE(kinds)))
                asset->kind = (EntityKind)currentKind;
            ImGui::InputInt("Health", &asset->health);
            if (ImGui::Button("+"))
                asset->health++;
            ImGui::SameLine();
            if (ImGui::Button("-") && asset->health > 0)
                asset->health--;
            ImGui::End();
        }

        if (selectedEntityIndex != -1)
        {
            // --- In-World Entity Inspector ---
            ImGui::Begin("Entity Inspector");
            ImGui::SetWindowPos(ImVec2(SCREEN_WIDTH - 260, SCREEN_HEIGHT / 2 - 50));
            ImGui::SetWindowSize(ImVec2(250, 250));
            if (selectedEntityIndex >= 0)
            {
                Entity *enemy = &gameState->enemies[selectedEntityIndex];
                ImGui::Text("Type: %s", enemy->physicsType == GROUND ? "Ground" : "Flying");
                if (ImGui::Button("Toggle Type"))
                    enemy->physicsType = (enemy->physicsType == GROUND) ? FLYING : GROUND;
                ImGui::Text("Health: %d", enemy->health);
                if (ImGui::Button("+"))
                    enemy->health++;
                ImGui::SameLine();
                if (ImGui::Button("-") && enemy->health > 0)
                    enemy->health--;
                ImGui::Text("Pos: %.0f, %.0f", enemy->position.x, enemy->position.y);
                if (ImGui::Button("Delete"))
                {
                    enemy->health = 0;
                    enemy->kind = EMPTY;
                    selectedEntityIndex = -1;
                }
            }
            else if (selectedEntityIndex == -2)
            {
                ImGui::Text("Boss HP: %d", gameState->bossEnemy->health);
                if (ImGui::Button("+"))
                    gameState->bossEnemy->health++;
                ImGui::SameLine();
                if (ImGui::Button("-") && gameState->bossEnemy->health > 0)
                    gameState->bossEnemy->health--;
                ImGui::Text("Pos: %.0f, %.0f", gameState->bossEnemy->position.x, gameState->bossEnemy->position.y);
            }
            else if (selectedEntityIndex == -3)
            {
                ImGui::Text("Player HP: %d", gameState->player->health);
                if (ImGui::Button("+"))
                    gameState->player->health++;
                ImGui::SameLine();
                if (ImGui::Button("-") && gameState->player->health > 0)
                    gameState->player->health--;
                ImGui::Text("Pos: %.0f, %.0f", gameState->player->position.x, gameState->player->position.y);
            }
            ImGui::End();
        }

        if (ImGui::Begin("ToolInfo", NULL,
                         ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoMove))
        {
            ImGui::SetWindowPos(ImVec2(10, SCREEN_HEIGHT - 40));
            const char *toolText = "";
            // If no asset is selected, we are using tilemap tools.
            if (selectedAssetIndex == -1)
            {
                switch (currentTool)
                {
                case TOOL_GROUND:
                    toolText = "Tilemap Tool: Ground";
                    break;
                case TOOL_DEATH:
                    toolText = "Tilemap Tool: Death";
                    break;
                case TOOL_ERASER:
                    toolText = "Tilemap Tool: Eraser";
                    break;
                default:
                    toolText = "Tilemap Tool: Unknown";
                    break;
                }
            }
            else // An asset is selected; use entity placement tool.
            {
                switch (entityAssets[selectedAssetIndex].kind)
                {
                case ENTITY_ENEMY:
                    toolText = "Entity Tool: Place Enemy";
                    break;
                case ENTITY_BOSS:
                    toolText = "Entity Tool: Place Boss";
                    break;
                case ENTITY_PLAYER:
                    toolText = "Entity Tool: Place Player";
                    break;
                default:
                    toolText = "Entity Tool: Unknown";
                    break;
                }
            }
            ImGui::Text("%s", toolText);
            ImGui::End();
        }
    }

    // --- File List Window ---
    if (showFileList)
    {
        ImGui::Begin("Select a Level File", &showFileList);
        ImGui::SetWindowPos(ImVec2(60, 60));
        ImGui::SetWindowSize(ImVec2(375, 275));
        for (int i = 0; i < levelFileCount; i++)
        {
            if (ImGui::Selectable(levelFiles[i], selectedFileIndex == i))
                selectedFileIndex = i;
        }
        if (ImGui::Button("Load"))
        {
            if (selectedFileIndex >= 0 && selectedFileIndex < levelFileCount)
            {
                // Extract base filename from levelFiles[selectedFileIndex]
                const char *fullPath = levelFiles[selectedFileIndex];
                const char *baseName = strrchr(fullPath, '/');
                if (!baseName)
                    baseName = strrchr(fullPath, '\\');
                if (baseName)
                    baseName++; // Skip the separator
                else
                    baseName = fullPath; // No separator found; use the whole string

                strcpy(gameState->currentLevelFilename, baseName);

                if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player, &gameState->enemies,
                               &gameState->enemyCount, &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                {
                    TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
                }
                else
                {
                    TraceLog(LOG_INFO, "Loaded level: %s", gameState->currentLevelFilename);
                }
                showFileList = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            showFileList = false;
        ImGui::End();
    }

    rlImGuiEnd();
}

void DrawEditor()
{
    Vector2 mousePos = GetMousePosition();
    Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

    DrawEditorUI();
    TickInput();

    if (ImGui::GetIO().WantCaptureMouse || !IsLevelLoaded())
        return;

    DoEntityPicking(screenPos);
    DoEntityDrag(screenPos);
    DoEntityCreation(screenPos);
    DoTilePaint(screenPos);
}
