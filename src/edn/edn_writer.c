#include "edn_writer.h"
#include "../../include/utils.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WRITE_BUFFER_SIZE 8192

typedef enum {
    COLLECTION_VECTOR,
    COLLECTION_MAP,
    COLLECTION_SET,
    COLLECTION_LIST
} collection_type_t;

typedef struct {
    collection_type_t type;
    bool first_element;
    bool expecting_value;  // For maps: true after key, false after value
} collection_state_t;

struct edn_writer {
    FILE *output;
    char buffer[WRITE_BUFFER_SIZE];
    size_t buffer_pos;

    bool pretty_print;
    int indent_size;
    int indent_level;

    // Stack to track collection nesting
    collection_state_t collection_stack[MAX_NESTING_DEPTH];
    size_t collection_depth;
};

static void write_char(edn_writer_t *w, char c) {
    if (w->buffer_pos >= WRITE_BUFFER_SIZE) {
        fwrite(w->buffer, 1, w->buffer_pos, w->output);
        w->buffer_pos = 0;
    }

    w->buffer[w->buffer_pos++] = c;
}

static void write_string(edn_writer_t *w, const char *s) {
    while (*s) {
        write_char(w, *s++);
    }
}

static void write_indent(edn_writer_t *w) {
    if (!w->pretty_print) {
        return;
    }

    write_char(w, '\n');
    int spaces = w->indent_level * w->indent_size;
    for (int i = 0; i < spaces; i++) {
        write_char(w, ' ');
    }
}

static void write_separator(edn_writer_t *w) {
    if (w->collection_depth > 0) {
        collection_state_t *state = &w->collection_stack[w->collection_depth - 1];

        if (!state->first_element) {
            if (state->type == COLLECTION_MAP && !state->expecting_value) {
                write_char(w, ',');
            }

            // Always write space between elements (EDN requires it)
            write_char(w, ' ');
        }

        state->first_element = false;
    }
}

edn_writer_t* edn_writer_create(FILE *output, bool pretty_print, int indent_size) {
    assert(output != NULL);

    edn_writer_t *w = malloc(sizeof(edn_writer_t));
    if (!w) {
        return NULL;
    }

    w->output = output;
    w->buffer_pos = 0;
    w->pretty_print = pretty_print;
    w->indent_size = indent_size > 0 ? indent_size : 2;
    w->indent_level = 0;
    w->collection_depth = 0;

    return w;
}

void edn_writer_write_event(edn_writer_t *w, const parse_event_t *event) {
    assert(w != NULL);
    assert(event != NULL);

    switch (event->type) {
        case EVENT_START_VECTOR:
            write_separator(w);
            write_char(w, '[');
            if (w->collection_depth < MAX_NESTING_DEPTH) {
                w->collection_stack[w->collection_depth].type = COLLECTION_VECTOR;
                w->collection_stack[w->collection_depth].first_element = true;
                w->collection_stack[w->collection_depth].expecting_value = false;
                w->collection_depth++;
            }
            w->indent_level++;
            break;

        case EVENT_END_VECTOR:
            w->indent_level--;
            if (w->collection_depth > 0) {
                w->collection_depth--;
            }
            write_char(w, ']');
            break;

        case EVENT_START_SET:
            write_separator(w);
            write_string(w, "#{");
            if (w->collection_depth < MAX_NESTING_DEPTH) {
                w->collection_stack[w->collection_depth].type = COLLECTION_SET;
                w->collection_stack[w->collection_depth].first_element = true;
                w->collection_stack[w->collection_depth].expecting_value = false;
                w->collection_depth++;
            }
            w->indent_level++;
            break;

        case EVENT_END_SET:
            w->indent_level--;
            if (w->collection_depth > 0) {
                w->collection_depth--;
            }
            write_char(w, '}');
            break;

        case EVENT_START_MAP:
            write_separator(w);
            write_char(w, '{');
            if (w->collection_depth < MAX_NESTING_DEPTH) {
                w->collection_stack[w->collection_depth].type = COLLECTION_MAP;
                w->collection_stack[w->collection_depth].first_element = true;
                w->collection_stack[w->collection_depth].expecting_value = false;
                w->collection_depth++;
            }
            w->indent_level++;
            break;

        case EVENT_END_MAP:
            w->indent_level--;
            if (w->collection_depth > 0) {
                w->collection_depth--;
            }
            write_char(w, '}');
            break;

        case EVENT_START_LIST:
            write_separator(w);
            write_char(w, '(');
            if (w->collection_depth < MAX_NESTING_DEPTH) {
                w->collection_stack[w->collection_depth].type = COLLECTION_LIST;
                w->collection_stack[w->collection_depth].first_element = true;
                w->collection_stack[w->collection_depth].expecting_value = false;
                w->collection_depth++;
            }
            w->indent_level++;
            break;

        case EVENT_END_LIST:
            w->indent_level--;
            if (w->collection_depth > 0) {
                w->collection_depth--;
            }
            write_char(w, ')');
            break;

        case EVENT_KEY:
        case EVENT_VALUE: {
            // For maps, handle key/value separator
            if (w->collection_depth > 0) {
                collection_state_t *state = &w->collection_stack[w->collection_depth - 1];

                if (state->type == COLLECTION_MAP) {
                    if (state->expecting_value) {
                        write_char(w, ' ');
                        state->expecting_value = false;
                    } else {
                        write_separator(w);
                        state->expecting_value = true;
                    }
                } else {
                    write_separator(w);
                }
            }

            // Write the value based on type
            switch (event->value_type) {
                case VALUE_NIL:
                    write_string(w, "nil");
                    break;

                case VALUE_BOOL:
                    write_string(w, event->value.bool_val ? "true" : "false");
                    break;

                case VALUE_INT64: {
                    char num_buf[32];
                    snprintf(num_buf, sizeof(num_buf), "%ld", event->value.int_val);
                    write_string(w, num_buf);
                    break;
                }

                case VALUE_DOUBLE: {
                    char num_buf[64];
                    snprintf(num_buf, sizeof(num_buf), "%.15g", event->value.float_val);
                    write_string(w, num_buf);
                    break;
                }

                case VALUE_STRING: {
                    if (event->value.string_val) {
                        char *escaped = escape_edn_string(event->value.string_val,
                                                          strlen(event->value.string_val));
                        if (escaped) {
                            write_string(w, escaped);
                            free(escaped);
                        }
                    } else {
                        write_string(w, "\"\"");
                    }
                    break;
                }

                case VALUE_KEYWORD:
                    write_char(w, ':');
                    if (event->value.string_val) {
                        write_string(w, event->value.string_val);
                    }
                    break;

                case VALUE_SYMBOL:
                    if (event->value.string_val) {
                        write_string(w, event->value.string_val);
                    }
                    break;

                case VALUE_CHAR:
                    write_char(w, '\\');
                    write_char(w, event->value.int_val);
                    break;
            }
            break;
        }

        case EVENT_ERROR:
        case EVENT_EOF:
            // Nothing to write
            break;
    }
}

void edn_writer_flush(edn_writer_t *w) {
    assert(w != NULL);

    if (w->buffer_pos > 0) {
        fwrite(w->buffer, 1, w->buffer_pos, w->output);
        w->buffer_pos = 0;
    }

    fflush(w->output);
}

void edn_writer_destroy(edn_writer_t *w) {
    if (!w) {
        return;
    }

    edn_writer_flush(w);
    free(w);
}
