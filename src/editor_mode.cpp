#include "editor_mode.h"
#include <raylib.h>
#include <string.h>
#include <imgui.h>
#include <rlImGui.h>
#include <math.h>
#include "file_io.h"
#include "game_storage.h"
#include "tile.h"
#include "animation.h"

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

static bool showFileList = false;
static bool showAssetList = true;
static bool showOverwritePopup = false;
static bool showNewLevelPopup = false;
static bool isPainting = false;

static int selectedFileIndex = -1;
static int selectedAssetIndex = -1;
static int selectedEntityIndex = -1;
static int selectedCheckpointIndex = -1;
static int boundType = -1; // 0 = left, 1 = right

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
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        return;

    bool hitObject = false;

    // Pick enemy
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
    // Pick boss
    if (!hitObject && gameState->bossEnemy.kind != EMPTY)
    {
        float dx = screenPos.x - gameState->bossEnemy.basePos.x;
        float dy = screenPos.y - gameState->bossEnemy.basePos.y;
        if ((dx * dx + dy * dy) <= (gameState->bossEnemy.radius * gameState->bossEnemy.radius))
        {
            selectedEntityIndex = -2;
            hitObject = true;
        }
    }
    // Pick player
    if (!hitObject && gameState->player.kind != EMPTY)
    {
        float dx = screenPos.x - gameState->player.basePos.x;
        float dy = screenPos.y - gameState->player.basePos.y;
        if ((dx * dx + dy * dy) <= (gameState->player.radius * gameState->player.radius))
        {
            selectedEntityIndex = -3;
            hitObject = true;
        }
    }
    // Pick checkpoint
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
    // Check bounds for selected entity
    if (!hitObject && selectedEntityIndex != -1)
    {
        Entity *e = NULL;
        if (selectedEntityIndex == -2)
            e = &gameState->bossEnemy;
        else if (selectedEntityIndex >= 0)
            e = &gameState->enemies[selectedEntityIndex];

        if (e != NULL)
        {
            const float pickThreshold = 5.0f;
            float topY = e->basePos.y - 20;
            float bottomY = e->basePos.y + 20;
            float lbX = e->leftBound;
            bool onLeftBound = (fabsf(screenPos.x - lbX) < pickThreshold) && (screenPos.y >= topY && screenPos.y <= bottomY);
            float rbX = e->rightBound;
            bool onRightBound = (fabsf(screenPos.x - rbX) < pickThreshold) && (screenPos.y >= topY && screenPos.y <= bottomY);
            if (onLeftBound)
            {
                boundType = 0;
                hitObject = true;
                dragOffset.x = screenPos.x - lbX;
                dragOffset.y = 0;
            }
            else if (onRightBound)
            {
                boundType = 1;
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

void DoEntityDrag(Vector2 screenPos)
{
    if (selectedEntityIndex >= 0 && boundType == -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->enemies[selectedEntityIndex].basePos.x = screenPos.x - dragOffset.x;
            gameState->enemies[selectedEntityIndex].basePos.y = screenPos.y - dragOffset.y;
            gameState->enemies[selectedEntityIndex].position = gameState->enemies[selectedEntityIndex].basePos;
        }
    }
    else if (selectedEntityIndex == -2 && boundType == -1)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->bossEnemy.basePos.x = screenPos.x - dragOffset.x;
            gameState->bossEnemy.basePos.y = screenPos.y - dragOffset.y;
            gameState->bossEnemy.position = gameState->bossEnemy.basePos;
        }
    }
    else if (selectedEntityIndex == -3)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            gameState->player.basePos.x = screenPos.x - dragOffset.x;
            gameState->player.basePos.y = screenPos.y - dragOffset.y;
            gameState->player.position = gameState->player.basePos;
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
            Entity *e = (selectedEntityIndex == -2) ? &gameState->bossEnemy : &gameState->enemies[selectedEntityIndex];
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

void DoEntityCreation(Vector2 screenPos)
{
    if (selectedAssetIndex == -1)
        return;

    if (selectedEntityIndex == -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        EntityAsset *asset = &entityAssets[selectedAssetIndex];
        Entity newInstance = {0};

        newInstance.assetId = asset->id;
        newInstance.kind = asset->kind;
        newInstance.physicsType = asset->physicsType;
        newInstance.radius = asset->baseRadius;
        newInstance.health = asset->baseHp;
        newInstance.speed = asset->baseSpeed;
        newInstance.shootCooldown = asset->baseAttackSpeed;
        newInstance.basePos = screenPos;
        newInstance.position = screenPos;
        newInstance.direction = -1;
        newInstance.velocity = (Vector2){0, 0};
        newInstance.state = ENTITY_STATE_IDLE;
        InitEntityAnimation(&newInstance.idle, &asset->idle, asset->texture);
        InitEntityAnimation(&newInstance.walk, &asset->walk, asset->texture);
        InitEntityAnimation(&newInstance.ascend, &asset->ascend, asset->texture);
        InitEntityAnimation(&newInstance.fall, &asset->fall, asset->texture);
        InitEntityAnimation(&newInstance.shoot, &asset->shoot, asset->texture);
        InitEntityAnimation(&newInstance.die, &asset->die, asset->texture);

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
                gameState->enemies = (Entity *)arena_alloc(&gameArena, sizeof(Entity));
                gameState->enemies[0] = newInstance;
                selectedEntityIndex = 0;
            }
            else
            {
                int newCount = gameState->enemyCount + 1;
                gameState->enemies = (Entity *)arena_realloc(&gameArena, gameState->enemies, newCount * sizeof(Entity));
                gameState->enemies[gameState->enemyCount] = newInstance;
                selectedEntityIndex = gameState->enemyCount;
                gameState->enemyCount = newCount;
            }
        }
        else if (asset->kind == ENTITY_BOSS)
        {
            gameState->bossEnemy = newInstance;
            selectedEntityIndex = -2;
        }
        else if (asset->kind == ENTITY_PLAYER)
        {
            gameState->player = newInstance;
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
                if (tileX >= 0 && tileX < currentMapWidth && tileY >= 0 && tileY < currentMapHeight)
                {
                    if (currentTool == TILE_TOOL_ERASER)
                    {
                        mapTiles[tileY][tileX] = 0;
                    }
                    else
                    {
                        if (selectedTilesetIndex >= 0 && selectedTileIndex >= 0)
                        {
                            int compositeId = (((selectedTilesetIndex + 1) & 0xFFF) << 20) |
                                              (((selectedTilePhysics) & 0xF) << 16) |
                                              ((selectedTileIndex + 1) & 0xFFFF);
                            mapTiles[tileY][tileX] = compositeId;
                        }
                        else
                        {
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

static void DrawNewLevelPopup()
{
    if (showNewLevelPopup)
    {
        ImGui::OpenPopup("New Level");
        if (ImGui::BeginPopupModal("New Level", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::SetWindowPos(ImVec2(10, 10));
            ImGui::SetWindowSize(ImVec2(300, 150));
            static char tempLevelName[128] = "";
            static int newMapWidth = 60;
            static int newMapHeight = 16;

            ImGui::InputText("Level Name (.level)", tempLevelName, sizeof(tempLevelName));
            ImGui::InputInt("Map Width (tiles)", &newMapWidth);
            ImGui::InputInt("Map Height (tiles)", &newMapHeight);

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
                    {
                        snprintf(fixedName, sizeof(fixedName), "%s.level", tempLevelName);
                    }
                }
                else
                {
                    strcpy(fixedName, tempLevelName);
                }

                arena_reset(&gameArena);
                gameState = (GameState *)arena_alloc(&gameArena, sizeof(GameState));
                memset(gameState, 0, sizeof(GameState));
                gameState->currentState = EDITOR;
                strcpy(gameState->currentLevelFilename, fixedName);
                mapTiles = InitializeTilemap(newMapWidth, newMapHeight);
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
                strcpy(gameState->currentLevelFilename, fullPath);
                if (!LoadLevel(gameState->currentLevelFilename, &mapTiles, &gameState->player,
                               &gameState->enemies, &gameState->enemyCount,
                               &gameState->bossEnemy, &gameState->checkpoints, &gameState->checkpointCount))
                    TraceLog(LOG_ERROR, "Failed to load level: %s", gameState->currentLevelFilename);
                else
                    TraceLog(LOG_INFO, "Loaded level: %s", gameState->currentLevelFilename);
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
                    char displayName[128];
                    if (strlen(entityAssets[i].name) == 0)
                        sprintf(displayName, "UnnamedAsset_%llu", (unsigned long long)entityAssets[i].id);
                    else
                        strcpy(displayName, entityAssets[i].name);
                    if (ImGui::Selectable(displayName, selectedAssetIndex == i))
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
                bool closeInspector = false;
                AnimationFrames *animFrames = NULL;
                EntityAsset *asset = NULL;
                if (ImGui::SmallButton("X"))
                {
                    closeInspector = true;
                    selectedAssetIndex = -1;
                }
                if (!closeInspector)
                {
                    asset = &entityAssets[selectedAssetIndex];
                    static int selectedAnim = 0;
                    if (asset->texture.id != 0)
                    {
                        if (ImGui::Begin("Sprite Sheet Preview"))
                        {
                            ImGui::Image((ImTextureID)(intptr_t)&asset->texture,
                                         ImVec2((float)asset->texture.width, (float)asset->texture.height));
                            ImVec2 imagePosMin = ImGui::GetItemRectMin();
                            float scale = 1.0f;
                            switch (selectedAnim)
                            {
                            case 0:
                                animFrames = &asset->idle;
                                break;
                            case 1:
                                animFrames = &asset->walk;
                                break;
                            case 2:
                                animFrames = &asset->ascend;
                                break;
                            case 3:
                                animFrames = &asset->fall;
                                break;
                            case 4:
                                animFrames = &asset->shoot;
                                break;
                            case 5:
                                animFrames = &asset->die;
                                break;
                            }
                            if (animFrames && animFrames->frames)
                            {
                                ImDrawList *drawList = ImGui::GetWindowDrawList();
                                for (int i = 0; i < animFrames->frameCount; i++)
                                {
                                    Rectangle r = animFrames->frames[i];
                                    ImVec2 rectMin = ImVec2(imagePosMin.x + r.x * scale,
                                                            imagePosMin.y + r.y * scale);
                                    ImVec2 rectMax = ImVec2(rectMin.x + r.width * scale,
                                                            rectMin.y + r.height * scale);
                                    drawList->AddRect(rectMin, rectMax, IM_COL32(255, 0, 0, 255));
                                }
                            }
                            ImGui::End();
                        }
                    }
                    // Asset properties
                    char nameBuffer[64];
                    strcpy(nameBuffer, asset->name);
                    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
                        strcpy(asset->name, nameBuffer);
                    static const char *entityKindOptions[] = {"Empty", "Player", "Enemy", "Boss"};
                    int kindIndex = (int)asset->kind;
                    if (ImGui::Combo("Entity Kind", &kindIndex, entityKindOptions, IM_ARRAYSIZE(entityKindOptions)))
                        asset->kind = (EntityKind)kindIndex;
                    static const char *physicsTypeOptions[] = {"None", "Ground", "Flying"};
                    int physIndex = (int)asset->physicsType;
                    if (ImGui::Combo("Physics Type", &physIndex, physicsTypeOptions, IM_ARRAYSIZE(physicsTypeOptions)))
                        asset->physicsType = (PhysicsType)physIndex;
                    ImGui::InputFloat("Base Radius", &asset->baseRadius);
                    ImGui::InputInt("Base HP", &asset->baseHp);
                    ImGui::InputFloat("Base Speed", &asset->baseSpeed);
                    ImGui::InputFloat("Base Attack Speed", &asset->baseAttackSpeed);
                    char texturePathBuffer[128];
                    strcpy(texturePathBuffer, asset->texturePath);
                    if (ImGui::InputText("Texture Path", texturePathBuffer, sizeof(texturePathBuffer)))
                        strcpy(asset->texturePath, texturePathBuffer);
                    if (ImGui::Button("Load Sprite Sheet"))
                    {
                        if (strlen(asset->texturePath) > 0)
                        {
                            Texture2D cached = GetCachedTexture(asset->texturePath);
                            if (cached.id != 0)
                                asset->texture = cached;
                            else
                            {
                                asset->texture = LoadTexture(asset->texturePath);
                                if (asset->texture.id != 0)
                                    AddTextureToCache(asset->texturePath, asset->texture);
                                else
                                    TraceLog(LOG_WARNING, "Failed to load texture for asset %s from path %s", asset->name, asset->texturePath);
                            }
                        }
                    }
                    static const char *animTypes[] = {"Idle", "Walk", "Ascend", "Fall", "Shoot", "Die"};
                    ImGui::Combo("Animation", &selectedAnim, animTypes, IM_ARRAYSIZE(animTypes));
                    switch (selectedAnim)
                    {
                    case 0:
                        animFrames = &asset->idle;
                        break;
                    case 1:
                        animFrames = &asset->walk;
                        break;
                    case 2:
                        animFrames = &asset->ascend;
                        break;
                    case 3:
                        animFrames = &asset->fall;
                        break;
                    case 4:
                        animFrames = &asset->shoot;
                        break;
                    case 5:
                        animFrames = &asset->die;
                        break;
                    }
                    if (animFrames)
                    {
                        if (ImGui::InputInt("Frame Count", &animFrames->frameCount))
                        {
                            if (animFrames->frames)
                                animFrames->frames = (Rectangle *)arena_realloc(&assetArena, animFrames->frames, sizeof(Rectangle) * animFrames->frameCount);
                            else
                                animFrames->frames = (Rectangle *)arena_alloc(&assetArena, sizeof(Rectangle) * animFrames->frameCount);
                        }
                        ImGui::InputFloat("Frame Time", &animFrames->frameTime);
                        if (animFrames->frameCount > 0 && animFrames->frames != NULL)
                        {
                            ImGui::Text("x y width height");
                            for (int i = 0; i < animFrames->frameCount; i++)
                            {
                                ImGui::PushID(i);
                                char label[16];
                                sprintf(label, "Frame: %d", i);
                                ImGui::InputFloat4(label, (float *)&animFrames->frames[i]);
                                ImGui::PopID();
                            }
                        }
                    }
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
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
            ImGui::Text("Boss HP: %d", gameState->bossEnemy.health);
            if (ImGui::Button("+"))
                gameState->bossEnemy.health++;
            ImGui::SameLine();
            if (ImGui::Button("-") && gameState->bossEnemy.health > 0)
                gameState->bossEnemy.health--;
            ImGui::Text("Pos: %.0f, %.0f", gameState->bossEnemy.basePos.x, gameState->bossEnemy.basePos.y);
        }
        else if (selectedEntityIndex == -3)
        {
            ImGui::Text("Player HP: %d", gameState->player.health);
            if (ImGui::Button("+"))
                gameState->player.health++;
            ImGui::SameLine();
            if (ImGui::Button("-") && gameState->player.health > 0)
                gameState->player.health--;
            ImGui::Text("Pos: %.0f, %.0f", gameState->player.basePos.x, gameState->player.basePos.y);
        }
        ImGui::End();
    }
}

static void DrawToolInfoPanel()
{
    if (ImGui::Begin("ToolInfo", NULL,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
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
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
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

    Vector2 mousePos = GetMousePosition();
    Vector2 screenPos = GetScreenToWorld2D(mousePos, camera);

    DrawEntities(0, screenPos, &gameState->player, gameState->enemies,
                 gameState->enemyCount, &gameState->bossEnemy, 0, true);

    if (gameState->checkpoints)
    {
        for (int i = 0; i < gameState->checkpointCount; i++)
        {
            DrawRectangle(gameState->checkpoints[i].x, gameState->checkpoints[i].y,
                          TILE_SIZE, TILE_SIZE * 2, Fade(GREEN, 0.3f));
        }
    }
    if (selectedEntityIndex != -1 && selectedEntityIndex != -3)
    {
        Entity *e = (selectedEntityIndex == -2) ? &gameState->bossEnemy : &gameState->enemies[selectedEntityIndex];
        float topY = e->basePos.y - 20;
        float bottomY = e->basePos.y + 20;
        DrawLine((int)e->leftBound, (int)topY, (int)e->leftBound, (int)bottomY, BLUE);
        DrawLine((int)e->rightBound, (int)topY, (int)e->rightBound, (int)bottomY, BLUE);
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
                showNewLevelPopup = true;
                ImGui::OpenPopup("New Level");
            }
            if (ImGui::MenuItem("Open"))
            {
                LoadLevelFiles();
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
                    TraceLog(LOG_ERROR, "Failed to save tilesets!");
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
                            entityAssets = (EntityAsset *)arena_alloc(&assetArena, sizeof(EntityAsset) * (entityAssetCount + 1));
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
                        TraceLog(LOG_ERROR, "EDITOR: Failed to load entity assets");
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
            if (ImGui::BeginMenu("Checkpoint"))
            {
                if (ImGui::MenuItem("Add Checkpoint"))
                {
                    Vector2 cp = camera.target;

                    // If no checkpoint array exists, allocate one; otherwise, resize.
                    if (gameState->checkpoints == NULL)
                    {
                        gameState->checkpointCount = 1;
                        gameState->checkpoints = (Vector2 *)arena_alloc(&gameArena, sizeof(Vector2));
                    }
                    else
                    {
                        int newCount = gameState->checkpointCount + 1;
                        gameState->checkpoints = (Vector2 *)arena_realloc(&gameArena, gameState->checkpoints, newCount * sizeof(Vector2));
                        gameState->checkpointCount = newCount;
                    }
                    gameState->checkpoints[gameState->checkpointCount - 1] = cp;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        float windowWidth = ImGui::GetWindowWidth();
        float buttonWidth = 120.0f;
        float offset = windowWidth - buttonWidth - 10.0f;
        ImGui::SetCursorPosX(offset);
        if (gameState->currentState != EDITOR)
        {
            if (ImGui::Button("Stop", ImVec2(buttonWidth, 0)))
            {
                if (!LoadLevel(gameState->currentLevelFilename, &mapTiles,
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
                                         gameState->checkpoints, &gameState->checkpointCount, &gameState->currentCheckpointIndex))
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
