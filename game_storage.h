#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include "entity.h"
#include "game_state.h"
#include "game_rendering.h"

// Asset Loading & Saving
bool LoadEntityAssetFromJson(const char *filename, EntityAsset *asset);
bool LoadEntityAssets(const char *directory, EntityAsset **assets, int *count);
bool SaveEntityAssetToJson(const char *directory, const char *filename, const EntityAsset *asset, bool allowOverride);
bool SaveAllEntityAssets(const char *directory, EntityAsset *assets, int count, bool allowOverride);

// Level Loading & Saving (instance data)
bool SaveLevel(const char *filename, int **mapTiles, Entity *player, Entity *enemies, Entity *bossEnemy);
bool LoadLevel(const char *filename, int **mapTiles, Entity **player, Entity **enemies, int *enemyCount, Entity **bossEnemy, Vector2 **checkpoints, int *checkpointCount);

// Checkpoints
bool SaveCheckpointState(const char *filename, Entity player, Entity *enemies, Entity bossEnemy, Vector2 checkpoints[], int checkpointCount, int currentIndex);
bool LoadCheckpointState(const char *filename, Entity **player, Entity **enemies, Entity **bossEnemy, Vector2 checkpoints[], int *checkpointCount);

EntityAsset *GetEntityAssetById(uint64_t id);

#endif