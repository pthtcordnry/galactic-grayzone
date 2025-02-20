// editor_mode.c
#include "editor_mode.h"
#include <string.h>
#include <raylib.h>
#include "file_io.h"
#include "main.h"
#include <imgui.h>
#include <rlImGui.h>

// --- ENUMS AND CONSTANTS ---
enum TileTool
{
    TILE_TOOL_NONE = 0,
    TILE_TOOL_GROUND = 1,
    TILE_TOOL_DEATH = 2,
    TILE_TOOL_ERASER = 3
};

const char *fileItems[3] = {"New", "Open", "Save"};
const char *tilemapItems[3] = {"Ground", "Death", "Eraser"};
const char *entitiesItems[3] = {"New Asset", "Load Assets", "Save Assets"};

// --- GLOBAL STATE (for editor UI) ---
static bool showFileList = false;
static bool showAssetList = true;
static bool showOverwritePopup = false;
static bool showNewLevelPopup = false;
static bool isPainting = false;

static int selectedFileIndex = -1;
static int selectedAssetIndex = -1;  // index into the EntityAsset array
static int selectedEntityIndex = -1; // index into the instance arrays
static int selectedCheckpointIndex = -1;
static int boundType = -1; // 0 = left bound, 1 = right bound

static Vector2 dragOffset = {0};
static TileTool currentTool = TILE_TOOL_NONE;

static bool IsLevelLoaded(void)
{
    return (gameState->currentLevelFilename[0] != '\0');
}

void TickInput(void)
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
                hitObject = true;
            }
        }
        if (!hitObject && gameState->checkpoints != NULL)
        {
            for (int i = 0; i < gameState->checkpointCount; i++)
            {
                Rectangle cpRect = {gameState->checkpoints[i].x, gameState->checkpoints[i].y, TILE_SIZE, TILE_SIZE * 2};
                if (selectedCheckpointIndex == -1 && selectedEntityIndex == -1 &&
                    CheckCollisionPointRec(screenPos, cpRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                {
                    selectedCheckpointIndex = i;
                    hitObject = true;
                    break;
                }
            }
        }
        if (!hitObject)
        {
            selectedEntityIndex = -1;
            selectedCheckpointIndex = -1;
        }
    }
}

void DoEntityDrag(Vector2 screenPos)
{
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
    if (selectedCheckpointIndex != -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->checkpoints[selectedCheckpointIndex].x = screenPos.x - dragOffset.x;
            gameState->checkpoints[selectedCheckpointIndex].y = screenPos.y - dragOffset.y;
        }
        else
        {
            selectedCheckpointIndex = -1;
        }
    }
}

///
/// DoEntityCreation: When clicking in the editor world, create a new instance
/// from the selected asset. If the asset kind is EMPTY (None), do nothing.
///
void DoEntityCreation(Vector2 screenPos)
{
    if (selectedAssetIndex == -1 || entityAssets[selectedAssetIndex].kind == EMPTY)
        return;

    if (selectedAssetIndex != -1 && selectedEntityIndex == -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        TraceLog(LOG_INFO, "Creating entity instance from asset!");
        EntityAsset *asset = &entityAssets[selectedAssetIndex];

        Entity newInstance = {0};
        newInstance.asset = asset;
        newInstance.position = screenPos;
        newInstance.velocity = (Vector2){0, 0};
        newInstance.radius = asset->radius;
        newInstance.health = asset->baseHp;
        newInstance.speed = asset->baseSpeed;
        newInstance.shootCooldown = asset->baseAttackSpeed;

        if (asset->kind != ENTITY_PLAYER)
        {
            newInstance.leftBound = 0;
            newInstance.rightBound = 100;
            newInstance.baseY = screenPos.y;
            newInstance.waveOffset = 0;
            newInstance.waveAmplitude = 0;
            newInstance.waveSpeed = 0;
        }

        if (asset->kind == ENTITY_ENEMY)
        {
            if (gameState->enemies == NULL)
            {
                gameState->enemyCount = 1;
                gameState->enemies = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
                gameState->enemies[0] = newInstance;
                selectedEntityIndex = 0;
            }
            else
            {
                int newCount = gameState->enemyCount + 1;
                gameState->enemies = (Entity *)arena_realloc(&gameState->gameArena, gameState->enemies, newCount * sizeof(Entity));
                gameState->enemies[gameState->enemyCount] = newInstance;
                selectedEntityIndex = gameState->enemyCount;
                gameState->enemyCount = newCount;
            }
        }
        else if (asset->kind == ENTITY_BOSS)
        {
            if (gameState->bossEnemy == NULL)
            {
                gameState->bossEnemy = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            }
            *gameState->bossEnemy = newInstance;
            selectedEntityIndex = -2;
        }
        else if (asset->kind == ENTITY_PLAYER)
        {
            if (gameState->player == NULL)
            {
                gameState->player = (Entity *)arena_alloc(&gameState->gameArena, sizeof(Entity));
            }
            *gameState->player = newInstance;
            selectedEntityIndex = -3;
        }
    }
}

void DoTilePaint(Vector2 screenPos)
{
    bool placementEditing = (selectedEntityIndex != -1 || selectedCheckpointIndex != -1);
    if (selectedAssetIndex == -1 && !placementEditing)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !isPainting)
                isPainting = true;
            if (isPainting)
            {
                int tileX = (int)(screenPos.x / TILE_SIZE);
                int tileY = (int)(screenPos.y / TILE_SIZE);
                if (tileX >= 0 && tileX < MAP_COLS && tileY >= 0 && tileY < MAP_ROWS)
                {
                    if (currentTool == TILE_TOOL_GROUND)
                        mapTiles[tileY][tileX] = 1;
                    else if (currentTool == TILE_TOOL_DEATH)
                        mapTiles[tileY][tileX] = 2;
                    else if (currentTool == TILE_TOOL_ERASER)
                        mapTiles[tileY][tileX] = 0;
                }
            }
        }
        else
        {
            isPainting = false;
        }
    }
}

//
// DRAWING UI PANELS
//
static void DrawMainMenuBar(void)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {
                gameState->player = NULL;
                gameState->enemies = NULL;
                gameState->bossEnemy = NULL;
                gameState->enemyCount = 0;
                gameState->checkpointCount = 0;
                showNewLevelPopup = true;
                ImGui::OpenPopup("New Level");
            }
            if (ImGui::MenuItem("Open"))
            {
                const char *levelsDir = "levels";
                const char *levelExtension = ".level";
                int currentCount = CountFilesWithExtension(levelsDir, levelExtension);
                if (currentCount <= 0)
                {
                    TraceLog(LOG_WARNING, "No level files found in %s", levelsDir);
                }
                else
                {
                    if (levelFiles == NULL)
                    {
                        levelFiles = (char(*)[256])arena_alloc(&gameState->gameArena, currentCount * sizeof(*levelFiles));
                        if (levelFiles == NULL)
                            TraceLog(LOG_ERROR, "Failed to allocate memory for level file list!");
                    }
                    else if (currentCount != levelFileCount)
                    {
                        levelFiles = (char(*)[256])arena_realloc(&gameState->gameArena, levelFiles, currentCount * sizeof(*levelFiles));
                    }
                    levelFileCount = currentCount;
                    int index = 0;
                    FillFilesWithExtensionContiguous(levelsDir, levelExtension, levelFiles, &index);
                }
                showFileList = !showFileList;
            }
            if (ImGui::MenuItem("Save"))
            {
                if (IsLevelLoaded())
                {
                    if (SaveLevel(gameState->currentLevelFilename, mapTiles,
                                  gameState->player, gameState->enemies, gameState->bossEnemy))
                        TraceLog(LOG_INFO, "Level saved successfully!");
                    else
                        TraceLog(LOG_ERROR, "Failed to save Level!");
                }
                else
                    TraceLog(LOG_WARNING, "No level loaded to save!");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::BeginMenu("Tilemap"))
            {
                if (ImGui::MenuItem("Ground"))
                    currentTool = TILE_TOOL_GROUND;
                if (ImGui::MenuItem("Death"))
                    currentTool = TILE_TOOL_DEATH;
                if (ImGui::MenuItem("Eraser"))
                    currentTool = TILE_TOOL_ERASER;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Entities"))
            {
                if (ImGui::MenuItem("New Asset"))
                {
                    if (entityAssetCount < MAX_ENTITY_ASSETS)
                    {
                        EntityAsset newAsset = {0};
                        newAsset.kind = EMPTY;
                        newAsset.physicsType = NONE;
                        newAsset.radius = 0;
                        strcpy(newAsset.name, "New Enemy");
                        if (entityAssets == NULL)
                        {
                            entityAssets = (EntityAsset *)arena_alloc(&gameState->gameArena, sizeof(EntityAsset) * (entityAssetCount + 1));
                        }
                        entityAssets[entityAssetCount] = newAsset;
                        selectedAssetIndex = entityAssetCount;
                        entityAssetCount++;
                    }
                }
                if (ImGui::MenuItem("Load Assets"))
                {
                    if (LoadEntityAssets("./assets", &entityAssets, &entityAssetCount))
                        TraceLog(LOG_INFO, "Entity assets loaded");
                    else
                        TraceLog(LOG_ERROR, "Failed to load entity assets");
                }
                if (ImGui::MenuItem("Save Assets"))
                {
                    if (SaveAllEntityAssets("./assets", entityAssets, entityAssetCount, false))
                        TraceLog(LOG_INFO, "Entity assets saved");
                    else
                        showOverwritePopup = true;
                }
                if (ImGui::MenuItem("Show Asset List"))
                    showAssetList = true;
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::Button("Play"))
        {
            if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player, &gameState->enemies,
                                     &gameState->bossEnemy, gameState->checkpoints, &gameState->checkpointCount))
                TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
            *gameState->editorMode = false;
        }
        if (ImGui::Button("Stop"))
        {
            if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player,
                           &gameState->enemies, &gameState->enemyCount,
                           &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                TraceLog(LOG_ERROR, "Failed to reload level for editor mode!");
            *gameState->editorMode = true;
        }
        ImGui::EndMainMenuBar();
    }
}

static void DrawNewLevelPopup(void)
{
    if (showNewLevelPopup)
    {
        ImGui::OpenPopup("New Level");
        if (ImGui::BeginPopupModal("New Level", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetWindowPos(ImVec2(10, 10));
            ImGui::SetWindowSize(ImVec2(250, 75));
            static char tempLevelName[128] = "";
            ImGui::InputText(".level", tempLevelName, sizeof(tempLevelName));
            if (ImGui::Button("Create"))
            {
                char fixedName[256] = "";
                const char *ext = strrchr(tempLevelName, '.');
                if (ext == NULL)
                    snprintf(fixedName, sizeof(fixedName), "%s.level", tempLevelName);
                else if (strcmp(ext, ".level") != 0)
                {
                    size_t baseLen = ext - tempLevelName;
                    if (baseLen > 0 && baseLen < sizeof(fixedName))
                    {
                        strncpy(fixedName, tempLevelName, baseLen);
                        fixedName[baseLen] = '\0';
                        strcat(fixedName, ".level");
                    }
                    else
                        snprintf(fixedName, sizeof(fixedName), "%s.level", tempLevelName);
                }
                else
                    strcpy(fixedName, tempLevelName);
                strcpy(gameState->currentLevelFilename, fixedName);
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
    }
}

static void DrawOverwritePopup(void)
{
    if (showOverwritePopup)
    {
        ImGui::OpenPopup("Overwrite Confirmation");
        if (ImGui::BeginPopupModal("Overwrite Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("One or more asset files already exist.\nOverwrite them?");
            ImGui::Separator();
            if (ImGui::Button("Yes", ImVec2(120, 0)))
            {
                if (SaveAllEntityAssets("./assets", entityAssets, entityAssetCount, true))
                    TraceLog(LOG_INFO, "Entity assets saved with overwrite!");
                else
                    TraceLog(LOG_ERROR, "Failed to save entity assets even with overwrite!");
                ImGui::CloseCurrentPopup();
                showOverwritePopup = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
                showOverwritePopup = false;
            }
            ImGui::EndPopup();
        }
    }
}

static void DrawFileListWindow(void)
{
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
                const char *fullPath = levelFiles[selectedFileIndex];
                const char *baseName = strrchr(fullPath, '/');
                if (!baseName)
                    baseName = strrchr(fullPath, '\\');
                if (baseName)
                    baseName++;
                else
                    baseName = fullPath;
                strcpy(gameState->currentLevelFilename, baseName);
                if (!LoadLevel(gameState->currentLevelFilename, mapTiles, &gameState->player,
                               &gameState->enemies, &gameState->enemyCount,
                               &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
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
}

static void DrawAssetListPanel(void)
{
    if (showAssetList)
    {
        // Get the height of the main menu bar.
        // (Assumes that BeginMainMenuBar() was called earlier in the frame.)
        float menuBarHeight = ImGui::GetFrameHeight();

        // Position the left panel below the main menu bar.
        ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
        // Set the left panel's size to 300 pixels wide and full screen height minus the menu bar.
        ImGui::SetNextWindowSize(ImVec2(300, SCREEN_HEIGHT - menuBarHeight));

        if (ImGui::Begin("Asset Panel", &showAssetList, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            // Calculate the available vertical space in this panel.
            float panelHeight = ImGui::GetContentRegionAvail().y;

            // When an asset is selected, split the panel into two equal halves.
            float assetListHeight = (selectedAssetIndex != -1) ? panelHeight * 0.5f : panelHeight;
            float inspectorHeight = (selectedAssetIndex != -1) ? panelHeight * 0.5f : 0.0f;

            // --- Top Child: Asset List ---
            ImGui::BeginChild("AssetListItems", ImVec2(0, assetListHeight), true);
            if (entityAssets != NULL && entityAssetCount > 0)
            {
                for (int i = 0; i < entityAssetCount; i++)
                {
                    if (ImGui::Selectable(entityAssets[i].name, selectedAssetIndex == i))
                        selectedAssetIndex = i;
                }
            }
            ImGui::EndChild();

            // --- Bottom Child: Asset Inspector (only if an asset is selected) ---
            if (selectedAssetIndex != -1)
            {
                ImGui::Separator();
                ImGui::BeginChild("AssetInspectorRegion", ImVec2(0, inspectorHeight), true);

                // Place a small "X" button at the top right of the inspector.
                float regionWidth = ImGui::GetWindowContentRegionMax().x;
                ImGui::SetCursorPosX(regionWidth - 20);
                bool closeInspector = ImGui::SmallButton("X");

                if (!closeInspector)
                {
                    EntityAsset *asset = &entityAssets[selectedAssetIndex];

                    // Edit the asset name.
                    char nameBuffer[64];
                    strcpy(nameBuffer, asset->name);
                    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                        strcpy(asset->name, nameBuffer);

                    // Edit the asset kind.
                    const char *kinds[] = {"None", "Player", "Enemy", "Boss"};
                    int currentKind = (int)asset->kind;
                    if (ImGui::Combo("Kind", &currentKind, kinds, IM_ARRAYSIZE(kinds)))
                    {
                        TraceLog(LOG_INFO, "Changed Asset %s's type from %d to %d", asset->name, asset->kind, currentKind);
                        asset->kind = (EntityKind)currentKind;
                    }

                    // Edit the physics type.
                    const char *physicsOptions[] = {"Ground", "Flying"};
                    int currentPhysics = (int)asset->physicsType;
                    if (ImGui::Combo("Physics", &currentPhysics, physicsOptions, IM_ARRAYSIZE(physicsOptions)))
                    {
                        asset->physicsType = (PhysicsType)currentPhysics;
                    }

                    // Edit numerical properties.
                    ImGui::InputFloat("Radius", &asset->radius, 0.1f, 1.0f, "%.2f");
                    ImGui::InputInt("Base HP", &asset->baseHp);
                    ImGui::InputFloat("Base Speed", &asset->baseSpeed, 0.1f, 1.0f, "%.2f");
                    ImGui::InputFloat("Attack Speed", &asset->baseAttackSpeed, 0.1f, 1.0f, "%.2f");
                }
                ImGui::EndChild();

                if (closeInspector)
                    selectedAssetIndex = -1;
            }
        }
        ImGui::End();
    }
}

static void DrawEntityInspectorPanel(void)
{
    if (selectedEntityIndex != -1)
    {
        ImGui::Begin("Entity Inspector");
        ImGui::SetWindowPos(ImVec2(SCREEN_WIDTH - 260, SCREEN_HEIGHT / 2 - 50));
        ImGui::SetWindowSize(ImVec2(250, 250));
        if (selectedEntityIndex >= 0)
        {
            Entity *enemy = &gameState->enemies[selectedEntityIndex];
            ImGui::Text("Type: %s", enemy->asset->physicsType == GROUND ? "Ground" : "Flying");
            if (ImGui::Button("Toggle Type"))
                enemy->asset->physicsType = (enemy->asset->physicsType == GROUND) ? FLYING : GROUND;
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
                enemy->asset->kind = EMPTY;
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
}

static void DrawToolInfoPanel(void)
{
    if (ImGui::Begin("ToolInfo", NULL,
                     ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoMove))
    {
        ImGui::SetWindowPos(ImVec2(10, SCREEN_HEIGHT - 40));
        const char *toolText = "";
        if (selectedAssetIndex == -1)
        {
            switch (currentTool)
            {
            case TILE_TOOL_GROUND:
                toolText = "Tilemap Tool: Ground";
                break;
            case TILE_TOOL_DEATH:
                toolText = "Tilemap Tool: Death";
                break;
            case TILE_TOOL_ERASER:
                toolText = "Tilemap Tool: Eraser";
                break;
            default:
                toolText = "";
                break;
            }
        }
        else
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
                toolText = "";
                break;
            }
        }
        ImGui::Text("%s", toolText);
        ImGui::End();
    }
}

static void DrawNoLevelWindow(void)
{
    if (!IsLevelLoaded())
    {
        ImGui::Begin("No Level Loaded", NULL,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove);
        ImGui::SetWindowPos(ImVec2(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 - 50));
        ImGui::SetWindowSize(ImVec2(300, 100));
        ImGui::Text("No level loaded.");
        ImGui::Text("Create or open a file.");
        ImGui::End();
    }
}

static void DrawEditorUI(void)
{
    rlImGuiBegin();

    DrawMainMenuBar();
    DrawNewLevelPopup();
    DrawOverwritePopup();
    DrawAssetListPanel();
    DrawFileListWindow();

    if (!IsLevelLoaded())
    {
        DrawNoLevelWindow();
        rlImGuiEnd();
        return;
    }

    DrawEntityInspectorPanel();
    DrawToolInfoPanel();

    rlImGuiEnd();
}

void DrawEditor(void)
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
