#include "nippy_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Nippy type tags (based on nippy format)
#define NIPPY_TYPE_NIL           0x03  // nil value (3)
#define NIPPY_TYPE_TRUE          0x08  // true value (8)
#define NIPPY_TYPE_FALSE         0x09  // false value (9)
#define NIPPY_TYPE_BYTE          0x28  // byte (40)
#define NIPPY_TYPE_SHORT         0x29  // short (41)
#define NIPPY_TYPE_INT           0x2A  // integer (42)
#define NIPPY_TYPE_LONG          0x2B  // long-xl (43)
#define NIPPY_TYPE_FLOAT         0x3C  // float (60)
#define NIPPY_TYPE_DOUBLE        0x3D  // double (61)

// Optimized long types (positive/negative variants)
#define NIPPY_TYPE_LONG_POS_SM   0x57  // long-pos-sm (87) - 1-byte positive
#define NIPPY_TYPE_LONG_POS_MD   0x58  // long-pos-md (88) - 2-byte positive
#define NIPPY_TYPE_LONG_POS_LG   0x59  // long-pos-lg (89) - 4-byte positive
#define NIPPY_TYPE_LONG_NEG_SM   0x5D  // long-neg-sm (93) - 1-byte negative
#define NIPPY_TYPE_LONG_NEG_MD   0x5E  // long-neg-md (94) - 2-byte negative
#define NIPPY_TYPE_LONG_NEG_LG   0x5F  // long-neg-lg (95) - 4-byte negative

// Optimized long types (deprecated, v3.3.0+)
#define NIPPY_TYPE_LONG_SM_      0x64  // long-sm_ (100) - 1-byte signed
#define NIPPY_TYPE_LONG_MD_      0x65  // long-md_ (101) - 2-byte signed
#define NIPPY_TYPE_LONG_LG_      0x66  // long-lg_ (102) - 4-byte signed

// String types (IDs from nippy.clj types-spec)
#define NIPPY_TYPE_STRING_0      0x22  // 34 - str-0 (empty)
#define NIPPY_TYPE_STRING_SM     0x60  // 96 - str-sm* (unsigned, 1-byte length)
#define NIPPY_TYPE_STRING_MD     0x10  // 16 - str-md (2-byte length)
#define NIPPY_TYPE_STRING_LG     0x0D  // 13 - str-lg (4-byte length)
#define NIPPY_TYPE_STRING_SM_    0x69  // 105 - str-sm_ (signed, 1-byte length, v3.3.0+)

// Keyword types (IDs from nippy.clj types-spec)
#define NIPPY_TYPE_KEYWORD_SM    0x6A  // 106 - kw-sm (1-byte length)
#define NIPPY_TYPE_KEYWORD_MD    0x55  // 85 - kw-md (2-byte length)
#define NIPPY_TYPE_KEYWORD_LG    0x4D  // 77 - kw-md_ (4-byte length, buggy but still used)

// Symbol types
#define NIPPY_TYPE_SYMBOL_SM     0x38  // 56 - sym-sm (1-byte length)
#define NIPPY_TYPE_SYMBOL_MD     0x56  // 86 - sym-md (2-byte length)

// Collection types (IDs from nippy.clj types-spec)
#define NIPPY_TYPE_VECTOR_0      0x11  // 17 - vec-0
#define NIPPY_TYPE_VECTOR_SM     0x61  // 97 - vec-sm* (unsigned, 1-byte count)
#define NIPPY_TYPE_VECTOR_MD     0x45  // 69 - vec-md (2-byte count)
#define NIPPY_TYPE_VECTOR_LG     0x15  // 21 - vec-lg (4-byte count)
#define NIPPY_TYPE_VECTOR_SM_    0x6E  // 110 - vec-sm_ (signed, 1-byte count, v3.3.0+)

#define NIPPY_TYPE_SET_0         0x12  // 18 - set-0
#define NIPPY_TYPE_SET_SM        0x62  // 98 - set-sm* (unsigned, 1-byte count)
#define NIPPY_TYPE_SET_MD        0x20  // 32 - set-md (2-byte count)
#define NIPPY_TYPE_SET_LG        0x17  // 23 - set-lg (4-byte count)
#define NIPPY_TYPE_SET_SM_       0x6F  // 111 - set-sm_ (signed, 1-byte count, v3.3.0+)

#define NIPPY_TYPE_MAP_0         0x13  // 19 - map-0
#define NIPPY_TYPE_MAP_SM        0x63  // 99 - map-sm* (unsigned, 1-byte count * 2)
#define NIPPY_TYPE_MAP_MD        0x21  // 33 - map-md (2-byte count * 2)
#define NIPPY_TYPE_MAP_LG        0x1E  // 30 - map-lg (4-byte count * 2)
#define NIPPY_TYPE_MAP_SM_       0x70  // 112 - map-sm_ (signed, 1-byte count * 2, v3.3.0+)

#define NIPPY_TYPE_LIST_0        0x23  // 35 - list-0
#define NIPPY_TYPE_LIST_SM       0x24  // 36 - list-sm (1-byte count)
#define NIPPY_TYPE_LIST_MD       0x36  // 54 - list-md (2-byte count)
#define NIPPY_TYPE_LIST_LG       0x14  // 20 - list-lg (4-byte count)

// Arrays (Java native arrays, treated as vectors in EDN)
#define NIPPY_TYPE_OBJECT_ARRAY_LG 0x73  // object-array-lg

// Additional types
#define NIPPY_TYPE_LONG_0       0x00  // long 0 (0)
#define NIPPY_TYPE_CHAR         0x0A  // char (10)
#define NIPPY_TYPE_CACHED_0     0x3B  // Cached value definition 0 (59)
#define NIPPY_TYPE_CACHED_1     0x3F  // Cached value definition 1 (63)
#define NIPPY_TYPE_CACHED_2     0x40  // Cached value definition 2 (64)
#define NIPPY_TYPE_CACHED_3     0x41  // Cached value definition 3 (65)
#define NIPPY_TYPE_UTIL_DATE    0x5A  // java.util.Date (8 bytes) (90)
#define NIPPY_TYPE_UUID         0x5B  // UUID (16 bytes) (91)

//=============================================================================
// Lookup table for fast type dispatch
//=============================================================================
typedef enum {
    CAT_UNKNOWN = 0,
    CAT_NIL,
    CAT_TRUE,
    CAT_FALSE,
    CAT_LONG,
    CAT_FLOAT,
    CAT_DOUBLE,
    CAT_CHAR,
    CAT_STRING,
    CAT_KEYWORD,
    CAT_SYMBOL,
    CAT_VECTOR,
    CAT_SET,
    CAT_MAP,
    CAT_LIST,
    CAT_DATE,
    CAT_UUID
} tag_category_t;

// Combined info: category (4 bits) + length_size (4 bits)
// length_size: 0=none, 1=1byte, 2=2bytes, 4=4bytes, 8=8bytes
typedef struct {
    uint8_t category;
    uint8_t length_size;
    uint8_t data_size;    // Fixed data size (for integers, etc.), 0 = variable
    uint8_t flags;        // Reserved for future use
} tag_info_t;

#define TAG_INFO(cat, len, data) { CAT_##cat, len, data, 0 }
#define TAG_UNKNOWN_INFO { CAT_UNKNOWN, 0, 0, 0 }

static const tag_info_t TAG_LOOKUP[256] = {
    // 0x00 - 0x0F
    [0x00] = TAG_INFO(LONG, 0, 0),      // long-0
    [0x03] = TAG_INFO(NIL, 0, 0),       // nil
    [0x08] = TAG_INFO(TRUE, 0, 0),      // true
    [0x09] = TAG_INFO(FALSE, 0, 0),     // false
    [0x0A] = TAG_INFO(CHAR, 0, 2),      // char (2 bytes)
    [0x0D] = TAG_INFO(STRING, 4, 0),    // str-lg

    // 0x10 - 0x1F
    [0x10] = TAG_INFO(STRING, 2, 0),    // str-md
    [0x11] = TAG_INFO(VECTOR, 0, 0),    // vec-0
    [0x12] = TAG_INFO(SET, 0, 0),       // set-0
    [0x13] = TAG_INFO(MAP, 0, 0),       // map-0
    [0x14] = TAG_INFO(LIST, 4, 0),      // list-lg
    [0x15] = TAG_INFO(VECTOR, 4, 0),    // vec-lg
    [0x17] = TAG_INFO(SET, 4, 0),       // set-lg
    [0x1E] = TAG_INFO(MAP, 4, 0),       // map-lg

    // 0x20 - 0x2F
    [0x20] = TAG_INFO(SET, 2, 0),       // set-md
    [0x21] = TAG_INFO(MAP, 2, 0),       // map-md
    [0x22] = TAG_INFO(STRING, 0, 0),    // str-0
    [0x23] = TAG_INFO(LIST, 0, 0),      // list-0
    [0x24] = TAG_INFO(LIST, 1, 0),      // list-sm
    [0x28] = TAG_INFO(LONG, 0, 1),      // byte
    [0x29] = TAG_INFO(LONG, 0, 2),      // short
    [0x2A] = TAG_INFO(LONG, 0, 4),      // int
    [0x2B] = TAG_INFO(LONG, 0, 8),      // long-xl

    // 0x30 - 0x3F
    [0x36] = TAG_INFO(LIST, 2, 0),      // list-md
    [0x38] = TAG_INFO(SYMBOL, 1, 0),    // sym-sm
    [0x3C] = TAG_INFO(FLOAT, 0, 4),     // float
    [0x3D] = TAG_INFO(DOUBLE, 0, 8),    // double

    // 0x40 - 0x4F
    [0x45] = TAG_INFO(VECTOR, 2, 0),    // vec-md
    [0x4D] = TAG_INFO(KEYWORD, 4, 0),   // kw-lg

    // 0x50 - 0x5F
    [0x55] = TAG_INFO(KEYWORD, 2, 0),   // kw-md
    [0x56] = TAG_INFO(SYMBOL, 2, 0),    // sym-md
    [0x57] = TAG_INFO(LONG, 0, 1),      // long-pos-sm
    [0x58] = TAG_INFO(LONG, 0, 2),      // long-pos-md
    [0x59] = TAG_INFO(LONG, 0, 4),      // long-pos-lg
    [0x5A] = TAG_INFO(DATE, 0, 8),      // util.Date
    [0x5B] = TAG_INFO(UUID, 0, 16),     // UUID
    [0x5D] = TAG_INFO(LONG, 0, 1),      // long-neg-sm
    [0x5E] = TAG_INFO(LONG, 0, 2),      // long-neg-md
    [0x5F] = TAG_INFO(LONG, 0, 4),      // long-neg-lg

    // 0x60 - 0x6F
    [0x60] = TAG_INFO(STRING, 1, 0),    // str-sm
    [0x61] = TAG_INFO(VECTOR, 1, 0),    // vec-sm
    [0x62] = TAG_INFO(SET, 1, 0),       // set-sm
    [0x63] = TAG_INFO(MAP, 1, 0),       // map-sm
    [0x64] = TAG_INFO(LONG, 0, 1),      // long-sm_ (signed)
    [0x65] = TAG_INFO(LONG, 0, 2),      // long-md_ (signed)
    [0x66] = TAG_INFO(LONG, 0, 4),      // long-lg_ (signed)
    [0x69] = TAG_INFO(STRING, 1, 0),    // str-sm_
    [0x6A] = TAG_INFO(KEYWORD, 1, 0),   // kw-sm
    [0x6E] = TAG_INFO(VECTOR, 1, 0),    // vec-sm_
    [0x6F] = TAG_INFO(SET, 1, 0),       // set-sm_

    // 0x70 - 0x7F
    [0x70] = TAG_INFO(MAP, 1, 0),       // map-sm_
    [0x73] = TAG_INFO(VECTOR, 4, 0),    // object-array-lg
};

// Fast lookup macros
#define TAG_CATEGORY(tag)     (TAG_LOOKUP[tag].category)
#define TAG_LENGTH_SIZE(tag)  (TAG_LOOKUP[tag].length_size)
#define TAG_DATA_SIZE(tag)    (TAG_LOOKUP[tag].data_size)

#define IS_STRING_TYPE(tag)   (TAG_CATEGORY(tag) == CAT_STRING)
#define IS_KEYWORD_TYPE(tag)  (TAG_CATEGORY(tag) == CAT_KEYWORD)
#define IS_SYMBOL_TYPE(tag)   (TAG_CATEGORY(tag) == CAT_SYMBOL)
#define IS_VECTOR_TYPE(tag)   (TAG_CATEGORY(tag) == CAT_VECTOR)
#define IS_SET_TYPE(tag)      (TAG_CATEGORY(tag) == CAT_SET)
#define IS_MAP_TYPE(tag)      (TAG_CATEGORY(tag) == CAT_MAP)
#define IS_LIST_TYPE(tag)     (TAG_CATEGORY(tag) == CAT_LIST)

// Parser structure with PDA stack
// Nippy header info
typedef struct {
    bool present;           // Whether header was found
    uint8_t version;        // Header format version
    uint8_t meta_byte;      // Metadata byte
    bool compressed;        // Has compression
    bool encrypted;         // Has encryption
} nippy_header_t;

struct nippy_parser {
    buffer_t *buffer;
    arena_t *arena;

    // Header info
    nippy_header_t header;

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

#ifdef USE_LZ4
    // Memory stream (NULL if using external FILE*)
    // Only used for create_from_bytes
    FILE *mem_stream;
#endif
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

// Read length prefix using lookup table
static inline size_t read_length_prefix(nippy_parser_t *p, uint8_t tag) {
    uint8_t len_size = TAG_LENGTH_SIZE(tag);

    switch (len_size) {
        case 0:
            return 0;
        case 1: {
            int b = buffer_read_byte(p->buffer);
            return (b >= 0) ? (size_t)((uint8_t)b) : 0;
        }
        case 2: {
            int16_t len = buffer_read_int16_be(p->buffer);
            return (len >= 0) ? (size_t)len : 0;
        }
        case 4: {
            int32_t len = buffer_read_int32_be(p->buffer);
            return (len >= 0) ? (size_t)len : 0;
        }
        default:
            return 0;
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
    ev->value_length = 0;

    switch (tag) {
        case NIPPY_TYPE_NIL:
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_NIL;
            return true;

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

        // Optimized positive long types (unsigned reads)
        case NIPPY_TYPE_LONG_POS_SM: {
            uint8_t byte = buffer_read_byte(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = byte;
            return true;
        }

        case NIPPY_TYPE_LONG_POS_MD: {
            uint16_t val = buffer_read_uint16_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = val;
            return true;
        }

        case NIPPY_TYPE_LONG_POS_LG: {
            uint32_t val = buffer_read_uint32_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = val;
            return true;
        }

        // Optimized negative long types (unsigned read, then negate)
        case NIPPY_TYPE_LONG_NEG_SM: {
            uint8_t byte = buffer_read_byte(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = -(int64_t)byte;
            return true;
        }

        case NIPPY_TYPE_LONG_NEG_MD: {
            uint16_t val = buffer_read_uint16_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = -(int64_t)val;
            return true;
        }

        case NIPPY_TYPE_LONG_NEG_LG: {
            uint32_t val = buffer_read_uint32_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = -(int64_t)val;
            return true;
        }

        // Deprecated signed long types (v3.3.0+)
        case NIPPY_TYPE_LONG_SM_: {
            int8_t byte = buffer_read_byte(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = byte;
            return true;
        }

        case NIPPY_TYPE_LONG_MD_: {
            int16_t val = buffer_read_int16_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = val;
            return true;
        }

        case NIPPY_TYPE_LONG_LG_: {
            int32_t val = buffer_read_int32_be(p->buffer);
            ev->type = EVENT_VALUE;
            ev->value_type = VALUE_INT64;
            ev->value.int_val = val;
            return true;
        }

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
            ev->value_type = VALUE_UUID;
            ev->value.string_val = uuid_str;
            ev->value_length = 36;
            return true;
        }

        default:
            if (IS_STRING_TYPE(tag)) {
                size_t len = read_length_prefix(p, tag);
                char *str = read_string_data(p, len);
                if (!str && len > 0) {
                    return false;
                }
                ev->type = EVENT_VALUE;
                ev->value_type = VALUE_STRING;
                ev->value.string_val = str;
                ev->value_length = len;
                return true;
            }
            else if (IS_KEYWORD_TYPE(tag)) {
                size_t len = read_length_prefix(p, tag);
                char *str = read_string_data(p, len);
                if (!str && len > 0) {
                    return false;
                }
                ev->type = EVENT_VALUE;
                ev->value_type = VALUE_KEYWORD;
                ev->value.string_val = str;
                ev->value_length = len;
                return true;
            }
            else if (IS_SYMBOL_TYPE(tag)) {
                size_t len = read_length_prefix(p, tag);
                char *str = read_string_data(p, len);
                if (!str && len > 0) {
                    return false;
                }
                ev->type = EVENT_VALUE;
                ev->value_type = VALUE_SYMBOL;
                ev->value.string_val = str;
                ev->value_length = len;
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

// Read and validate Nippy header (if present)
// Returns true on success, false on error
static bool read_nippy_header(nippy_parser_t *p) {
    // Peek at first 4 bytes to check for header
    int b1 = buffer_peek_byte(p->buffer);
    if (b1 < 0) {
        // Empty file or read error
        p->header.present = false;
        return true;
    }

    // Check for "NPY" signature
    if (b1 != 0x4E) {  // 'N'
        // No header, regular data starts immediately
        p->header.present = false;
        return true;
    }

    // Read potential header
    uint8_t header_bytes[4];
    header_bytes[0] = buffer_read_byte(p->buffer);
    header_bytes[1] = buffer_read_byte(p->buffer);
    header_bytes[2] = buffer_read_byte(p->buffer);
    header_bytes[3] = buffer_read_byte(p->buffer);

    // Verify "NPY" signature
    if (header_bytes[0] != 0x4E || header_bytes[1] != 0x50 || header_bytes[2] != 0x59) {
        snprintf(p->error_buffer, sizeof(p->error_buffer),
                 "Invalid Nippy header signature");
        return false;
    }

    // Parse metadata byte
    uint8_t meta = header_bytes[3];
    p->header.present = true;
    p->header.version = 1;  // Current version
    p->header.meta_byte = meta;

    // Decode compression and encryption flags
    // Based on nippy.clj head-meta mapping
    switch (meta) {
        case 0:  // No compression, no encryption
            p->header.compressed = false;
            p->header.encrypted = false;
            break;

        case 1:  // Snappy compression, no encryption
        case 8:  // LZ4 compression, no encryption
        case 11: // LZMA2 compression, no encryption
        case 20: // ZSTD compression, no encryption
        case 5:  // Other compression, no encryption
            p->header.compressed = true;
            p->header.encrypted = false;
            snprintf(p->error_buffer, sizeof(p->error_buffer),
                     "Compression not yet supported (meta byte: 0x%02X)", meta);
            return false;

        case 2:  // No compression, AES128-CBC-SHA512
        case 14: // No compression, AES128-GCM-SHA512
        case 4:  // No compression, other encryption
        case 3:  // Snappy + encryption
        case 7:  // Snappy + other encryption
        case 9:  // LZ4 + AES128-CBC-SHA512
        case 10: // LZ4 + other encryption
        case 12: // LZMA2 + AES128-CBC-SHA512
        case 13: // LZMA2 + other encryption
        case 15: // Snappy + AES128-GCM-SHA512
        case 16: // LZ4 + AES128-GCM-SHA512
        case 17: // LZMA2 + AES128-GCM-SHA512
        case 18: // Other compression + AES128-CBC-SHA512
        case 19: // Other compression + AES128-GCM-SHA512
        case 21: // ZSTD + AES128-CBC-SHA512
        case 22: // ZSTD + AES128-GCM-SHA512
        case 23: // ZSTD + other encryption
        case 6:  // Other compression + other encryption
            p->header.compressed = (meta != 2 && meta != 14 && meta != 4);
            p->header.encrypted = true;
            snprintf(p->error_buffer, sizeof(p->error_buffer),
                     "Encryption not yet supported (meta byte: 0x%02X)", meta);
            return false;

        default:
            snprintf(p->error_buffer, sizeof(p->error_buffer),
                     "Unknown Nippy header metadata: 0x%02X", meta);
            return false;
    }

    return true;
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
#ifdef USE_LZ4
    p->mem_stream = NULL;  // No memory stream for external FILE*
#endif

    memset(&p->current_event, 0, sizeof(parse_event_t));
    memset(&p->header, 0, sizeof(nippy_header_t));

    // Try to read Nippy header (if present)
    if (!read_nippy_header(p)) {
        // Header read failed - error already in buffer
        fprintf(stderr, "Nippy header error: %s\n", p->error_buffer);
        buffer_destroy(p->buffer);
        free(p);
        return NULL;
    }

    return p;
}

#ifdef USE_LZ4
nippy_parser_t* nippy_parser_create_from_bytes(const uint8_t *data, size_t size, arena_t *arena) {
    assert(data != NULL);
    assert(arena != NULL);

    // Create memory stream from byte array
    // Note: fmemopen expects non-const pointer, but it won't modify data in "rb" mode
    FILE *mem_stream = fmemopen((void*)data, size, "rb");
    if (!mem_stream) {
        return NULL;
    }

    // Create parser using the memory stream
    nippy_parser_t *p = nippy_parser_create(mem_stream, arena);
    if (!p) {
        fclose(mem_stream);
        return NULL;
    }

    // Store memory stream so it can be closed in destroy
    p->mem_stream = mem_stream;

    return p;
}
#endif

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
                if (IS_VECTOR_TYPE(tag) || IS_SET_TYPE(tag) ||
                    IS_MAP_TYPE(tag) || IS_LIST_TYPE(tag)) {
                    size_t count = read_length_prefix(p, tag);

                    // Update parent context before pushing new one
                    update_context(p);

                    // Determine collection type and emit START event
                    if (IS_VECTOR_TYPE(tag)) {
                        p->current_event.type = EVENT_START_VECTOR;
                        push_context(p, CONTEXT_VECTOR, count);
                    } else if (IS_SET_TYPE(tag)) {
                        p->current_event.type = EVENT_START_SET;
                        push_context(p, CONTEXT_SET, count);
                    } else if (IS_MAP_TYPE(tag)) {
                        p->current_event.type = EVENT_START_MAP;
                        push_context(p, CONTEXT_MAP_KEY, count);
                    } else if (IS_LIST_TYPE(tag)) {
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

#ifdef USE_LZ4
    // Close memory stream if we created one
    if (p->mem_stream) {
        fclose(p->mem_stream);
    }
#endif

    free(p);
}
