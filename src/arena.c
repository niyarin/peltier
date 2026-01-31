#include "arena.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Memory block in the arena
typedef struct arena_block {
    struct arena_block *next;
    size_t size;
    size_t used;
    char data[];  // Flexible array member
} arena_block_t;

// Arena structure
struct arena {
    arena_block_t *current;
    arena_block_t *first;
    size_t block_size;
};

// Align size to 8-byte boundary
static size_t align_size(size_t size) {
    return (size + 7) & ~7;
}

// Create a new arena block
static arena_block_t* arena_block_create(size_t size) {
    arena_block_t *block = malloc(sizeof(arena_block_t) + size);
    if (!block) {
        return NULL;
    }

    block->next = NULL;
    block->size = size;
    block->used = 0;

    return block;
}

arena_t* arena_create(size_t initial_size) {
    if (initial_size == 0) {
        initial_size = 1024 * 1024;  // 1MB default
    }

    arena_t *arena = malloc(sizeof(arena_t));
    if (!arena) {
        return NULL;
    }

    arena->block_size = initial_size;
    arena->first = arena_block_create(initial_size);
    if (!arena->first) {
        free(arena);
        return NULL;
    }

    arena->current = arena->first;

    return arena;
}

void* arena_alloc(arena_t *arena, size_t size) {
    assert(arena != NULL);

    if (size == 0) {
        return NULL;
    }

    size = align_size(size);

    // Check if current block has enough space
    arena_block_t *block = arena->current;
    if (block->used + size > block->size) {
        // Need a new block
        size_t new_block_size = arena->block_size;
        if (size > new_block_size) {
            new_block_size = size;  // Allocate larger block if needed
        }

        arena_block_t *new_block = arena_block_create(new_block_size);
        if (!new_block) {
            return NULL;
        }

        block->next = new_block;
        arena->current = new_block;
        block = new_block;
    }

    // Allocate from current block
    void *ptr = &block->data[block->used];
    block->used += size;

    return ptr;
}

char* arena_strdup(arena_t *arena, const char *str) {
    if (!str) {
        return NULL;
    }

    size_t len = strlen(str) + 1;
    char *copy = arena_alloc(arena, len);
    if (copy) {
        memcpy(copy, str, len);
    }

    return copy;
}

void* arena_memdup(arena_t *arena, const void *data, size_t size) {
    if (!data || size == 0) {
        return NULL;
    }

    void *copy = arena_alloc(arena, size);
    if (copy) {
        memcpy(copy, data, size);
    }

    return copy;
}

void arena_destroy(arena_t *arena) {
    if (!arena) {
        return;
    }

    arena_block_t *block = arena->first;
    while (block) {
        arena_block_t *next = block->next;
        free(block);
        block = next;
    }

    free(arena);
}
