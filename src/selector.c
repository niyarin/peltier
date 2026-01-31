#include "selector.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Placeholder selector structure
struct selector {
    char *expression;
    // TODO: Parse tree, filter functions, etc.
};

selector_t* selector_parse(const char *expr, arena_t *arena) {
    (void)expr;
    (void)arena;

    // TODO: Implement selector parsing
    return NULL;
}

bool selector_matches(selector_t *sel, const path_t *current_path) {
    (void)sel;
    (void)current_path;

    // TODO: Implement path matching
    return true;  // Match everything for now
}

void path_init(path_t *path) {
    assert(path != NULL);

    memset(path, 0, sizeof(path_t));
}

void path_push_key(path_t *path, const char *key) {
    assert(path != NULL);
    assert(key != NULL);

    if (path->depth < MAX_NESTING_DEPTH) {
        path->segments[path->depth] = (char *)key;  // NOTE: Not copying, just reference
        path->indices[path->depth] = -1;
        path->depth++;
    }
}

void path_push_index(path_t *path, int index) {
    assert(path != NULL);

    if (path->depth < MAX_NESTING_DEPTH) {
        path->segments[path->depth] = NULL;
        path->indices[path->depth] = index;
        path->depth++;
    }
}

void path_pop(path_t *path) {
    assert(path != NULL);

    if (path->depth > 0) {
        path->depth--;
    }
}

char* path_to_string(const path_t *path, arena_t *arena) {
    (void)path;
    (void)arena;

    // TODO: Implement path to string conversion
    return NULL;
}
