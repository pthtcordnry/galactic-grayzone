#include <stdio.h>
#include <string.h>
#include <raylib.h>
#include "tile.h"
#include "file_io.h"
#include "memory_arena.h"
#include "game_state.h"
#include "imgui.h"
#include "game_storage.h"

int selectedTilesetIndex = -1;
int selectedTileIndex = -1;
int selectedTilePhysics = TILE_PHYS_GROUND;
int tilesetCount = 0;
Tileset *tilesets = NULL;

void DrawTilesetListPanel()
{
    ImGui::Begin("Tilesets");

    if (tilesets != NULL)
    {
        for (int i = 0; i < tilesetCount; i++)
        {
            if (ImGui::Selectable(tilesets[i].name, selectedTilesetIndex == i))
                selectedTilesetIndex = i;
        }
    }
    if (ImGui::Button("New Tileset"))
        ImGui::OpenPopup("New Tileset Popup");

    static char newTilesetPath[256] = "";
    static int newTileWidth = 32;
    static int newTileHeight = 32;
    static char newTilesetName[128] = "New Tileset Name";

    if (ImGui::BeginPopupModal("New Tileset Popup", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Image Path", newTilesetPath, sizeof(newTilesetPath));
        ImGui::InputInt("Tile Width", &newTileWidth);
        ImGui::InputInt("Tile Height", &newTileHeight);
        ImGui::InputText("Name", newTilesetName, sizeof(newTilesetName));
        if (ImGui::Button("Create"))
        {
            Texture2D tex = LoadTextureWithCache(newTilesetPath);
            if (tex.id == 0)
            {
                TraceLog(LOG_WARNING, "Failed to load texture from path %s", newTilesetPath);
            }
            if (tex.id > 0)
            {
                Tileset ts;
                ts.uniqueId = GenerateRandomUInt() & 0xFFF;
                strncpy(ts.name, newTilesetName, sizeof(ts.name));
                strncpy(ts.imagePath, newTilesetPath, sizeof(ts.imagePath));
                ts.texture = tex;
                ts.tileWidth = newTileWidth;
                ts.tileHeight = newTileHeight;
                ts.tilesPerRow = tex.width / newTileWidth;
                ts.tilesPerColumn = tex.height / newTileHeight;
                tilesetCount++;

                if (tilesets == NULL)
                    tilesets = (Tileset *)arena_alloc(&assetArena, sizeof(Tileset) * tilesetCount);
                else
                    tilesets = (Tileset *)arena_realloc(&assetArena, tilesets, sizeof(Tileset) * tilesetCount);

                if (tilesets == NULL)
                {
                    TraceLog(LOG_FATAL, "Failed to allocate tileset memory!");
                    return;
                }
                if (tilesetCount < MAX_TILESETS)
                    tilesets[tilesetCount - 1] = ts;

                int totalTiles = ts.tilesPerRow * ts.tilesPerColumn;
            }
            else
            {
                TraceLog(LOG_ERROR, "Failed to load tilesheet: %s", newTilesetPath);
            }
            newTilesetPath[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

void DrawSelectedTilesetEditor()
{
    if (selectedTilesetIndex < 0 || tilesets == NULL)
        return;

    Tileset *ts = &tilesets[selectedTilesetIndex];
    if (ts->texture.width == 0 || ts->texture.height == 0)
    {
        ImGui::Text("Invalid texture!");
        return;
    }

    ImGui::Begin("Tileset Editor");
    ImTextureID texID = (ImTextureID)(intptr_t)&ts->texture;
    ImVec2 fullSize(ts->texture.width, ts->texture.height);
    ImGui::Text("Tilesheet Preview:");
    ImGui::Image(texID, fullSize);
    ImGui::Separator();
    ImGui::Text("Select a Tile:");

    for (int y = 0; y < ts->tilesPerColumn; y++)
    {
        for (int x = 0; x < ts->tilesPerRow; x++)
        {
            float uv0_x = (float)(x * ts->tileWidth) / ts->texture.width;
            float uv0_y = (float)(y * ts->tileHeight) / ts->texture.height;
            float uv1_x = (float)((x + 1) * ts->tileWidth) / ts->texture.width;
            float uv1_y = (float)((y + 1) * ts->tileHeight) / ts->texture.height;

            ImVec2 buttonSize((float)ts->tileWidth, (float)ts->tileHeight);
            char buttonID[64];
            sprintf(buttonID, "##Tile_%d_%d", x, y);
            if (ImGui::ImageButton(buttonID, texID, buttonSize, ImVec2(uv0_x, uv0_y), ImVec2(uv1_x, uv1_y)))
                selectedTileIndex = y * ts->tilesPerRow + x;

            if (x < ts->tilesPerRow - 1)
                ImGui::SameLine();
        }
    }

    const char *physicsTypes[] = {"None", "Ground", "Death"};
    if (ImGui::Combo("Physics", &selectedTilePhysics, physicsTypes, IM_ARRAYSIZE(physicsTypes)))
        ;
    ImGui::End();
}

bool SaveTilesetToJson(const char *directory, const char *filename, const Tileset *ts, bool allowOverwrite)
{
    if (!allowOverwrite)
    {
        FILE *checkFile = fopen(filename, "r");
        if (checkFile != NULL)
        {
            TraceLog(LOG_ERROR, "File %s already exists, no overwrite allowed!", filename);
            fclose(checkFile);
            return false;
        }
    }
    if (!EnsureDirectoryExists(directory))
    {
        TraceLog(LOG_ERROR, "Directory %s doesn't exist (or can't create)!", directory);
        return false;
    }
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        TraceLog(LOG_ERROR, "Failed to open %s for writing!", filename);
        return false;
    }
    fprintf(file,
            "{\n"
            "    \"name\": \"%s\",\n"
            "    \"imagePath\": \"%s\",\n"
            "    \"tileWidth\": %d,\n"
            "    \"tileHeight\": %d,\n"
            "    \"uniqueId\": %llu\n"
            "}\n",
            ts->name,
            ts->imagePath,
            ts->tileWidth,
            ts->tileHeight,
            ts->uniqueId);
    fclose(file);
    return true;
}

bool SaveAllTilesets(const char *directory, Tileset *tilesets, int count, bool allowOverwrite)
{
    bool error = false;
    for (int i = 0; i < count; i++)
    {
        char filename[256];
        int len = (int)strlen(directory);
        const char *sep = (len > 0 && (directory[len - 1] == '/' || directory[len - 1] == '\\')) ? "" : "/";
        snprintf(filename, sizeof(filename), "%s%s%s.tiles", directory, sep, tilesets[i].name);
        if (!SaveTilesetToJson(directory, filename, &tilesets[i], allowOverwrite))
        {
            TraceLog(LOG_ERROR, "Failed to save tileset: %s", tilesets[i].name);
            error = true;
        }
    }
    return !error;
}

bool LoadTilesetFromJson(const char *filename, Tileset *ts)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        return false;
    }
    char buffer[1024];
    size_t size = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[size] = '\0';
    fclose(file);

    TraceLog(LOG_INFO, "Read file for tileset successfully");
    int ret = sscanf(
        buffer,
        "{\n"
        "    \"name\": \"%127[^\"]\",\n"
        "    \"imagePath\": \"%255[^\"]\",\n"
        "    \"tileWidth\": %d,\n"
        "    \"tileHeight\": %d,\n"
        "    \"uniqueId\": %llu\n"
        "}\n",
        ts->name,
        ts->imagePath,
        &ts->tileWidth,
        &ts->tileHeight,
        &ts->uniqueId);

    if (ret != 5)
    {
        TraceLog(LOG_ERROR, "Tileset parse returned %d instead of 5", ret);
        return false;
    }

    ts->texture = LoadTextureWithCache(ts->imagePath);
    if (ts->texture.id == 0)
        return false;
    ts->tilesPerRow = ts->texture.width / ts->tileWidth;
    ts->tilesPerColumn = ts->texture.height / ts->tileHeight;

    int totalTiles = ts->tilesPerRow * ts->tilesPerColumn;
    return true;
}

bool LoadAllTilesets(const char *directory, Tileset **tilesets, int *count)
{
    char fileList[256][256];
    int numFiles = ListFilesInDirectory(directory, "*.tiles", fileList, 256);
    if (numFiles <= 0)
        return false;

    if (*tilesets == NULL)
        *tilesets = (Tileset *)arena_alloc(&assetArena, sizeof(Tileset) * numFiles);
    else
        *tilesets = (Tileset *)arena_realloc(&assetArena, *tilesets, sizeof(Tileset) * numFiles);

    if (*tilesets == NULL)
    {
        TraceLog(LOG_ERROR, "Failed to allocate memory for tilesets (size %d)", numFiles);
        return false;
    }

    int loadedCount = 0;
    for (int i = 0; i < numFiles; i++)
    {
        char fullPath[256];
        const char *fileName = fileList[i];
        // If the filename starts with a slash or backslash, skip it.
        if (fileName[0] == '\\' || fileName[0] == '/')
            fileName++;

        // Build the full path using the directory and the file name.
        snprintf(fullPath, sizeof(fullPath), "%s%s", directory, fileName);

        if (LoadTilesetFromJson(fullPath, &((*tilesets)[loadedCount])))
            loadedCount++;
        else
            TraceLog(LOG_ERROR, "Failed to load tileset from file: %s", fullPath);
    }
    *count = loadedCount;
    return true;
}
