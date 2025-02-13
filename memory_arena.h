#ifndef MEMORY_ARENA_H
#define MEMORY_ARENA_H

#include <stddef.h>
#include <stdint.h>

// A simple memory arena structure.
typedef struct MemoryArena {
    size_t size;       // Total size of the arena
    size_t used;       // How many bytes have been allocated so far
    unsigned char *base; // Pointer to the start of the block
} MemoryArena;

// Initialize the arena with a given size.
void arena_init(MemoryArena *arena, size_t size);

// Reset the arena (all allocations are invalidated).
void arena_reset(MemoryArena *arena);

// Free the arena’s backing memory.
void arena_destroy(MemoryArena *arena);

// Allocate memory from the arena. (Here we align allocations to 8 bytes.)
void *arena_alloc(MemoryArena *arena, size_t size);

// Free the last allocated block by moving the used pointer back.
// Note: This only works if the block to free is the most recent allocation.
void arena_free_last(MemoryArena *arena, size_t size);


#endif // MEMORY_ARENA_H
