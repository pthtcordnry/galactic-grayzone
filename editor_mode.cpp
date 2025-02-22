#include "editor_mode.h"
#include <raylib.h>
#include <string.h>
#include <imgui.h>
#include <rlImGui.h>
#include "file_io.h"
#include "main.h"
#include "game_storage.h"
#include "tile.h"

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

static bool IsLevelLoaded()
{
    return (gameState->currentLevelFilename[0] != '\0');
}

static uint64_t GenerateRandomUInt()
{
    uint64_t hi = ((uint64_t)rand() << 32) ^ (uint64_t)rand();
    uint64_t lo = ((uint64_t)rand() << 32) ^ (uint64_t)rand();
    return (hi << 32) ^ lo;
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
        // Use basePos for picking in editor mode.
        if (gameState->enemies != NULL)
        {
            for (int i = 0; i < gameState->enemyCount; i++)
            {
                float dx = screenPos.x - gameState->enemies[i].basePos.x;
                float dy = screenPos.y - gameState->enemies[i].basePos.y;
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
            float dx = screenPos.x - gameState->bossEnemy->basePos.x;
            float dy = screenPos.y - gameState->bossEnemy->basePos.y;
            if ((dx * dx + dy * dy) <= (gameState->bossEnemy->radius * gameState->bossEnemy->radius))
            {
                selectedEntityIndex = -2; // boss
                hitObject = true;
            }
        }
        if (!hitObject && gameState->player != NULL)
        {
            float dx = screenPos.x - gameState->player->basePos.x;
            float dy = screenPos.y - gameState->player->basePos.y;
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
        if (!hitObject && selectedEntityIndex != -1)
        {
            // Identify which entity is selected
            Entity *e = NULL;
            if (selectedEntityIndex == -2) // boss
                e = gameState->bossEnemy;
            else if (selectedEntityIndex >= 0) // normal enemy
                e = &gameState->enemies[selectedEntityIndex];

            if (e != NULL)
            {
                const float pickThreshold = 5.0f;
                // Use basePos for calculating bounds in editor mode.
                float topY = e->basePos.y - 20;
                float bottomY = e->basePos.y + 20;

                // leftBound
                float lbX = e->leftBound;
                bool onLeftBound =
                    (fabsf(screenPos.x - lbX) < pickThreshold) &&
                    (screenPos.y >= topY && screenPos.y <= bottomY);

                // rightBound
                float rbX = e->rightBound;
                bool onRightBound =
                    (fabsf(screenPos.x - rbX) < pickThreshold) &&
                    (screenPos.y >= topY && screenPos.y <= bottomY);

                if (onLeftBound)
                {
                    boundType = 0; // left
                    hitObject = true;
                    dragOffset.x = screenPos.x - lbX;
                    dragOffset.y = 0;
                }
                else if (onRightBound)
                {
                    boundType = 1; // right
                    hitObject = true;
                    dragOffset.x = screenPos.x - rbX;
                    dragOffset.y = 0;
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
    // Update basePos when dragging in editor mode.
    if (selectedEntityIndex >= 0 && boundType == -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->enemies[selectedEntityIndex].basePos.x = screenPos.x - dragOffset.x;
            gameState->enemies[selectedEntityIndex].basePos.y = screenPos.y - dragOffset.y;
        }
    }
    else if (selectedEntityIndex == -2 && boundType == -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->bossEnemy->basePos.x = screenPos.x - dragOffset.x;
            gameState->bossEnemy->basePos.y = screenPos.y - dragOffset.y;
        }
    }
    else if (selectedEntityIndex == -3)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->player->basePos.x = screenPos.x - dragOffset.x;
            gameState->player->basePos.y = screenPos.y - dragOffset.y;
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
    if (selectedEntityIndex != -1 && boundType != -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            float newX = screenPos.x - dragOffset.x;
            Entity *e = NULL;
            if (selectedEntityIndex == -2) // boss
                e = gameState->bossEnemy;
            else if (selectedEntityIndex >= 0) // normal enemy
                e = &gameState->enemies[selectedEntityIndex];

            if (boundType == 0)
                e->leftBound = newX;
            else
                e->rightBound = newX;
        }
        else
        {
            boundType = -1;
        }
    }
}

///
/// DoEntityCreation: Create a new instance from the selected asset.
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
        newInstance.assetId = asset->id;
        newInstance.kind = asset->kind;
        newInstance.physicsType = asset->physicsType;
        newInstance.radius = asset->baseRadius;
        newInstance.health = asset->baseHp;
        newInstance.speed = asset->baseSpeed;
        newInstance.shootCooldown = asset->baseAttackSpeed;
        // Set both basePos and runtime position initially
        newInstance.basePos = screenPos;
        newInstance.position = screenPos;
        newInstance.velocity = (Vector2){0, 0};

        if (asset->kind != ENTITY_PLAYER)
        {
            newInstance.leftBound = screenPos.x - 50;
            newInstance.rightBound = screenPos.x + 50;
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
    // Only paint if no entity or checkpoint is selected.
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
                    // If the eraser tool is active, clear the tile.
                    if (currentTool == TILE_TOOL_ERASER)
                    {
                        mapTiles[tileY][tileX] = 0;
                    }
                    else
                    {
                        if (selectedTilesetIndex >= 0 && selectedTileIndex >= 0)
                        {
                            // Pack the composite id as follows:
                            // Bits 31-20: (selectedTilesetIndex + 1) [12 bits]
                            // Bits 19-16: (selectedTilePhysics)     [4 bits]
                            // Bits 15-0 : (selectedTileIndex + 1)     [16 bits]
                            int compositeId = (((selectedTilesetIndex + 1) & 0xFFF) << 20) | (((selectedTilePhysics) & 0xF) << 16) | ((selectedTileIndex + 1) & 0xFFFF);
                            mapTiles[tileY][tileX] = compositeId;
                        }
                        else
                        {
                            // Fallback: if no tile is selected, you may choose a default.
                            // Here we use the legacy ground tile value (e.g. 1).
                            mapTiles[tileY][tileX] = 1;
                        }

                        TraceLog(LOG_INFO, "Painted the tile using: %d", mapTiles[tileY][tileX]);
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

//
// DRAWING UI PANELS
//
static void DrawNewLevelPopup()
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

static void DrawOverwritePopup()
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

static void DrawFileListWindow()
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

static void DrawAssetListPanel()
{
    if (showAssetList)
    {
        float menuBarHeight = ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(ImVec2(0, menuBarHeight));
        ImGui::SetNextWindowSize(ImVec2(300, SCREEN_HEIGHT - menuBarHeight));

        if (ImGui::Begin("Asset Panel", &showAssetList, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
        {
            float panelHeight = ImGui::GetContentRegionAvail().y;
            float assetListHeight = (selectedAssetIndex != -1) ? panelHeight * 0.5f : panelHeight;
            float inspectorHeight = (selectedAssetIndex != -1) ? panelHeight * 0.5f : 0.0f;

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

            if (selectedAssetIndex != -1)
            {
                ImGui::Separator();
                ImGui::BeginChild("AssetInspectorRegion", ImVec2(0, inspectorHeight), true);

                float regionWidth = ImGui::GetWindowContentRegionMax().x;
                ImGui::SetCursorPosX(regionWidth - 20);
                bool closeInspector = ImGui::SmallButton("X");

                if (!closeInspector)
                {
                    EntityAsset *asset = &entityAssets[selectedAssetIndex];
                    char nameBuffer[64];
                    strcpy(nameBuffer, asset->name);
                    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                        strcpy(asset->name, nameBuffer);

                    const char *kinds[] = {"None", "Player", "Enemy", "Boss"};
                    int currentKind = (int)asset->kind;
                    if (ImGui::Combo("Kind", &currentKind, kinds, IM_ARRAYSIZE(kinds)))
                    {
                        TraceLog(LOG_INFO, "Changed Asset %s's type from %d to %d", asset->name, asset->kind, currentKind);
                        asset->kind = (EntityKind)currentKind;
                    }

                    const char *physicsOptions[] = {"None", "Ground", "Flying"};
                    int currentPhysics = (int)asset->physicsType;
                    if (ImGui::Combo("Physics", &currentPhysics, physicsOptions, IM_ARRAYSIZE(physicsOptions)))
                    {
                        asset->physicsType = (PhysicsType)currentPhysics;
                    }

                    ImGui::InputFloat("Radius", &asset->baseRadius, 0.1f, 1.0f, "%.2f");
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

static void DrawEntityInspectorPanel()
{
    if (selectedEntityIndex != -1)
    {
        ImGui::Begin("Entity Inspector");
        ImGui::SetWindowPos(ImVec2(SCREEN_WIDTH - 260, SCREEN_HEIGHT / 2 - 50));
        ImGui::SetWindowSize(ImVec2(250, 250));
        if (selectedEntityIndex >= 0)
        {
            Entity *enemy = &gameState->enemies[selectedEntityIndex];
            ImGui::Text("Type: %s", enemy->physicsType == PHYS_GROUND ? "Ground" : "Flying");
            if (ImGui::Button("Toggle Type"))
                enemy->physicsType = (enemy->physicsType == PHYS_GROUND) ? PHYS_FLYING : PHYS_GROUND;
            ImGui::Text("Health: %d", enemy->health);
            if (ImGui::Button("+"))
                enemy->health++;
            ImGui::SameLine();
            if (ImGui::Button("-") && enemy->health > 0)
                enemy->health--;
            // Display the basePos for editor mode.
            ImGui::Text("Pos: %.0f, %.0f", enemy->basePos.x, enemy->basePos.y);
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
            ImGui::Text("Pos: %.0f, %.0f", gameState->bossEnemy->basePos.x, gameState->bossEnemy->basePos.y);
        }
        else if (selectedEntityIndex == -3)
        {
            ImGui::Text("Player HP: %d", gameState->player->health);
            if (ImGui::Button("+"))
                gameState->player->health++;
            ImGui::SameLine();
            if (ImGui::Button("-") && gameState->player->health > 0)
                gameState->player->health--;
            ImGui::Text("Pos: %.0f, %.0f", gameState->player->basePos.x, gameState->player->basePos.y);
        }
        ImGui::End();
    }
}

static void DrawToolInfoPanel()
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

static void DrawNoLevelWindow()
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

static void DrawEditorUI()
{
    DrawNewLevelPopup();
    DrawOverwritePopup();
    DrawAssetListPanel();
    DrawFileListWindow();

    if (!IsLevelLoaded())
    {
        DrawNoLevelWindow();
        return;
    }

    DrawEntityInspectorPanel();
    DrawToolInfoPanel();
}

static void DrawEditorWorldspace()
{
    BeginMode2D(camera);
    DrawTilemap(&camera);

    // Draw checkpoints
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
    // Draw enemies using basePos
    if (gameState->enemies)
    {
        for (int i = 0; i < gameState->enemyCount; i++)
        {
            if (gameState->enemies[i].physicsType == PHYS_GROUND)
            {
                float halfSide = gameState->enemies[i].radius;
                DrawRectangle((int)(gameState->enemies[i].basePos.x - halfSide),
                              (int)(gameState->enemies[i].basePos.y - halfSide),
                              (int)(halfSide * 2),
                              (int)(halfSide * 2),
                              RED);
            }
            else if (gameState->enemies[i].physicsType == PHYS_FLYING)
            {
                DrawPoly(gameState->enemies[i].basePos,
                         4,
                         gameState->enemies[i].radius,
                         45.0f,
                         ORANGE);
            }
        }
    }
    // Draw boss using basePos
    if (gameState->bossEnemy)
    {
        DrawCircleV(gameState->bossEnemy->basePos,
                    gameState->bossEnemy->radius,
                    PURPLE);
    }
    // Draw player using basePos
    if (gameState->player)
    {
        DrawCircleV(gameState->player->basePos,
                    gameState->player->radius,
                    BLUE);
        DrawText("PLAYER",
                 (int)(gameState->player->basePos.x - 20),
                 (int)(gameState->player->basePos.y - gameState->player->radius - 20),
                 12,
                 BLACK);
    }
    // Draw bounds for selected entity (using basePos for reference)
    if (selectedEntityIndex != -1 && selectedEntityIndex != -3)
    {
        Entity *e = NULL;

        if (selectedEntityIndex == -2)
        {
            e = gameState->bossEnemy;
        }
        else if (selectedEntityIndex >= 0)
        {
            e = &gameState->enemies[selectedEntityIndex];
        }

        float topY = e->basePos.y - 20;
        float bottomY = e->basePos.y + 20;

        DrawLine((int)e->leftBound, (int)topY,
                 (int)e->leftBound, (int)bottomY,
                 BLUE);
        DrawLine((int)e->rightBound, (int)topY,
                 (int)e->rightBound, (int)bottomY,
                 BLUE);
    }

    EndMode2D();
}

void DrawMainMenuBar()
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
                    ListFilesInDirectory(levelsDir, levelExtension, levelFiles, levelFileCount);
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

                if (!SaveAllTilesets("./tilesets/", tilesets, tilesetCount, true))
                {
                    TraceLog(LOG_ERROR, "Failed to save tilesets!");
                }
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
                        newAsset.id = GenerateRandomUInt();
                        newAsset.kind = EMPTY;
                        newAsset.physicsType = PHYS_NONE;
                        newAsset.baseRadius = 0;
                        strcpy(newAsset.name, "New Asset");
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

        float windowWidth = ImGui::GetWindowWidth();
        float buttonWidth = 120.0f;                       // Adjust as needed.
        float offset = windowWidth - buttonWidth - 10.0f; // 10 pixels padding.
        ImGui::SetCursorPosX(offset);

        // Toggle label based on current state.
        if (gameState->currentState == PLAY)
        {
            if (ImGui::Button("Stop", ImVec2(buttonWidth, 0)))
            {
                // Switch to editor mode.
                if (!LoadLevel(gameState->currentLevelFilename, mapTiles,
                               &gameState->player, &gameState->enemies, &gameState->enemyCount,
                               &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                    TraceLog(LOG_ERROR, "Failed to reload level for editor mode!");
                gameState->currentState = EDITOR;
            }
        }
        else
        {
            if (ImGui::Button("Play", ImVec2(buttonWidth, 0)))
            {
                if (!LoadCheckpointState(CHECKPOINT_FILE, &gameState->player,
                                         &gameState->enemies, &gameState->bossEnemy,
                                         gameState->checkpoints, &gameState->checkpointCount))
                    TraceLog(LOG_WARNING, "Failed to load checkpoint in init state.");
                gameState->currentState = PLAY;
            }
        }
        ImGui::EndMainMenuBar();
    }
}

void DrawEditor()
{
    Vector2 mousePos = GetMousePosition();
    Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

    TickInput();
    DrawEditorWorldspace();
    DrawEditorUI();
    DrawTilesetListPanel();
    DrawSelectedTilesetEditor();

    if (ImGui::GetIO().WantCaptureMouse || !IsLevelLoaded())
        return;

    DoEntityPicking(screenPos);
    DoEntityDrag(screenPos);
    DoEntityCreation(screenPos);
    DoTilePaint(screenPos);
}
