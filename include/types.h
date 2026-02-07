#ifndef PELTIER_TYPES_H
#define PELTIER_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Maximum nesting depth for collections
#define MAX_NESTING_DEPTH 256

// Parse event types (generic, used for both Nippy and EDN parsing)
typedef enum {
    EVENT_START_MAP,
    EVENT_END_MAP,
    EVENT_START_VECTOR,
    EVENT_END_VECTOR,
    EVENT_START_SET,
    EVENT_END_SET,
    EVENT_START_LIST,
    EVENT_END_LIST,
    EVENT_KEY,          // Map key
    EVENT_VALUE,        // Atomic value
    EVENT_ERROR,
    EVENT_EOF
} event_type_t;

// Value types for atomic values
typedef enum {
    VALUE_NIL,
    VALUE_BOOL,
    VALUE_INT64,
    VALUE_DOUBLE,
    VALUE_STRING,
    VALUE_KEYWORD,
    VALUE_SYMBOL,
    VALUE_CHAR,
    VALUE_UUID
} value_type_t;

// Value union for different data types
typedef union {
    bool bool_val;
    int64_t int_val;
    double float_val;
    char *string_val;
} value_data_t;

// Parse context type for PDA stack
typedef enum {
    CONTEXT_TOP_LEVEL,
    CONTEXT_VECTOR,      // Inside a vector
    CONTEXT_MAP_KEY,     // Inside map, expecting key
    CONTEXT_MAP_VALUE,   // Inside map, expecting value
    CONTEXT_SET,         // Inside a set
    CONTEXT_LIST         // Inside a list
} parse_context_type_t;

// Parse context for stack
typedef struct {
    parse_context_type_t type;
    size_t remaining_elements;  // Elements left to parse
    size_t index;               // Current index (for vectors)
} parse_context_t;

// Parser state machine states
typedef enum {
    STATE_INITIAL,           // Start of parsing
    STATE_READ_TYPE_TAG,     // Reading next type tag byte
    STATE_READ_LENGTH,       // Reading length prefix
    STATE_READ_VALUE,        // Reading value bytes
    STATE_EMIT_EVENT,        // Emit event to caller
    STATE_ERROR,             // Parse error
    STATE_EOF                // End of input
} parser_state_t;

// Parse event structure (generic, used for both Nippy and EDN parsing)
typedef struct {
    event_type_t type;
    value_type_t value_type;
    value_data_t value;
    size_t collection_size;  // For START_* events
    const char *error_message;
} parse_event_t;

#endif // PELTIER_TYPES_H
