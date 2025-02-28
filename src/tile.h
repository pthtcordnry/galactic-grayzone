#ifndef TILE_H
#define TILE_H

#include <raylib.h>
#include <stdint.h>

#define MAX_TILESETS 32

// Tile physics types.
typedef enum TilePhysicsType {
    TILE_PHYS_NONE = 0,
    TILE_PHYS_GROUND = 1,
    TILE_PHYS_DEATH = 2,
} TilePhysicsType;

typedef struct Tileset {
    uint64_t uniqueId; 
    char name[128];             
    char imagePath[256];   
    Texture2D texture;      
    int tileWidth;          
    int tileHeight;         
    int tilesPerRow;        
    int tilesPerColumn;     
    TilePhysicsType *physicsFlags; 
} Tileset;

extern Tileset *tilesets;
extern int tilesetCount;
extern int selectedTilesetIndex;
extern int selectedTileIndex;
extern int selectedTilePhysics;

void DrawTilesetListPanel();
void DrawSelectedTilesetEditor();

bool SaveTilesetToJson(const char *directory, const char *filename, const Tileset *ts, bool allowOverwrite);
bool SaveAllTilesets(const char *directory, Tileset *tilesets, int count, bool allowOverwrite);
bool LoadTilesetFromJson(const char *filename, Tileset *ts);
bool LoadAllTilesets(const char *directory, Tileset **tilesets, int *count);

#endif
