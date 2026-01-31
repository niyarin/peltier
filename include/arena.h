#ifndef PELTIER_ARENA_H
#define PELTIER_ARENA_H

#include <stddef.h>

// Forward declaration
typedef struct arena arena_t;

// Create a new arena with initial size
arena_t* arena_create(size_t initial_size);

// Allocate memory from arena (no free needed)
void* arena_alloc(arena_t *arena, size_t size);

// Allocate and copy string
char* arena_strdup(arena_t *arena, const char *str);

// Allocate and copy N bytes
void* arena_memdup(arena_t *arena, const void *data, size_t size);

// Free entire arena at once
void arena_destroy(arena_t *arena);

#endif // PELTIER_ARENA_H
