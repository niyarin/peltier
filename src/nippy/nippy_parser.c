#include "nippy_parser.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Nippy type tags (based on nippy format)
// NOTE: nil doesn't have an explicit type tag in some versions
#define NIPPY_TYPE_TRUE          0x01
#define NIPPY_TYPE_FALSE         0x02
#define NIPPY_TYPE_BYTE          0x28
#define NIPPY_TYPE_SHORT         0x29
#define NIPPY_TYPE_INT           0x2A
#define NIPPY_TYPE_LONG          0x2B
#define NIPPY_TYPE_FLOAT         0x2D
#define NIPPY_TYPE_DOUBLE        0x2E

// String types
#define NIPPY_TYPE_STRING_0      0x50
#define NIPPY_TYPE_STRING_SM     0x51  // Small string (1-byte length)
#define NIPPY_TYPE_STRING_MD     0x52  // Medium string (2-byte length)
#define NIPPY_TYPE_STRING_LG     0x53  // Large string (4-byte length)

// Keyword types
#define NIPPY_TYPE_KEYWORD_0     0x54
#define NIPPY_TYPE_KEYWORD_SM    0x55
#define NIPPY_TYPE_KEYWORD_MD    0x56
#define NIPPY_TYPE_KEYWORD_LG    0x57

// Collection types
#define NIPPY_TYPE_VECTOR_0      0x60
#define NIPPY_TYPE_VECTOR_SM     0x61
#define NIPPY_TYPE_VECTOR_MD     0x62
#define NIPPY_TYPE_VECTOR_LG     0x63

#define NIPPY_TYPE_SET_0         0x64
#define NIPPY_TYPE_SET_SM        0x65
#define NIPPY_TYPE_SET_MD        0x66
#define NIPPY_TYPE_SET_LG        0x67

#define NIPPY_TYPE_MAP_0         0x68
#define NIPPY_TYPE_MAP_SM        0x69
#define NIPPY_TYPE_MAP_MD        0x6A
#define NIPPY_TYPE_MAP_LG        0x6B

#define NIPPY_TYPE_LIST_0        0x6C
#define NIPPY_TYPE_LIST_SM       0x6D

// v3.3.0+ unsigned variants
#define NIPPY_TYPE_VECTOR_SM_U   0x6E  // vec-sm_ (unsigned)
#define NIPPY_TYPE_SET_SM_U      0x6F  // set-sm_ (unsigned)
#define NIPPY_TYPE_MAP_SM_U      0x70  // map-sm_ (unsigned)

// Arrays (Java native arrays, treated as vectors in EDN)
#define NIPPY_TYPE_OBJECT_ARRAY_LG 0x73  // object-array-lg

// Additional types
#define NIPPY_TYPE_LONG_0       0x00  // long 0
// Note: 0x01-0x03 are used for caching and conflict with other values
#define NIPPY_TYPE_CHAR         0x0A  // char (2 bytes)
#define NIPPY_TYPE_CACHED_0     0x3B  // Cached value definition 0 (59)
#define NIPPY_TYPE_FLOAT        0x3C  // float (4 bytes) (60)
#define NIPPY_TYPE_CACHED_1     0x3F  // Cached value definition 1 (63)
#define NIPPY_TYPE_CACHED_2     0x40  // Cached value definition 2 (64)
#define NIPPY_TYPE_CACHED_3     0x41  // Cached value definition 3 (65)
#define NIPPY_TYPE_UTIL_DATE    0x5A  // java.util.Date (8 bytes) (90)
#define NIPPY_TYPE_UUID         0x5B  // UUID (16 bytes) (91)

// Parser structure with PDA stack
struct nippy_parser {
    buffer_t *buffer;
    arena_t *arena;

    // PDA stack
    parse_context_t stack[MAX_NESTING_DEPTH];
    size_t stack_depth;

    // State machine
    parser_state_t state;
    uint8_t current_tag;

    // Current event being prepared
    parse_event_t current_event;

    // Error message
    char error_buffer[256];
};

// Stack operations
static void push_context(nippy_parser_t *p, parse_context_type_t type, size_t count) {
    assert(p->stack_depth < MAX_NESTING_DEPTH);

    p->stack[p->stack_depth].type = type;
    p->stack[p->stack_depth].remaining_elements = count;
    p->stack[p->stack_depth].index = 0;
    p->stack_depth++;
}

static void pop_context(nippy_parser_t *p) {
    assert(p->stack_depth > 0);
    p->stack_depth--;
}

static parse_context_t* current_context(nippy_parser_t *p) {
    if (p->stack_depth == 0) {
        return NULL;
    }
    return &p->stack[p->stack_depth - 1];
}

// Type checking helpers
static bool is_string_type(uint8_t tag) {
    return tag >= NIPPY_TYPE_STRING_0 && tag <= NIPPY_TYPE_STRING_LG;
}

static bool is_keyword_type(uint8_t tag) {
    return tag >= NIPPY_TYPE_KEYWORD_0 && tag <= NIPPY_TYPE_KEYWORD_LG;
}

static bool is_vector_type(uint8_t tag) {
    return (tag >= NIPPY_TYPE_VECTOR_0 && tag <= NIPPY_TYPE_VECTOR_LG) ||
           tag == NIPPY_TYPE_VECTOR_SM_U ||
           tag == NIPPY_TYPE_OBJECT_ARRAY_LG;
}

static bool is_set_type(uint8_t tag) {
    return (tag >= NIPPY_TYPE_SET_0 && tag <= NIPPY_TYPE_SET_LG) ||
           tag == NIPPY_TYPE_SET_SM_U;
}

static bool is_map_type(uint8_t tag) {
    return (tag >= NIPPY_TYPE_MAP_0 && tag <= NIPPY_TYPE_MAP_LG) ||
           tag == NIPPY_TYPE_MAP_SM_U;
}

static bool is_list_type(uint8_t tag) {
    return tag >= NIPPY_TYPE_LIST_0 && tag <= NIPPY_TYPE_LIST_SM;
}

// Read length prefix based on type tag
static size_t read_length_prefix(nippy_parser_t *p, uint8_t tag) {
    // v3.3.0+ unsigned variants (1-byte unsigned)
    if (tag == NIPPY_TYPE_VECTOR_SM_U ||
        tag == NIPPY_TYPE_SET_SM_U ||
        tag == NIPPY_TYPE_MAP_SM_U) {
        int b = buffer_read_byte(p->buffer);
        return (b >= 0) ? (size_t)((uint8_t)b) : 0;
    }

    // Arrays (4-byte length)
    if (tag == NIPPY_TYPE_OBJECT_ARRAY_LG) {
        int32_t len = buffer_read_int32_be(p->buffer);
        return (len >= 0) ? (size_t)len : 0;
    }

    // _0 suffix means empty (length 0)
    if ((tag & 0x03) == 0x00) {
        return 0;
    }
    // _SM suffix means 1-byte length (signed)
    else if ((tag & 0x03) == 0x01) {
        int b = buffer_read_byte(p->buffer);
        return (b >= 0) ? (size_t)b : 0;
    }
    // _MD suffix means 2-byte length
    else if ((tag & 0x03) == 0x02) {
        int16_t len = buffer_read_int16_be(p->buffer);
        return (len >= 0) ? (size_t)len : 0;
    }
    // _LG suffix means 4-byte length
    else {
        int32_t len = buffer_read_int32_be(p->buffer);
        return (len >= 0) ? (size_t)len : 0;
    }
}

// Read string data
static char* read_string_data(nippy_parser_t *p, size_t len) {
    if (len == 0) {
        return arena_strdup(p->arena, "");
    }

    char *str = arena_alloc(p->arena, len + 1);
    if (!str) {
        return NULL;
    }

    size_t read = buffer_read_bytes(p->buffer, str, len);
    if (read != len) {
        snprintf(p->error_buffer, sizeof(p->error_buffer),
                 "Unexpected EOF while reading string (expected %zu bytes, got %zu)",
                 len, read);
        return NULL;
    }

    str[len] = '\0';
    return str;
}

// Parse primitive value
static bool parse_primitive(nippy_parser_t *p, uint8_t tag) {
    parse_event_t *ev = &p->current_event;

    switch (tag) {
        case NIPPY_TYPE_TRUE:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_BOOL;
            ev->value.bool_val = true;
            return true;

        case NIPPY_TYPE_FALSE:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_BOOL;
            ev->value.bool_val = false;
            return true;

        case NIPPY_TYPE_LONG_0:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = 0;
            return true;

        case NIPPY_TYPE_CACHED_0:
        case NIPPY_TYPE_CACHED_1:
        case NIPPY_TYPE_CACHED_2:
        case NIPPY_TYPE_CACHED_3:
            // Cached value definitions - not yet supported
            // For now, output nil
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_NIL;
            return true;

        case NIPPY_TYPE_CHAR: {
            int16_t ch = buffer_read_int16_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_CHAR;
            ev->value.int_val = ch;
            return true;
        }

        case NIPPY_TYPE_BYTE: {
            int byte = buffer_read_byte(p->buffer);
            if (byte < 0) {
                snprintf(p->error_buffer, sizeof(p->error_buffer),
                         "Unexpected EOF while reading byte");
                return false;
            }
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = (int8_t)byte;
            return true;
        }

        case NIPPY_TYPE_SHORT:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = buffer_read_int16_be(p->buffer);
            return true;

        case NIPPY_TYPE_INT:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = buffer_read_int32_be(p->buffer);
            return true;

        case NIPPY_TYPE_LONG:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = buffer_read_int64_be(p->buffer);
            return true;

        case NIPPY_TYPE_FLOAT: {
            int32_t bits = buffer_read_int32_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_DOUBLE;
            float f;
            memcpy(&f, &bits, sizeof(float));
            ev->value.float_val = (double)f;
            return true;
        }

        case NIPPY_TYPE_DOUBLE: {
            int64_t bits = buffer_read_int64_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_DOUBLE;
            memcpy(&ev->value.float_val, &bits, sizeof(double));
            return true;
        }

        case NIPPY_TYPE_UTIL_DATE: {
            // Read 8-byte timestamp (milliseconds since epoch)
            int64_t millis = buffer_read_int64_be(p->buffer);
            // For now, output as integer (TODO: format as #inst)
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = millis;
            return true;
        }

        case NIPPY_TYPE_UUID: {
            // Read 16-byte UUID
            uint8_t uuid_bytes[16];
            if (buffer_read_bytes(p->buffer, uuid_bytes, 16) != 16) {
                snprintf(p->error_buffer, sizeof(p->error_buffer),
                         "Unexpected EOF while reading UUID");
                return false;
            }
            // Format as string: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
            char *uuid_str = arena_alloc(p->arena, 37);
            if (!uuid_str) {
                return false;
            }
            snprintf(uuid_str, 37,
                     "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     uuid_bytes[0], uuid_bytes[1], uuid_bytes[2], uuid_bytes[3],
                     uuid_bytes[4], uuid_bytes[5], uuid_bytes[6], uuid_bytes[7],
                     uuid_bytes[8], uuid_bytes[9], uuid_bytes[10], uuid_bytes[11],
                     uuid_bytes[12], uuid_bytes[13], uuid_bytes[14], uuid_bytes[15]);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_STRING;
            ev->value.string_val = uuid_str;
            return true;
        }

        default:
            if (is_string_type(tag)) {
                size_t len = read_length_prefix(p, tag);
                char *str = read_string_data(p, len);
                if (!str && len > 0) {
                    return false;
                }
                ev->type = EVENT_VALUE;
                ev->value_type = VALUE_STRING;
                ev->value.string_val = str;
                return true;
            }
            else if (is_keyword_type(tag)) {
                size_t len = read_length_prefix(p, tag);
                char *str = read_string_data(p, len);
                if (!str && len > 0) {
                    return false;
                }
                ev->type = EVENT_VALUE;
                ev->value_type = VALUE_KEYWORD;
                ev->value.string_val = str;
                return true;
            }

            snprintf(p->error_buffer, sizeof(p->error_buffer),
                     "Unknown type tag: 0x%02X", tag);
            return false;
    }
}

// Update context after processing an element
static void update_context(nippy_parser_t *p) {
    parse_context_t *ctx = current_context(p);
    if (!ctx) {
        return;
    }

    // For maps, alternate between KEY and VALUE
    if (ctx->type == CONTEXT_MAP_KEY) {
        ctx->type = CONTEXT_MAP_VALUE;
    } else if (ctx->type == CONTEXT_MAP_VALUE) {
        ctx->type = CONTEXT_MAP_KEY;
        if (ctx->remaining_elements > 0) {
            ctx->remaining_elements--;
        }
    } else {
        // For vectors, sets, lists
        if (ctx->remaining_elements > 0) {
            ctx->remaining_elements--;
        }
        ctx->index++;
    }
}

nippy_parser_t* nippy_parser_create(FILE *input, arena_t *arena) {
    assert(input != NULL);
    assert(arena != NULL);

    nippy_parser_t *p = malloc(sizeof(nippy_parser_t));
    if (!p) {
        return NULL;
    }

    p->buffer = buffer_create(input, 8192);
    if (!p->buffer) {
        free(p);
        return NULL;
    }

    p->arena = arena;
    p->stack_depth = 0;
    p->state = STATE_READ_TYPE_TAG;
    p->current_tag = 0;
    p->error_buffer[0] = '\0';

    memset(&p->current_event, 0, sizeof(parse_event_t));

    return p;
}

const parse_event_t* nippy_parser_next_event(nippy_parser_t *p) {
    assert(p != NULL);

    while (1) {
        // First, check if we need to emit END events for completed collections
        parse_context_t *ctx = current_context(p);
        if (ctx && ctx->remaining_elements == 0) {
            // Emit END event
            parse_context_type_t ctx_type = ctx->type;
            pop_context(p);

            if (ctx_type == CONTEXT_VECTOR) {
                p->current_event.type = EVENT_END_VECTOR;
            } else if (ctx_type == CONTEXT_SET) {
                p->current_event.type = EVENT_END_SET;
            } else if (ctx_type == CONTEXT_MAP_KEY || ctx_type == CONTEXT_MAP_VALUE) {
                p->current_event.type = EVENT_END_MAP;
            } else if (ctx_type == CONTEXT_LIST) {
                p->current_event.type = EVENT_END_LIST;
            }

            return &p->current_event;
        }

        switch (p->state) {
            case STATE_READ_TYPE_TAG: {
                int tag_int = buffer_read_byte(p->buffer);
                if (tag_int < 0) {
                    // Reached EOF - close all open collections
                    if (p->stack_depth == 0) {
                        p->current_event.type = EVENT_EOF;
                        p->state = STATE_EOF;
                        return &p->current_event;
                    }

                    // Force all collections to close by setting remaining to 0
                    for (size_t i = 0; i < p->stack_depth; i++) {
                        p->stack[i].remaining_elements = 0;
                    }

                    // Continue to top of loop to emit END events
                    continue;
                }

                uint8_t tag = (uint8_t)tag_int;
                p->current_tag = tag;

                // Check if it's a collection type
                if (is_vector_type(tag) || is_set_type(tag) ||
                    is_map_type(tag) || is_list_type(tag)) {
                    size_t count = read_length_prefix(p, tag);

                    // Determine collection type and emit START event
                    if (is_vector_type(tag)) {
                        p->current_event.type = EVENT_START_VECTOR;
                        push_context(p, CONTEXT_VECTOR, count);
                    } else if (is_set_type(tag)) {
                        p->current_event.type = EVENT_START_SET;
                        push_context(p, CONTEXT_SET, count);
                    } else if (is_map_type(tag)) {
                        p->current_event.type = EVENT_START_MAP;
                        push_context(p, CONTEXT_MAP_KEY, count);
                    } else if (is_list_type(tag)) {
                        p->current_event.type = EVENT_START_LIST;
                        push_context(p, CONTEXT_LIST, count);
                    }

                    p->current_event.collection_size = count;

                    return &p->current_event;
                } else {
                    // Primitive type - parse it
                    if (!parse_primitive(p, tag)) {
                        p->state = STATE_ERROR;
                        p->current_event.type = EVENT_ERROR;
                        p->current_event.error_message = p->error_buffer;
                        return &p->current_event;
                    }

                    // Update context
                    update_context(p);

                    return &p->current_event;
                }
            }

            case STATE_ERROR:
                p->current_event.type = EVENT_ERROR;
                p->current_event.error_message = p->error_buffer;
                return &p->current_event;

            case STATE_EOF:
                p->current_event.type = EVENT_EOF;
                return &p->current_event;

            default:
                snprintf(p->error_buffer, sizeof(p->error_buffer),
                         "Invalid parser state: %d", p->state);
                p->state = STATE_ERROR;
                break;
        }
    }
}

const char* nippy_parser_error(nippy_parser_t *p) {
    assert(p != NULL);
    return p->error_buffer[0] ? p->error_buffer : NULL;
}

size_t nippy_parser_position(nippy_parser_t *p) {
    assert(p != NULL);
    return buffer_position(p->buffer);
}

void nippy_parser_destroy(nippy_parser_t *p) {
    if (!p) {
        return;
    }

    buffer_destroy(p->buffer);
    free(p);
}
