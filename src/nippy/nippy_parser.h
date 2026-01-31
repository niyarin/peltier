#ifndef PELTIER_NIPPY_PARSER_H
#define PELTIER_NIPPY_PARSER_H

#include "../../include/types.h"
#include "../../include/buffer.h"
#include "../../include/arena.h"
#include <stdio.h>

// Forward declaration
typedef struct nippy_parser nippy_parser_t;

// Create parser with input stream
nippy_parser_t* nippy_parser_create(FILE *input, arena_t *arena);

// Get next event (returns pointer to internal event, do not free)
const parse_event_t* nippy_parser_next_event(nippy_parser_t *parser);

// Get error message if any
const char* nippy_parser_error(nippy_parser_t *parser);

// Get current parse position for error reporting
size_t nippy_parser_position(nippy_parser_t *parser);

// Cleanup
void nippy_parser_destroy(nippy_parser_t *parser);

#endif // PELTIER_NIPPY_PARSER_H
