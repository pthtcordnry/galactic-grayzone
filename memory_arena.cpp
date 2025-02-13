#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// A simple memory arena structure.
typedef struct MemoryArena {
    size_t size;       // Total size of the arena
    size_t used;       // How many bytes have been allocated so far
    unsigned char *base; // Pointer to the start of the block
} MemoryArena;

// Initialize the arena with a given size.
void arena_init(MemoryArena *arena, size_t size) {
    arena->size = size;
    arena->used = 0;
    arena->base = (unsigned char *)malloc(size);
    if (!arena->base) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
}

// Reset the arena (all allocations are invalidated).
void arena_reset(MemoryArena *arena) {
    arena->used = 0;
}

// Free the arena’s backing memory.
void arena_destroy(MemoryArena *arena) {
    free(arena->base);
    arena->base = NULL;
    arena->size = 0;
    arena->used = 0;
}

// Allocate memory from the arena. (Here we align allocations to 8 bytes.)
void *arena_alloc(MemoryArena *arena, size_t size) {
    // Align size to 8 bytes for example.
    size = (size + 7) & ~7;
    if (arena->used + size > arena->size) {
        fprintf(stderr, "Arena out of memory\n");
        return NULL;
    }
    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

// Free the last allocated block by moving the used pointer back.
// Note: This only works if the block to free is the most recent allocation.
void arena_free_last(MemoryArena *arena, size_t size) {
    size = (size + 7) & ~7; // match alignment used in arena_alloc
    assert(arena->used >= size); // sanity check
    arena->used -= size;
}
