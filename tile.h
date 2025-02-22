#ifndef TILE_H
#define TILE_H

#include <raylib.h>

#define MAX_TILESETS 32

typedef struct Tileset {
    char name[128];         // A user-friendly name.
    char imagePath[256];    // The file path of the tilesheet.
    Texture2D texture;      // The loaded tilesheet texture.
    int tileWidth;          // Width of one tile in pixels.
    int tileHeight;         // Height of one tile in pixels.
    int tilesPerRow;        // Computed as texture.width / tileWidth.
    int tilesPerColumn;     // Computed as texture.height / tileHeight.
} Tileset;

extern Tileset *tilesets;
extern int tilesetCount;
void DrawTilesetListPanel();
void DrawSelectedTilesetEditor();

// Save/Load functions for tilesets.
bool SaveTilesetToJson(const char *directory, const char *filename, const Tileset *ts, bool allowOverwrite);
bool SaveAllTilesets(const char *directory, Tileset *tilesets, int count, bool allowOverwrite);
bool LoadTilesetFromJson(const char *filename, Tileset *ts);
bool LoadAllTilesets(const char *directory, Tileset **tilesets, int *count);

#endif
