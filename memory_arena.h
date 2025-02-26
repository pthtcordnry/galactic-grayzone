#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <stddef.h>
#include <stdbool.h>

#define GAME_ARENA_SIZE (1024 * 1024)

typedef struct BlockHeader
{
    size_t size;              // total size of this block (including header)
    struct BlockHeader *next; // next free block
} BlockHeader;

typedef struct MemoryArena
{
    size_t size;           // total size of the arena
    unsigned char *base;   // base pointer of the arena
    BlockHeader *freeList; // free list for available blocks
} MemoryArena;

// Arena for level/transient allocations.
extern MemoryArena gameArena;

// New arena for persistent asset allocations.
extern MemoryArena assetArena;

void arena_init(MemoryArena *arena, size_t size);
void arena_reset(MemoryArena *arena);
void arena_destroy(MemoryArena *arena);
void *arena_alloc(MemoryArena *arena, size_t size);
void arena_free(MemoryArena *arena, void *ptr);
void *arena_realloc(MemoryArena *arena, void *ptr, size_t new_size);

#endif // MEMORY_ARENA_H
