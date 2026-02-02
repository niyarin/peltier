#ifndef PELTIER_EDN_PARSER_H
#define PELTIER_EDN_PARSER_H

#include "../../include/types.h"
#include "../../include/arena.h"
#include <stdio.h>

// Forward declaration
typedef struct edn_parser edn_parser_t;

// Parser options
typedef struct {
    // Threshold for collection size buffering strategy:
    // - Collections < threshold: use memory buffer (fast, 1-pass)
    // - Collections >= threshold: use fseek (memory-efficient, 2-pass)
    size_t buffer_threshold;  // Default: 1000
} edn_parser_options_t;

// Create parser with input stream (uses default options)
edn_parser_t* edn_parser_create(FILE *input, arena_t *arena);

// Create parser with custom options
edn_parser_t* edn_parser_create_with_options(FILE *input, arena_t *arena, const edn_parser_options_t *options);

// Get next event (returns pointer to internal event, do not free)
const parse_event_t* edn_parser_next_event(edn_parser_t *parser);

// Get error message if any
const char* edn_parser_error(edn_parser_t *parser);

// Cleanup
void edn_parser_destroy(edn_parser_t *parser);

#endif // PELTIER_EDN_PARSER_H
