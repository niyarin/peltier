#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>

struct string_builder {
    char *data;
    size_t length;
    size_t capacity;
};

string_builder_t* sb_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 256;
    }

    string_builder_t *sb = malloc(sizeof(string_builder_t));
    if (!sb) {
        return NULL;
    }

    sb->data = malloc(initial_capacity);
    if (!sb->data) {
        free(sb);
        return NULL;
    }

    sb->data[0] = '\0';
    sb->length = 0;
    sb->capacity = initial_capacity;

    return sb;
}

static void sb_ensure_capacity(string_builder_t *sb, size_t needed) {
    if (sb->length + needed + 1 <= sb->capacity) {
        return;
    }

    size_t new_capacity = sb->capacity * 2;
    while (new_capacity < sb->length + needed + 1) {
        new_capacity *= 2;
    }

    char *new_data = realloc(sb->data, new_capacity);
    if (!new_data) {
        return;  // Keep old buffer on allocation failure
    }

    sb->data = new_data;
    sb->capacity = new_capacity;
}

void sb_append(string_builder_t *sb, const char *str) {
    assert(sb != NULL);

    if (!str) {
        return;
    }

    size_t len = strlen(str);
    sb_ensure_capacity(sb, len);

    memcpy(sb->data + sb->length, str, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

void sb_append_char(string_builder_t *sb, char c) {
    assert(sb != NULL);

    sb_ensure_capacity(sb, 1);

    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

void sb_append_format(string_builder_t *sb, const char *fmt, ...) {
    assert(sb != NULL);
    assert(fmt != NULL);

    va_list args;
    va_start(args, fmt);

    // Calculate required size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return;
    }

    sb_ensure_capacity(sb, needed);

    vsnprintf(sb->data + sb->length, needed + 1, fmt, args);
    sb->length += needed;

    va_end(args);
}

const char* sb_get(string_builder_t *sb) {
    assert(sb != NULL);

    return sb->data;
}

size_t sb_length(string_builder_t *sb) {
    assert(sb != NULL);

    return sb->length;
}

void sb_clear(string_builder_t *sb) {
    assert(sb != NULL);

    sb->length = 0;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

void sb_destroy(string_builder_t *sb) {
    if (!sb) {
        return;
    }

    free(sb->data);
    free(sb);
}

// EDN string escaping
char* escape_edn_string(const char *str, size_t len) {
    if (!str) {
        return NULL;
    }

    // Allocate worst case (every char needs escaping)
    char *escaped = malloc(len * 2 + 3);  // quotes + null terminator
    if (!escaped) {
        return NULL;
    }

    size_t j = 0;
    escaped[j++] = '"';

    for (size_t i = 0; i < len; i++) {
        char c = str[i];

        switch (c) {
            case '"':
                escaped[j++] = '\\';
                escaped[j++] = '"';
                break;
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            default:
                escaped[j++] = c;
                break;
        }
    }

    escaped[j++] = '"';
    escaped[j] = '\0';

    return escaped;
}

bool is_valid_keyword_char(char c) {
    return isalnum(c) || c == '-' || c == '_' || c == '.' ||
           c == '*' || c == '+' || c == '!' || c == '?' ||
           c == '$' || c == '%' || c == '&' || c == '=' ||
           c == '<' || c == '>' || c == '/';
}
