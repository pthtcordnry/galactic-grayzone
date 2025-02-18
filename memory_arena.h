#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <stddef.h>
#include <stdbool.h>

typedef struct BlockHeader {
    size_t size;             // Total size of this block (including header)
    struct BlockHeader *next;// Next free block in the free list (if free)
} BlockHeader;

typedef struct MemoryArena {
    size_t size;             // Total size of the arena
    unsigned char *base;     // Base pointer of the arena
    BlockHeader *freeList;   // Free list of available blocks
} MemoryArena;

void arena_init(MemoryArena *arena, size_t size);
void arena_reset(MemoryArena *arena);
void arena_destroy(MemoryArena *arena);
void *arena_alloc(MemoryArena *arena, size_t size);
void arena_free(MemoryArena *arena, void *ptr);
void *arena_realloc(MemoryArena *arena, void *ptr, size_t new_size);

#endif // MEMORY_ARENA_H
