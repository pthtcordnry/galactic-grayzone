#include "memory_arena.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define HEADER_SIZE (sizeof(size_t))

// Align to 8 bytes.
static size_t align8(size_t size) {
    return (size + 7) & ~7;
}

void arena_init(MemoryArena *arena, size_t size) {
    arena->size = size;
    arena->base = (unsigned char *)malloc(size);
    if (!arena->base) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    // Initialize one free block for the entire arena.
    arena->freeList = (BlockHeader *)arena->base;
    arena->freeList->size = size;
    arena->freeList->next = NULL;
}

void arena_reset(MemoryArena *arena) {
    arena->freeList = (BlockHeader *)arena->base;
    arena->freeList->size = arena->size;
    arena->freeList->next = NULL;
}

void arena_destroy(MemoryArena *arena) {
    free(arena->base);
    arena->base = NULL;
    arena->freeList = NULL;
    arena->size = 0;
}

void *arena_alloc(MemoryArena *arena, size_t size) {
    // Reserve space for the header as well.
    size_t totalSize = align8(size + HEADER_SIZE);
    BlockHeader *prev = NULL;
    BlockHeader *curr = arena->freeList;
    
    // First-fit search for a block that's large enough.
    while (curr) {
        if (curr->size >= totalSize) {
            // If the block is larger than needed by a threshold, split it.
            if (curr->size - totalSize >= sizeof(BlockHeader) + 8) {
                BlockHeader *newBlock = (BlockHeader *)((unsigned char *)curr + totalSize);
                newBlock->size = curr->size - totalSize;
                newBlock->next = curr->next;
                if (prev) {
                    prev->next = newBlock;
                } else {
                    arena->freeList = newBlock;
                }
                curr->size = totalSize;
            } else {
                // Use the whole block.
                if (prev) {
                    prev->next = curr->next;
                } else {
                    arena->freeList = curr->next;
                }
            }
            // Store the size in the header.
            *((size_t *)curr) = curr->size;
            return (void *)((unsigned char *)curr + HEADER_SIZE);
        }
        prev = curr;
        curr = curr->next;
    }
    fprintf(stderr, "Arena out of memory in alloc\n");
    return NULL;
}

void arena_free(MemoryArena *arena, void *ptr) {
    if (!ptr) return;
    BlockHeader *block = (BlockHeader *)((unsigned char *)ptr - HEADER_SIZE);

    // Insert the block into the free list in sorted order by address.
    BlockHeader *prev = NULL;
    BlockHeader *curr = arena->freeList;
    while (curr && curr < block) {
        prev = curr;
        curr = curr->next;
    }
    block->next = curr;
    if (prev) {
        prev->next = block;
    } else {
        arena->freeList = block;
    }
    
    // Coalesce with next block if adjacent.
    if (block->next && ((unsigned char *)block + block->size == (unsigned char *)block->next)) {
        block->size += block->next->size;
        block->next = block->next->next;
    }
    // Coalesce with previous block if adjacent.
    if (prev && ((unsigned char *)prev + prev->size == (unsigned char *)block)) {
        prev->size += block->size;
        prev->next = block->next;
    }
}

void *arena_realloc(MemoryArena *arena, void *ptr, size_t new_size) {
    if (ptr == NULL) {
        return arena_alloc(arena, new_size);
    }
    if (new_size == 0) {
        arena_free(arena, ptr);
        return NULL;
    }
    // Get pointer to the header.
    BlockHeader *oldHeader = (BlockHeader *)((unsigned char *)ptr - HEADER_SIZE);
    size_t oldTotalSize = oldHeader->size;
    size_t oldUserSize = oldTotalSize - HEADER_SIZE;
    size_t newTotalSize = align8(new_size + HEADER_SIZE);

    // For simplicity, we'll always allocate a new block, copy the data, then free the old block.
    void *new_ptr = arena_alloc(arena, new_size);
    if (new_ptr == NULL) {
        return NULL; // allocation failed
    }
    size_t copySize = (oldUserSize < new_size) ? oldUserSize : new_size;
    memcpy(new_ptr, ptr, copySize);
    arena_free(arena, ptr);
    return new_ptr;
}
