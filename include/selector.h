#ifndef PELTIER_SELECTOR_H
#define PELTIER_SELECTOR_H

#include "types.h"
#include "arena.h"
#include <stdbool.h>

// Forward declarations
typedef struct selector selector_t;
typedef struct path path_t;

// Path structure for tracking current position in data
struct path {
    char *segments[MAX_NESTING_DEPTH];
    int indices[MAX_NESTING_DEPTH];
    size_t depth;
};

// Parse selector expression
selector_t* selector_parse(const char *expr, arena_t *arena);

// Check if current path matches selector
bool selector_matches(selector_t *sel, const path_t *current_path);

// Path operations
void path_init(path_t *path);
void path_push_key(path_t *path, const char *key);
void path_push_index(path_t *path, int index);
void path_pop(path_t *path);
char* path_to_string(const path_t *path, arena_t *arena);

#endif // PELTIER_SELECTOR_H
