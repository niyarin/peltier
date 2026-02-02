#include "edn_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>

#define MAX_TOKEN_SIZE 65536

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NIL,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_INTEGER,
    TOKEN_DOUBLE,
    TOKEN_STRING,
    TOKEN_KEYWORD,
    TOKEN_LBRACKET,    // [
    TOKEN_RBRACKET,    // ]
    TOKEN_LBRACE,      // {
    TOKEN_RBRACE,      // }
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_HASH_LBRACE  // #{
} token_type_t;

typedef struct {
    token_type_t type;
    char *str_value;
    int64_t int_value;
    double double_value;
} token_t;

typedef enum {
    COLL_VECTOR,
    COLL_MAP,
    COLL_SET,
    COLL_LIST
} collection_type_t;

typedef struct {
    collection_type_t type;
    size_t count;
    bool is_map_key;  // For maps: true if expecting key
} collection_state_t;

// Event buffer for small collections
typedef struct {
    parse_event_t *events;
    size_t count;
    size_t capacity;
} event_buffer_t;

struct edn_parser {
    FILE *input;
    arena_t *arena;

    // Tokenizer state
    int current_char;
    bool eof_reached;

    // Collection stack
    collection_state_t stack[MAX_NESTING_DEPTH];
    size_t stack_depth;

    // Current event
    parse_event_t current_event;

    // Buffered token
    token_t current_token;
    bool token_buffered;

    // Error message
    char error_buffer[256];

    // Token buffer
    char token_buffer[MAX_TOKEN_SIZE];

    // Hybrid strategy options
    size_t buffer_threshold;  // Elements threshold for buffering
    bool is_seekable;         // Whether input stream supports fseek

    // Event buffer for small collections
    event_buffer_t event_buffer;
    size_t buffer_read_index;  // For reading buffered events
    bool buffering_mode;       // Currently buffering events
};

// Character reading
static int read_char(edn_parser_t *p) {
    if (p->eof_reached) {
        return EOF;
    }
    int c = fgetc(p->input);
    if (c == EOF) {
        p->eof_reached = true;
    }
    return c;
}

static int peek_char(edn_parser_t *p) {
    return p->current_char;
}

static void consume_char(edn_parser_t *p) {
    p->current_char = read_char(p);
}

static void skip_whitespace(edn_parser_t *p) {
    while (isspace(peek_char(p))) {
        consume_char(p);
    }
}

static void skip_comment(edn_parser_t *p) {
    if (peek_char(p) == ';') {
        while (peek_char(p) != '\n' && peek_char(p) != EOF) {
            consume_char(p);
        }
    }
}

// Tokenizer
static bool read_string(edn_parser_t *p, token_t *token) {
    assert(peek_char(p) == '"');
    consume_char(p);  // Skip opening quote

    size_t pos = 0;
    while (peek_char(p) != '"' && peek_char(p) != EOF) {
        if (peek_char(p) == '\\') {
            consume_char(p);
            int next = peek_char(p);
            switch (next) {
                case 'n': p->token_buffer[pos++] = '\n'; break;
                case 't': p->token_buffer[pos++] = '\t'; break;
                case 'r': p->token_buffer[pos++] = '\r'; break;
                case '"': p->token_buffer[pos++] = '"'; break;
                case '\\': p->token_buffer[pos++] = '\\'; break;
                default: p->token_buffer[pos++] = next; break;
            }
            consume_char(p);
        } else {
            p->token_buffer[pos++] = peek_char(p);
            consume_char(p);
        }

        if (pos >= MAX_TOKEN_SIZE - 1) {
            snprintf(p->error_buffer, sizeof(p->error_buffer), "String too long");
            return false;
        }
    }

    if (peek_char(p) != '"') {
        snprintf(p->error_buffer, sizeof(p->error_buffer), "Unterminated string");
        return false;
    }

    consume_char(p);  // Skip closing quote
    p->token_buffer[pos] = '\0';

    token->type = TOKEN_STRING;
    token->str_value = arena_strdup(p->arena, p->token_buffer);
    return true;
}

static bool read_number(edn_parser_t *p, token_t *token) {
    size_t pos = 0;
    bool is_double = false;

    // Handle negative sign
    if (peek_char(p) == '-') {
        p->token_buffer[pos++] = peek_char(p);
        consume_char(p);
    }

    // Read digits
    while (isdigit(peek_char(p)) || peek_char(p) == '.' || peek_char(p) == 'e' || peek_char(p) == 'E') {
        if (peek_char(p) == '.' || peek_char(p) == 'e' || peek_char(p) == 'E') {
            is_double = true;
        }
        p->token_buffer[pos++] = peek_char(p);
        consume_char(p);

        if (pos >= MAX_TOKEN_SIZE - 1) {
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Number too long");
            return false;
        }
    }

    p->token_buffer[pos] = '\0';

    if (is_double) {
        token->type = TOKEN_DOUBLE;
        token->double_value = atof(p->token_buffer);
    } else {
        token->type = TOKEN_INTEGER;
        token->int_value = atoll(p->token_buffer);
    }

    return true;
}

static bool read_symbol_or_keyword(edn_parser_t *p, token_t *token) {
    size_t pos = 0;
    bool is_keyword = false;

    if (peek_char(p) == ':') {
        is_keyword = true;
        p->token_buffer[pos++] = peek_char(p);
        consume_char(p);
    }

    while (peek_char(p) != EOF && !isspace(peek_char(p)) &&
           peek_char(p) != ']' && peek_char(p) != '}' && peek_char(p) != ')' &&
           peek_char(p) != ',' && peek_char(p) != ';') {
        p->token_buffer[pos++] = peek_char(p);
        consume_char(p);

        if (pos >= MAX_TOKEN_SIZE - 1) {
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Symbol/keyword too long");
            return false;
        }
    }

    p->token_buffer[pos] = '\0';

    if (is_keyword) {
        token->type = TOKEN_KEYWORD;
        token->str_value = arena_strdup(p->arena, p->token_buffer);
    } else {
        // Check for special symbols
        if (strcmp(p->token_buffer, "nil") == 0) {
            token->type = TOKEN_NIL;
        } else if (strcmp(p->token_buffer, "true") == 0) {
            token->type = TOKEN_TRUE;
        } else if (strcmp(p->token_buffer, "false") == 0) {
            token->type = TOKEN_FALSE;
        } else {
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Unknown symbol: %s", p->token_buffer);
            return false;
        }
    }

    return true;
}

static bool next_token(edn_parser_t *p, token_t *token) {
    if (p->token_buffered) {
        *token = p->current_token;
        p->token_buffered = false;
        return true;
    }

    skip_whitespace(p);
    skip_comment(p);
    skip_whitespace(p);

    int c = peek_char(p);

    if (c == EOF) {
        token->type = TOKEN_EOF;
        return true;
    }

    // Single character tokens
    switch (c) {
        case '[':
            token->type = TOKEN_LBRACKET;
            consume_char(p);
            return true;
        case ']':
            token->type = TOKEN_RBRACKET;
            consume_char(p);
            return true;
        case '{':
            token->type = TOKEN_LBRACE;
            consume_char(p);
            return true;
        case '}':
            token->type = TOKEN_RBRACE;
            consume_char(p);
            return true;
        case '(':
            token->type = TOKEN_LPAREN;
            consume_char(p);
            return true;
        case ')':
            token->type = TOKEN_RPAREN;
            consume_char(p);
            return true;
        case '#':
            consume_char(p);
            if (peek_char(p) == '{') {
                token->type = TOKEN_HASH_LBRACE;
                consume_char(p);
                return true;
            } else {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected character after #");
                return false;
            }
        case '"':
            return read_string(p, token);
        case ',':
            consume_char(p);  // Ignore commas
            return next_token(p, token);
        default:
            if (isdigit(c) || (c == '-' && isdigit(p->current_char))) {
                return read_number(p, token);
            } else if (c == ':' || isalpha(c) || c == '-' || c == '+' || c == '*' || c == '/') {
                return read_symbol_or_keyword(p, token);
            } else {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected character: %c", c);
                return false;
            }
    }
}

// Event buffer management
static bool buffer_add_event(edn_parser_t *p, const parse_event_t *event) {
    if (p->event_buffer.count >= p->event_buffer.capacity) {
        size_t new_capacity = p->event_buffer.capacity == 0 ? 16 : p->event_buffer.capacity * 2;
        parse_event_t *new_events = realloc(p->event_buffer.events, new_capacity * sizeof(parse_event_t));
        if (!new_events) {
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Out of memory for event buffer");
            return false;
        }
        p->event_buffer.events = new_events;
        p->event_buffer.capacity = new_capacity;
    }

    p->event_buffer.events[p->event_buffer.count++] = *event;
    return true;
}

static void buffer_clear(edn_parser_t *p) {
    p->event_buffer.count = 0;
    p->buffer_read_index = 0;
}

static void push_collection(edn_parser_t *p, collection_type_t type) {
    assert(p->stack_depth < MAX_NESTING_DEPTH);
    p->stack[p->stack_depth].type = type;
    p->stack[p->stack_depth].count = 0;
    p->stack[p->stack_depth].is_map_key = (type == COLL_MAP);
    p->stack_depth++;
}

static void pop_collection(edn_parser_t *p) {
    assert(p->stack_depth > 0);
    p->stack_depth--;
}

static collection_state_t* current_collection(edn_parser_t *p) {
    if (p->stack_depth == 0) {
        return NULL;
    }
    return &p->stack[p->stack_depth - 1];
}

// Count collection size by parsing until matching end token
// Returns -1 on error
static ssize_t count_collection_size(edn_parser_t *p, token_type_t start_token) {
    token_type_t end_token;
    switch (start_token) {
        case TOKEN_LBRACKET: end_token = TOKEN_RBRACKET; break;
        case TOKEN_LBRACE: end_token = TOKEN_RBRACE; break;
        case TOKEN_HASH_LBRACE: end_token = TOKEN_RBRACE; break;
        case TOKEN_LPAREN: end_token = TOKEN_RPAREN; break;
        default:
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Invalid collection start token");
            return -1;
    }

    size_t count = 0;
    int nesting = 1;  // We're already inside the collection

    while (nesting > 0) {
        token_t token;
        if (!next_token(p, &token)) {
            return -1;  // Error already set
        }

        if (token.type == TOKEN_EOF) {
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected EOF while counting collection");
            return -1;
        }

        // Handle nested collections
        if (token.type == TOKEN_LBRACKET || token.type == TOKEN_LBRACE ||
            token.type == TOKEN_HASH_LBRACE || token.type == TOKEN_LPAREN) {
            if (nesting == 1) {  // Top-level element (count before incrementing)
                count++;
            }
            nesting++;
        } else if (token.type == TOKEN_RBRACKET || token.type == TOKEN_RBRACE || token.type == TOKEN_RPAREN) {
            nesting--;
        } else {
            // Regular value
            if (nesting == 1) {  // Top-level element
                count++;
            }
        }
    }

    return (ssize_t)count;
}

// Buffer collection events until end marker
// Returns collection size (for maps: number of key-value pairs), or -1 on error
// This is used for non-seekable streams or small collections
static ssize_t buffer_collection_events(edn_parser_t *p, collection_type_t coll_type) {
    buffer_clear(p);

    // Set buffering mode to prevent nested collection size determination
    bool old_buffering_mode = p->buffering_mode;
    p->buffering_mode = true;

    size_t count = 0;  // Count of top-level elements
    int nesting = 1;   // We're already inside the collection

    while (nesting > 0) {
        // Parse next event recursively
        const parse_event_t *event = edn_parser_next_event(p);
        if (!event) {
            p->buffering_mode = old_buffering_mode;
            return -1;
        }

        if (event->type == EVENT_ERROR || event->type == EVENT_EOF) {
            p->buffering_mode = old_buffering_mode;
            return -1;
        }

        // Track nesting and count
        switch (event->type) {
            case EVENT_START_VECTOR:
            case EVENT_START_MAP:
            case EVENT_START_SET:
            case EVENT_START_LIST:
                if (nesting == 1) count++;  // Direct child collection
                nesting++;
                break;
            case EVENT_END_VECTOR:
            case EVENT_END_MAP:
            case EVENT_END_SET:
            case EVENT_END_LIST:
                nesting--;
                break;
            default:
                if (nesting == 1) count++;  // Direct child element
                break;
        }

        // Buffer this event (including END events)
        if (!buffer_add_event(p, event)) {
            p->buffering_mode = old_buffering_mode;
            return -1;
        }
    }

    p->buffering_mode = old_buffering_mode;
    p->buffer_read_index = 0;

    // Post-process buffer to fill in collection sizes for nested collections
    for (size_t i = 0; i < p->event_buffer.count; i++) {
        parse_event_t *event = &p->event_buffer.events[i];
        if (event->type == EVENT_START_VECTOR || event->type == EVENT_START_MAP ||
            event->type == EVENT_START_SET || event->type == EVENT_START_LIST) {
            // Count elements until matching END
            size_t nested_count = 0;
            int nesting = 1;
            for (size_t j = i + 1; j < p->event_buffer.count && nesting > 0; j++) {
                parse_event_t *inner = &p->event_buffer.events[j];
                switch (inner->type) {
                    case EVENT_START_VECTOR:
                    case EVENT_START_MAP:
                    case EVENT_START_SET:
                    case EVENT_START_LIST:
                        if (nesting == 1) nested_count++;
                        nesting++;
                        break;
                    case EVENT_END_VECTOR:
                    case EVENT_END_MAP:
                    case EVENT_END_SET:
                    case EVENT_END_LIST:
                        nesting--;
                        break;
                    default:
                        if (nesting == 1) nested_count++;
                        break;
                }
            }
            // For maps, convert to key-value pairs
            if (event->type == EVENT_START_MAP) {
                nested_count /= 2;
            }
            event->collection_size = nested_count;
        }
    }

    // Return raw element count (caller handles map conversion)
    return (ssize_t)count;
}

// Handle collection start with hybrid strategy
// Returns collection size, or -1 on error
// Sets up buffering or streaming mode as appropriate
static ssize_t handle_collection_start(edn_parser_t *p, token_type_t start_token, collection_type_t coll_type) {
    ssize_t size = -1;
    (void)start_token;  // Used for future streaming mode

    if (p->is_seekable && (size_t)p->buffer_threshold > 0) {
        // Seekable stream: use ftell/fseek approach for large collections
        long start_pos = ftell(p->input);
        int saved_char = p->current_char;
        bool saved_eof = p->eof_reached;

        if (start_pos == -1L) {
            // ftell failed, fall back to buffering
            goto use_buffering;
        }

        // Count collection size (first pass - no events generated)
        size = count_collection_size(p, start_token);
        if (size < 0) {
            return -1;  // Error already set
        }

        // Decision: buffer or stream based on size?
        if ((size_t)size < p->buffer_threshold) {
            // Small collection: use memory buffer (fast, 1-pass after fseek)
            // Seek back and buffer
            if (fseek(p->input, start_pos, SEEK_SET) != 0) {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "fseek failed");
                return -1;
            }
            // Restore tokenizer state
            p->current_char = saved_char;
            p->eof_reached = saved_eof;
            p->token_buffered = false;

            buffer_clear(p);
            ssize_t buffered_size = buffer_collection_events(p, coll_type);
            if (buffered_size < 0) {
                return -1;
            }
            p->buffer_read_index = 0;
            size = buffered_size;
        } else {
            // Large collection: use fseek approach (2-pass, memory efficient)
            // Seek back for streaming
            if (fseek(p->input, start_pos, SEEK_SET) != 0) {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "fseek failed");
                return -1;
            }
            // Restore tokenizer state
            p->current_char = saved_char;
            p->eof_reached = saved_eof;
            p->token_buffered = false;
            // Don't buffer - will stream directly
            // Size is already set from count_collection_size
        }
    } else {
use_buffering:
        // Non-seekable stream: must buffer (single pass)
        buffer_clear(p);
        size = buffer_collection_events(p, coll_type);
        if (size < 0) {
            return -1;
        }
        p->buffer_read_index = 0;
    }

    return size;
}

edn_parser_t* edn_parser_create_with_options(FILE *input, arena_t *arena, const edn_parser_options_t *options) {
    assert(input != NULL);
    assert(arena != NULL);

    edn_parser_t *p = malloc(sizeof(edn_parser_t));
    if (!p) {
        return NULL;
    }

    p->input = input;
    p->arena = arena;
    p->current_char = ' ';
    p->eof_reached = false;
    p->stack_depth = 0;
    p->token_buffered = false;
    p->error_buffer[0] = '\0';

    memset(&p->current_event, 0, sizeof(parse_event_t));
    memset(&p->current_token, 0, sizeof(token_t));

    // Set options
    if (options) {
        p->buffer_threshold = options->buffer_threshold;
    } else {
        p->buffer_threshold = 1000;  // Default threshold
    }

    // Check if stream is seekable
    long pos = ftell(input);
    p->is_seekable = (pos != -1L);

    // Initialize event buffer
    p->event_buffer.events = NULL;
    p->event_buffer.count = 0;
    p->event_buffer.capacity = 0;
    p->buffer_read_index = 0;
    p->buffering_mode = false;

    // Read first character
    consume_char(p);

    return p;
}

edn_parser_t* edn_parser_create(FILE *input, arena_t *arena) {
    return edn_parser_create_with_options(input, arena, NULL);
}

const parse_event_t* edn_parser_next_event(edn_parser_t *p) {
    assert(p != NULL);

    // If we're replaying buffered events (not during buffering), return next buffered event
    if (!p->buffering_mode && p->buffer_read_index < p->event_buffer.count) {
        p->current_event = p->event_buffer.events[p->buffer_read_index++];

        // Handle stack operations for replayed events
        switch (p->current_event.type) {
            case EVENT_START_VECTOR: push_collection(p, COLL_VECTOR); break;
            case EVENT_START_MAP: push_collection(p, COLL_MAP); break;
            case EVENT_START_SET: push_collection(p, COLL_SET); break;
            case EVENT_START_LIST: push_collection(p, COLL_LIST); break;
            case EVENT_END_VECTOR:
            case EVENT_END_MAP:
            case EVENT_END_SET:
            case EVENT_END_LIST:
                pop_collection(p);
                break;
            default:
                break;
        }

        return &p->current_event;
    }

    token_t token;
    if (!next_token(p, &token)) {
        p->current_event.type = EVENT_ERROR;
        p->current_event.error_message = p->error_buffer;
        return &p->current_event;
    }

    collection_state_t *coll = current_collection(p);

    switch (token.type) {
        case TOKEN_EOF:
            if (p->stack_depth > 0) {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected EOF, unclosed collection");
                p->current_event.type = EVENT_ERROR;
                p->current_event.error_message = p->error_buffer;
                return &p->current_event;
            }
            p->current_event.type = EVENT_EOF;
            return &p->current_event;

        case TOKEN_NIL:
            p->current_event.type = (coll && coll->type == COLL_MAP && coll->is_map_key) ? EVENT_KEY : EVENT_VALUE;
            p->current_event.value_type = VALUE_NIL;
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_TRUE:
        case TOKEN_FALSE:
            p->current_event.type = (coll && coll->type == COLL_MAP && coll->is_map_key) ? EVENT_KEY : EVENT_VALUE;
            p->current_event.value_type = VALUE_BOOL;
            p->current_event.value.bool_val = (token.type == TOKEN_TRUE);
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_INTEGER:
            p->current_event.type = (coll && coll->type == COLL_MAP && coll->is_map_key) ? EVENT_KEY : EVENT_VALUE;
            p->current_event.value_type = VALUE_INT64;
            p->current_event.value.int_val = token.int_value;
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_DOUBLE:
            p->current_event.type = (coll && coll->type == COLL_MAP && coll->is_map_key) ? EVENT_KEY : EVENT_VALUE;
            p->current_event.value_type = VALUE_DOUBLE;
            p->current_event.value.float_val = token.double_value;
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_STRING:
            p->current_event.type = (coll && coll->type == COLL_MAP && coll->is_map_key) ? EVENT_KEY : EVENT_VALUE;
            p->current_event.value_type = VALUE_STRING;
            p->current_event.value.string_val = token.str_value;
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_KEYWORD:
            p->current_event.type = (coll && coll->type == COLL_MAP && coll->is_map_key) ? EVENT_KEY : EVENT_VALUE;
            p->current_event.value_type = VALUE_KEYWORD;
            p->current_event.value.string_val = token.str_value;
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_LBRACKET:
            // Vector
            if (p->buffering_mode) {
                // During buffering, don't do size determination or stack operations
                p->current_event.type = EVENT_START_VECTOR;
                p->current_event.collection_size = 0;
                return &p->current_event;
            } else {
                // Use hybrid strategy to determine size
                ssize_t size = handle_collection_start(p, TOKEN_LBRACKET, COLL_VECTOR);
                if (size < 0) {
                    p->current_event.type = EVENT_ERROR;
                    return &p->current_event;
                }
                p->current_event.type = EVENT_START_VECTOR;
                p->current_event.collection_size = (size_t)size;
                push_collection(p, COLL_VECTOR);
                return &p->current_event;
            }

        case TOKEN_RBRACKET:
            if (p->buffering_mode) {
                p->current_event.type = EVENT_END_VECTOR;
                return &p->current_event;
            }
            if (!coll || coll->type != COLL_VECTOR) {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected ]");
                p->current_event.type = EVENT_ERROR;
                return &p->current_event;
            }
            p->current_event.type = EVENT_END_VECTOR;
            pop_collection(p);
            coll = current_collection(p);
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_LBRACE:
            // Map
            if (p->buffering_mode) {
                p->current_event.type = EVENT_START_MAP;
                p->current_event.collection_size = 0;
                return &p->current_event;
            } else {
                ssize_t size = handle_collection_start(p, TOKEN_LBRACE, COLL_MAP);
                if (size < 0) {
                    p->current_event.type = EVENT_ERROR;
                    return &p->current_event;
                }
                p->current_event.type = EVENT_START_MAP;
                // size is raw element count, convert to key-value pairs
                p->current_event.collection_size = (size_t)size / 2;
                push_collection(p, COLL_MAP);
                return &p->current_event;
            }

        case TOKEN_RBRACE:
            if (p->buffering_mode) {
                // Could be map or set end
                p->current_event.type = EVENT_END_MAP;  // Will be corrected by caller if needed
                return &p->current_event;
            }
            if (!coll || (coll->type != COLL_MAP && coll->type != COLL_SET)) {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected }");
                p->current_event.type = EVENT_ERROR;
                return &p->current_event;
            }
            p->current_event.type = (coll->type == COLL_MAP) ? EVENT_END_MAP : EVENT_END_SET;
            pop_collection(p);
            coll = current_collection(p);
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        case TOKEN_HASH_LBRACE:
            // Set
            if (p->buffering_mode) {
                p->current_event.type = EVENT_START_SET;
                p->current_event.collection_size = 0;
                return &p->current_event;
            } else {
                ssize_t size = handle_collection_start(p, TOKEN_HASH_LBRACE, COLL_SET);
                if (size < 0) {
                    p->current_event.type = EVENT_ERROR;
                    return &p->current_event;
                }
                p->current_event.type = EVENT_START_SET;
                p->current_event.collection_size = (size_t)size;
                push_collection(p, COLL_SET);
                return &p->current_event;
            }

        case TOKEN_LPAREN:
            // List
            if (p->buffering_mode) {
                p->current_event.type = EVENT_START_LIST;
                p->current_event.collection_size = 0;
                return &p->current_event;
            } else {
                ssize_t size = handle_collection_start(p, TOKEN_LPAREN, COLL_LIST);
                if (size < 0) {
                    p->current_event.type = EVENT_ERROR;
                    return &p->current_event;
                }
                p->current_event.type = EVENT_START_LIST;
                p->current_event.collection_size = (size_t)size;
                push_collection(p, COLL_LIST);
                return &p->current_event;
            }

        case TOKEN_RPAREN:
            if (p->buffering_mode) {
                p->current_event.type = EVENT_END_LIST;
                return &p->current_event;
            }
            if (!coll || coll->type != COLL_LIST) {
                snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected )");
                p->current_event.type = EVENT_ERROR;
                return &p->current_event;
            }
            p->current_event.type = EVENT_END_LIST;
            pop_collection(p);
            coll = current_collection(p);
            if (coll) {
                coll->count++;
                if (coll->type == COLL_MAP) {
                    coll->is_map_key = !coll->is_map_key;
                }
            }
            return &p->current_event;

        default:
            snprintf(p->error_buffer, sizeof(p->error_buffer), "Unexpected token type");
            p->current_event.type = EVENT_ERROR;
            return &p->current_event;
    }
}

const char* edn_parser_error(edn_parser_t *p) {
    assert(p != NULL);
    return p->error_buffer[0] ? p->error_buffer : NULL;
}

void edn_parser_destroy(edn_parser_t *p) {
    if (!p) {
        return;
    }
    // Free event buffer if allocated
    if (p->event_buffer.events) {
        free(p->event_buffer.events);
    }
    free(p);
}
