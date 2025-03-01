#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <stddef.h>
#include <stdbool.h>

#define GAME_ARENA_SIZE (1024 * 1024)

typedef struct BlockHeader
{
    size_t size;
    struct BlockHeader *next;
} BlockHeader;

typedef struct MemoryArena
{
    size_t size;
    unsigned char *base;
    BlockHeader *freeList;
} MemoryArena;

extern MemoryArena gameArena;
extern MemoryArena assetArena;

void arena_init(MemoryArena *arena, size_t size);
void arena_reset(MemoryArena *arena);
void arena_destroy(MemoryArena *arena);
void *arena_alloc(MemoryArena *arena, size_t size);
void arena_free(MemoryArena *arena, void *ptr);
void *arena_realloc(MemoryArena *arena, void *ptr, size_t new_size);

#endif
