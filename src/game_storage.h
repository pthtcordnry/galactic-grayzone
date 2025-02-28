#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include "entity.h"
#include "game_state.h"
#include "game_rendering.h"

#define MAX_TEXTURE_CACHE 64

// Structure for caching textures.
typedef struct TextureCacheEntry {
    char path[128];
    Texture2D texture;
} TextureCacheEntry;

// Texture cache functions.
Texture2D GetCachedTexture(const char *path);
void AddTextureToCache(const char *path, Texture2D texture);

// Asset Loading & Saving.
bool LoadEntityAssetFromJson(const char *filename, EntityAsset *asset);
bool LoadEntityAssets(const char *directory, EntityAsset **assets, int *count);
bool SaveEntityAssetToJson(const char *directory, const char *filename, const EntityAsset *asset, bool allowOverride);
bool SaveAllEntityAssets(const char *directory, EntityAsset *assets, int count, bool allowOverride);

// Level Loading & Saving.
void LoadLevelFiles();
bool SaveLevel(const char *filename, int **mapTiles, Entity player, Entity *enemies, Entity bossEnemy);
bool LoadLevel(const char *filename, int ***mapTiles, Entity *player, Entity **enemies, int *enemyCount, 
               Entity *bossEnemy, Vector2 **checkpoints, int *checkpointCount);

// Checkpoint Save/Load.
bool SaveCheckpointState(const char *filename, Entity player, Entity *enemies, Entity bossEnemy, 
                         Vector2 checkpoints[], int checkpointCount, int currentIndex);
bool LoadCheckpointState(const char *filename, Entity *player, Entity **enemies, Entity *bossEnemy, 
                         Vector2 checkpoints[], int *checkpointCount);

#endif
