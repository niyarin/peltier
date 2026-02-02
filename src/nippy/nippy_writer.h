#ifndef PELTIER_NIPPY_WRITER_H
#define PELTIER_NIPPY_WRITER_H

#include "../../include/types.h"
#include <stdio.h>

// Forward declaration
typedef struct nippy_writer nippy_writer_t;

// Create writer with output stream
nippy_writer_t* nippy_writer_create(FILE *output);

// Write event to Nippy format
bool nippy_writer_write_event(nippy_writer_t *writer, const parse_event_t *event);

// Get error message if any
const char* nippy_writer_error(nippy_writer_t *writer);

// Cleanup
void nippy_writer_destroy(nippy_writer_t *writer);

#endif // PELTIER_NIPPY_WRITER_H
