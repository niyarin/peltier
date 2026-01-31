#ifndef PELTIER_EDN_WRITER_H
#define PELTIER_EDN_WRITER_H

#include "../../include/types.h"
#include <stdio.h>
#include <stdbool.h>

// Forward declaration
typedef struct edn_writer edn_writer_t;

// Create EDN writer
edn_writer_t* edn_writer_create(FILE *output, bool pretty_print, int indent_size);

// Write event to EDN output
void edn_writer_write_event(edn_writer_t *writer, const parse_event_t *event);

// Flush output buffer
void edn_writer_flush(edn_writer_t *writer);

// Cleanup
void edn_writer_destroy(edn_writer_t *writer);

#endif // PELTIER_EDN_WRITER_H
