#include "nippy_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

// Nippy type tags (same as parser)
#define NIPPY_TYPE_NIL           0x03
#define NIPPY_TYPE_TRUE          0x08
#define NIPPY_TYPE_FALSE         0x09
#define NIPPY_TYPE_BYTE          0x28
#define NIPPY_TYPE_SHORT         0x29
#define NIPPY_TYPE_INT           0x2A
#define NIPPY_TYPE_LONG          0x2B
#define NIPPY_TYPE_FLOAT         0x3C
#define NIPPY_TYPE_DOUBLE        0x3D

// Optimized long types (using v3.3.0+ signed types)
#define NIPPY_TYPE_LONG_0        0x00
#define NIPPY_TYPE_LONG_SM       0x64  // long-sm_ (v3.3.0+ signed 1-byte)
#define NIPPY_TYPE_LONG_MD       0x65  // long-md_ (v3.3.0+ signed 2-byte)
#define NIPPY_TYPE_LONG_LG       0x66  // long-lg_ (v3.3.0+ signed 4-byte)

// String types
#define NIPPY_TYPE_STRING_0      0x22
#define NIPPY_TYPE_STRING_SM     0x69  // str-sm_ (v3.3.0+ signed length)
#define NIPPY_TYPE_STRING_MD     0x10
#define NIPPY_TYPE_STRING_LG     0x0D

// Keyword types
#define NIPPY_TYPE_KEYWORD_SM    0x6A
#define NIPPY_TYPE_KEYWORD_MD    0x55
#define NIPPY_TYPE_KEYWORD_LG    0x4D

// Symbol types
#define NIPPY_TYPE_SYMBOL_SM     0x38
#define NIPPY_TYPE_SYMBOL_MD     0x56

// Collection types (using v3.3.0+ signed count types for SM)
#define NIPPY_TYPE_VECTOR_0      0x11
#define NIPPY_TYPE_VECTOR_SM     0x6E  // vec-sm_ (v3.3.0+ signed count)
#define NIPPY_TYPE_VECTOR_MD     0x45
#define NIPPY_TYPE_VECTOR_LG     0x15

#define NIPPY_TYPE_SET_0         0x12
#define NIPPY_TYPE_SET_SM        0x6F  // set-sm_ (v3.3.0+ signed count)
#define NIPPY_TYPE_SET_MD        0x20
#define NIPPY_TYPE_SET_LG        0x17

#define NIPPY_TYPE_MAP_0         0x13
#define NIPPY_TYPE_MAP_SM        0x70  // map-sm_ (v3.3.0+ signed count)
#define NIPPY_TYPE_MAP_MD        0x21
#define NIPPY_TYPE_MAP_LG        0x1E

#define NIPPY_TYPE_LIST_0        0x23
#define NIPPY_TYPE_LIST_SM       0x24
#define NIPPY_TYPE_LIST_MD       0x36
#define NIPPY_TYPE_LIST_LG       0x14

#define NIPPY_TYPE_UUID          0x5B

#define MAX_NESTING_DEPTH 256

typedef enum {
    COLLECTION_VECTOR,
    COLLECTION_MAP,
    COLLECTION_SET,
    COLLECTION_LIST
} collection_type_t;

typedef struct {
    collection_type_t type;
    size_t expected_count;  // For vectors/sets/lists: element count, for maps: key-value pairs
    size_t current_count;
    bool expecting_key;     // For maps: true if next value should be a key
} collection_context_t;

struct nippy_writer {
    FILE *output;

    // Collection stack
    collection_context_t stack[MAX_NESTING_DEPTH];
    size_t stack_depth;

    // Header written flag
    bool header_written;

    // Error message
    char error_buffer[256];
};

// Write helpers
static bool write_byte(nippy_writer_t *w, uint8_t byte) {
    if (fwrite(&byte, 1, 1, w->output) != 1) {
        snprintf(w->error_buffer, sizeof(w->error_buffer), "Failed to write byte");
        return false;
    }
    return true;
}

static bool write_bytes(nippy_writer_t *w, const void *data, size_t size) {
    if (fwrite(data, 1, size, w->output) != size) {
        snprintf(w->error_buffer, sizeof(w->error_buffer), "Failed to write bytes");
        return false;
    }
    return true;
}

static bool write_uint16_be(nippy_writer_t *w, uint16_t value) {
    uint8_t bytes[2] = {
        (value >> 8) & 0xFF,
        value & 0xFF
    };
    return write_bytes(w, bytes, 2);
}

static bool write_uint32_be(nippy_writer_t *w, uint32_t value) {
    uint8_t bytes[4] = {
        (value >> 24) & 0xFF,
        (value >> 16) & 0xFF,
        (value >> 8) & 0xFF,
        value & 0xFF
    };
    return write_bytes(w, bytes, 4);
}

static bool write_int16_be(nippy_writer_t *w, int16_t value) {
    return write_uint16_be(w, (uint16_t)value);
}

static bool write_int32_be(nippy_writer_t *w, int32_t value) {
    return write_uint32_be(w, (uint32_t)value);
}

static bool write_int64_be(nippy_writer_t *w, int64_t value) {
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (value >> (56 - i * 8)) & 0xFF;
    }
    return write_bytes(w, bytes, 8);
}

static bool write_float(nippy_writer_t *w, float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    return write_uint32_be(w, bits);
}

static bool write_double(nippy_writer_t *w, double value) {
    uint64_t bits;
    memcpy(&bits, &value, 8);
    uint8_t bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (bits >> (56 - i * 8)) & 0xFF;
    }
    return write_bytes(w, bytes, 8);
}

// Stack operations
static void push_collection(nippy_writer_t *w, collection_type_t type, size_t count) {
    assert(w->stack_depth < MAX_NESTING_DEPTH);
    w->stack[w->stack_depth].type = type;
    w->stack[w->stack_depth].expected_count = count;
    w->stack[w->stack_depth].current_count = 0;
    w->stack[w->stack_depth].expecting_key = (type == COLLECTION_MAP);
    w->stack_depth++;
}

static void pop_collection(nippy_writer_t *w) {
    assert(w->stack_depth > 0);
    w->stack_depth--;
}

static collection_context_t* current_collection(nippy_writer_t *w) {
    if (w->stack_depth == 0) {
        return NULL;
    }
    return &w->stack[w->stack_depth - 1];
}

// Write Nippy header
static bool write_header(nippy_writer_t *w) {
    if (w->header_written) {
        return true;
    }

    // Write "NPY\0" header (no compression, no encryption)
    uint8_t header[4] = {'N', 'P', 'Y', 0x00};
    if (!write_bytes(w, header, 4)) {
        return false;
    }

    w->header_written = true;
    return true;
}

// Write integer with optimal encoding (using v3.3.0+ signed types)
static bool write_integer(nippy_writer_t *w, int64_t value) {
    // Special case: zero
    if (value == 0) {
        return write_byte(w, NIPPY_TYPE_LONG_0);
    }

    // Use signed types for optimal encoding
    if (value >= -128 && value <= 127) {
        return write_byte(w, NIPPY_TYPE_LONG_SM) &&
               write_byte(w, (uint8_t)(int8_t)value);
    } else if (value >= -32768 && value <= 32767) {
        return write_byte(w, NIPPY_TYPE_LONG_MD) &&
               write_uint16_be(w, (uint16_t)(int16_t)value);
    } else if (value >= -2147483648LL && value <= 2147483647LL) {
        return write_byte(w, NIPPY_TYPE_LONG_LG) &&
               write_uint32_be(w, (uint32_t)(int32_t)value);
    } else {
        return write_byte(w, NIPPY_TYPE_LONG) &&
               write_int64_be(w, value);
    }
}

// Write string with optimal encoding
static bool write_string(nippy_writer_t *w, const char *str) {
    size_t len = strlen(str);

    if (len == 0) {
        return write_byte(w, NIPPY_TYPE_STRING_0);
    } else if (len <= 255) {
        return write_byte(w, NIPPY_TYPE_STRING_SM) &&
               write_byte(w, (uint8_t)len) &&
               write_bytes(w, str, len);
    } else if (len <= 65535) {
        return write_byte(w, NIPPY_TYPE_STRING_MD) &&
               write_uint16_be(w, (uint16_t)len) &&
               write_bytes(w, str, len);
    } else {
        return write_byte(w, NIPPY_TYPE_STRING_LG) &&
               write_uint32_be(w, (uint32_t)len) &&
               write_bytes(w, str, len);
    }
}

// Write keyword with optimal encoding
static bool write_keyword(nippy_writer_t *w, const char *kw) {
    // Remove leading ':' if present
    if (kw[0] == ':') {
        kw++;
    }

    size_t len = strlen(kw);

    if (len <= 255) {
        return write_byte(w, NIPPY_TYPE_KEYWORD_SM) &&
               write_byte(w, (uint8_t)len) &&
               write_bytes(w, kw, len);
    } else if (len <= 65535) {
        return write_byte(w, NIPPY_TYPE_KEYWORD_MD) &&
               write_uint16_be(w, (uint16_t)len) &&
               write_bytes(w, kw, len);
    } else {
        return write_byte(w, NIPPY_TYPE_KEYWORD_LG) &&
               write_uint32_be(w, (uint32_t)len) &&
               write_bytes(w, kw, len);
    }
}

// Write symbol with optimal encoding
static bool write_symbol(nippy_writer_t *w, const char *sym) {
    size_t len = strlen(sym);

    if (len <= 255) {
        return write_byte(w, NIPPY_TYPE_SYMBOL_SM) &&
               write_byte(w, (uint8_t)len) &&
               write_bytes(w, sym, len);
    } else {
        return write_byte(w, NIPPY_TYPE_SYMBOL_MD) &&
               write_uint16_be(w, (uint16_t)len) &&
               write_bytes(w, sym, len);
    }
}

// Parse hex character to nibble value
static int hex_to_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Write UUID (16 bytes binary)
// Input format: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" (36 chars)
static bool write_uuid(nippy_writer_t *w, const char *uuid_str) {
    if (!uuid_str || strlen(uuid_str) != 36) {
        snprintf(w->error_buffer, sizeof(w->error_buffer), "Invalid UUID string length");
        return false;
    }

    uint8_t uuid_bytes[16];
    int byte_idx = 0;

    for (int i = 0; i < 36 && byte_idx < 16; i++) {
        char c = uuid_str[i];
        if (c == '-') {
            continue;  // Skip hyphens
        }

        int high = hex_to_nibble(c);
        if (high < 0 || i + 1 >= 36) {
            snprintf(w->error_buffer, sizeof(w->error_buffer), "Invalid UUID hex character");
            return false;
        }

        i++;
        // Skip hyphen if next char
        if (uuid_str[i] == '-') {
            i++;
        }

        int low = hex_to_nibble(uuid_str[i]);
        if (low < 0) {
            snprintf(w->error_buffer, sizeof(w->error_buffer), "Invalid UUID hex character");
            return false;
        }

        uuid_bytes[byte_idx++] = (uint8_t)((high << 4) | low);
    }

    if (byte_idx != 16) {
        snprintf(w->error_buffer, sizeof(w->error_buffer), "Invalid UUID format");
        return false;
    }

    return write_byte(w, NIPPY_TYPE_UUID) && write_bytes(w, uuid_bytes, 16);
}

// Write collection start
static bool write_collection_start(nippy_writer_t *w, collection_type_t type, size_t count) {
    uint8_t type_tag;

    switch (type) {
        case COLLECTION_VECTOR:
            if (count == 0) {
                type_tag = NIPPY_TYPE_VECTOR_0;
            } else if (count <= 255) {
                type_tag = NIPPY_TYPE_VECTOR_SM;
            } else if (count <= 65535) {
                type_tag = NIPPY_TYPE_VECTOR_MD;
            } else {
                type_tag = NIPPY_TYPE_VECTOR_LG;
            }
            break;

        case COLLECTION_SET:
            if (count == 0) {
                type_tag = NIPPY_TYPE_SET_0;
            } else if (count <= 255) {
                type_tag = NIPPY_TYPE_SET_SM;
            } else if (count <= 65535) {
                type_tag = NIPPY_TYPE_SET_MD;
            } else {
                type_tag = NIPPY_TYPE_SET_LG;
            }
            break;

        case COLLECTION_MAP:
            if (count == 0) {
                type_tag = NIPPY_TYPE_MAP_0;
            } else if (count <= 255) {
                type_tag = NIPPY_TYPE_MAP_SM;
            } else if (count <= 65535) {
                type_tag = NIPPY_TYPE_MAP_MD;
            } else {
                type_tag = NIPPY_TYPE_MAP_LG;
            }
            break;

        case COLLECTION_LIST:
            if (count == 0) {
                type_tag = NIPPY_TYPE_LIST_0;
            } else if (count <= 255) {
                type_tag = NIPPY_TYPE_LIST_SM;
            } else if (count <= 65535) {
                type_tag = NIPPY_TYPE_LIST_MD;
            } else {
                type_tag = NIPPY_TYPE_LIST_LG;
            }
            break;

        default:
            snprintf(w->error_buffer, sizeof(w->error_buffer), "Unknown collection type");
            return false;
    }

    if (!write_byte(w, type_tag)) {
        return false;
    }

    // Write count (except for empty collections)
    if (count > 0) {
        if (count <= 255) {
            if (!write_byte(w, (uint8_t)count)) {
                return false;
            }
        } else if (count <= 65535) {
            if (!write_uint16_be(w, (uint16_t)count)) {
                return false;
            }
        } else {
            if (!write_uint32_be(w, (uint32_t)count)) {
                return false;
            }
        }
    }

    return true;
}

// API functions
nippy_writer_t* nippy_writer_create(FILE *output) {
    assert(output != NULL);

    nippy_writer_t *w = malloc(sizeof(nippy_writer_t));
    if (!w) {
        return NULL;
    }

    w->output = output;
    w->stack_depth = 0;
    w->header_written = false;
    w->error_buffer[0] = '\0';

    return w;
}

bool nippy_writer_write_event(nippy_writer_t *w, const parse_event_t *event) {
    assert(w != NULL);
    assert(event != NULL);

    // Write header on first event
    if (!w->header_written) {
        if (!write_header(w)) {
            return false;
        }
    }

    // Update collection context
    collection_context_t *ctx = current_collection(w);

    switch (event->type) {
        case EVENT_KEY:
            // EVENT_KEY is treated the same as EVENT_VALUE in Nippy format
            // Fall through to EVENT_VALUE
        case EVENT_VALUE:
            switch (event->value_type) {
                case VALUE_NIL:
                    if (!write_byte(w, NIPPY_TYPE_NIL)) {
                        return false;
                    }
                    break;

                case VALUE_BOOL:
                    if (!write_byte(w, event->value.bool_val ? NIPPY_TYPE_TRUE : NIPPY_TYPE_FALSE)) {
                        return false;
                    }
                    break;

                case VALUE_INT64:
                    if (!write_integer(w, event->value.int_val)) {
                        return false;
                    }
                    break;

                case VALUE_DOUBLE:
                    if (!write_byte(w, NIPPY_TYPE_DOUBLE) ||
                        !write_double(w, event->value.float_val)) {
                        return false;
                    }
                    break;

                case VALUE_STRING:
                    if (!write_string(w, event->value.string_val)) {
                        return false;
                    }
                    break;

                case VALUE_KEYWORD:
                    if (!write_keyword(w, event->value.string_val)) {
                        return false;
                    }
                    break;

                case VALUE_SYMBOL:
                    if (!write_symbol(w, event->value.string_val)) {
                        return false;
                    }
                    break;

                case VALUE_CHAR:
                    snprintf(w->error_buffer, sizeof(w->error_buffer),
                             "Unsupported value type for Nippy: %d", event->value_type);
                    return false;

                case VALUE_UUID:
                    if (!write_uuid(w, event->value.string_val)) {
                        return false;
                    }
                    break;

                default:
                    snprintf(w->error_buffer, sizeof(w->error_buffer),
                             "Unknown value type: %d", event->value_type);
                    return false;
            }

            // Update collection context
            if (ctx) {
                ctx->current_count++;
                if (ctx->type == COLLECTION_MAP) {
                    ctx->expecting_key = !ctx->expecting_key;
                }
            }
            break;

        case EVENT_START_VECTOR:
            if (!write_collection_start(w, COLLECTION_VECTOR, event->collection_size)) {
                return false;
            }
            push_collection(w, COLLECTION_VECTOR, event->collection_size);
            break;

        case EVENT_START_MAP:
            if (!write_collection_start(w, COLLECTION_MAP, event->collection_size)) {
                return false;
            }
            push_collection(w, COLLECTION_MAP, event->collection_size);
            break;

        case EVENT_START_SET:
            if (!write_collection_start(w, COLLECTION_SET, event->collection_size)) {
                return false;
            }
            push_collection(w, COLLECTION_SET, event->collection_size);
            break;

        case EVENT_START_LIST:
            if (!write_collection_start(w, COLLECTION_LIST, event->collection_size)) {
                return false;
            }
            push_collection(w, COLLECTION_LIST, event->collection_size);
            break;

        case EVENT_END_VECTOR:
        case EVENT_END_MAP:
        case EVENT_END_SET:
        case EVENT_END_LIST:
            if (!ctx) {
                snprintf(w->error_buffer, sizeof(w->error_buffer),
                         "Unexpected end event with no collection context");
                return false;
            }

            // Verify count matches
            size_t expected = ctx->expected_count;
            if (ctx->type == COLLECTION_MAP) {
                expected *= 2;  // Maps have key-value pairs
            }

            if (ctx->current_count != expected) {
                snprintf(w->error_buffer, sizeof(w->error_buffer),
                         "Collection count mismatch: expected %zu, got %zu",
                         expected, ctx->current_count);
                return false;
            }

            pop_collection(w);

            // Update parent collection context
            ctx = current_collection(w);
            if (ctx) {
                ctx->current_count++;
                if (ctx->type == COLLECTION_MAP) {
                    ctx->expecting_key = !ctx->expecting_key;
                }
            }
            break;

        case EVENT_EOF:
        case EVENT_ERROR:
            // Nothing to write
            break;
    }

    return true;
}

const char* nippy_writer_error(nippy_writer_t *w) {
    assert(w != NULL);
    return w->error_buffer[0] ? w->error_buffer : NULL;
}

void nippy_writer_destroy(nippy_writer_t *w) {
    if (!w) {
        return;
    }

    free(w);
}
