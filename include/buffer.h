#ifndef PELTIER_BUFFER_H
#define PELTIER_BUFFER_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct buffer {
    FILE *input;
    uint8_t *data;
    size_t size;
    size_t pos;
    size_t available;
    size_t total_read;
    bool eof_reached;
    bool owns_file;
} buffer_t;

#define BUFFER_READ_BYTE(buf) \
    ((buf)->pos < (buf)->available \
        ? ((buf)->total_read++, (int)(buf)->data[(buf)->pos++]) \
        : buffer_read_byte(buf))

// Create buffered reader
buffer_t* buffer_create(FILE *input, size_t buffer_size);

// Read single byte (returns -1 on EOF or error)
int buffer_read_byte(buffer_t *buf);

// Read N bytes into destination (returns number of bytes read)
size_t buffer_read_bytes(buffer_t *buf, void *dest, size_t n);

// Peek at next byte without consuming (-1 on EOF or error)
int buffer_peek_byte(buffer_t *buf);

// Read multi-byte integers (big-endian)
int16_t buffer_read_int16_be(buffer_t *buf);
int32_t buffer_read_int32_be(buffer_t *buf);
int64_t buffer_read_int64_be(buffer_t *buf);

// Read unsigned multi-byte integers (big-endian)
uint16_t buffer_read_uint16_be(buffer_t *buf);
uint32_t buffer_read_uint32_be(buffer_t *buf);

// Check if EOF reached
bool buffer_eof(buffer_t *buf);

// Get current position (for error reporting)
size_t buffer_position(buffer_t *buf);

// Cleanup
void buffer_destroy(buffer_t *buf);

#endif // PELTIER_BUFFER_H
