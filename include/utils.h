#ifndef PELTIER_UTILS_H
#define PELTIER_UTILS_H

#include <stddef.h>
#include <stdbool.h>

// String builder for dynamic string construction
typedef struct string_builder string_builder_t;

// Create a string builder
string_builder_t* sb_create(size_t initial_capacity);

// Append string to builder
void sb_append(string_builder_t *sb, const char *str);

// Append character to builder
void sb_append_char(string_builder_t *sb, char c);

// Append formatted string
void sb_append_format(string_builder_t *sb, const char *fmt, ...);

// Get the final string (does not free builder)
const char* sb_get(string_builder_t *sb);

// Get string length
size_t sb_length(string_builder_t *sb);

// Clear contents
void sb_clear(string_builder_t *sb);

// Free string builder
void sb_destroy(string_builder_t *sb);

// String escape/unescape utilities
char* escape_edn_string(const char *str, size_t len);
bool is_valid_keyword_char(char c);

#endif // PELTIER_UTILS_H
