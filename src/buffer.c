#include "buffer.h"
#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_BUFFER_SIZE 8192


// Refill buffer from input stream
static bool buffer_refill(buffer_t *buf) {
    if (buf->eof_reached) {
        return false;
    }

    size_t read_count = fread(buf->data, 1, buf->size, buf->input);
    buf->pos = 0;
    buf->available = read_count;

    if (read_count == 0) {
        buf->eof_reached = true;
        return false;
    }

    return true;
}

buffer_t* buffer_create(FILE *input, size_t buffer_size) {
    assert(input != NULL);

    if (buffer_size == 0) {
        buffer_size = DEFAULT_BUFFER_SIZE;
    }

    buffer_t *buf = malloc(sizeof(buffer_t));
    if (!buf) {
        return NULL;
    }

    buf->data = malloc(buffer_size);
    if (!buf->data) {
        free(buf);
        return NULL;
    }

    buf->input = input;
    buf->size = buffer_size;
    buf->pos = 0;
    buf->available = 0;
    buf->total_read = 0;
    buf->eof_reached = false;
    buf->owns_file = false;

    return buf;
}

int buffer_read_byte(buffer_t *buf) {
    assert(buf != NULL);

    // Refill if needed
    if (buf->pos >= buf->available) {
        if (!buffer_refill(buf)) {
            return -1;
        }
    }

    int byte = buf->data[buf->pos++];
    buf->total_read++;

    return byte;
}

size_t buffer_read_bytes(buffer_t *buf, void *dest, size_t n) {
    assert(buf != NULL);
    assert(dest != NULL || n == 0);

    size_t total = 0;
    uint8_t *out = (uint8_t *)dest;

    while (total < n) {
        // Refill if needed
        if (buf->pos >= buf->available) {
            if (!buffer_refill(buf)) {
                break;
            }
        }

        // Copy available bytes
        size_t remaining = n - total;
        size_t available = buf->available - buf->pos;
        size_t to_copy = (remaining < available) ? remaining : available;

        memcpy(&out[total], &buf->data[buf->pos], to_copy);
        buf->pos += to_copy;
        total += to_copy;
        buf->total_read += to_copy;
    }

    return total;
}

int buffer_peek_byte(buffer_t *buf) {
    assert(buf != NULL);

    // Refill if needed
    if (buf->pos >= buf->available) {
        if (!buffer_refill(buf)) {
            return -1;
        }
    }

    return buf->data[buf->pos];
}

int16_t buffer_read_int16_be(buffer_t *buf) {
    if (buf->available - buf->pos >= 2) {
        uint8_t *p = &buf->data[buf->pos];
        buf->pos += 2;
        buf->total_read += 2;
        return (int16_t)((p[0] << 8) | p[1]);
    }
    uint8_t bytes[2];
    if (buffer_read_bytes(buf, bytes, 2) != 2) {
        return 0;
    }
    return (int16_t)((bytes[0] << 8) | bytes[1]);
}

int32_t buffer_read_int32_be(buffer_t *buf) {
    if (buf->available - buf->pos >= 4) {
        uint8_t *p = &buf->data[buf->pos];
        buf->pos += 4;
        buf->total_read += 4;
        return (int32_t)((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
    }
    uint8_t bytes[4];
    if (buffer_read_bytes(buf, bytes, 4) != 4) {
        return 0;
    }
    return (int32_t)((bytes[0] << 24) | (bytes[1] << 16) |
                     (bytes[2] << 8) | bytes[3]);
}

int64_t buffer_read_int64_be(buffer_t *buf) {
    if (buf->available - buf->pos >= 8) {
        uint8_t *p = &buf->data[buf->pos];
        buf->pos += 8;
        buf->total_read += 8;
        return ((int64_t)p[0] << 56) | ((int64_t)p[1] << 48) |
               ((int64_t)p[2] << 40) | ((int64_t)p[3] << 32) |
               ((int64_t)p[4] << 24) | ((int64_t)p[5] << 16) |
               ((int64_t)p[6] << 8) | (int64_t)p[7];
    }
    uint8_t bytes[8];
    if (buffer_read_bytes(buf, bytes, 8) != 8) {
        return 0;
    }
    return ((int64_t)bytes[0] << 56) | ((int64_t)bytes[1] << 48) |
           ((int64_t)bytes[2] << 40) | ((int64_t)bytes[3] << 32) |
           ((int64_t)bytes[4] << 24) | ((int64_t)bytes[5] << 16) |
           ((int64_t)bytes[6] << 8) | (int64_t)bytes[7];
}

uint16_t buffer_read_uint16_be(buffer_t *buf) {
    if (buf->available - buf->pos >= 2) {
        uint8_t *p = &buf->data[buf->pos];
        buf->pos += 2;
        buf->total_read += 2;
        return (uint16_t)((p[0] << 8) | p[1]);
    }
    uint8_t bytes[2];
    if (buffer_read_bytes(buf, bytes, 2) != 2) {
        return 0;
    }
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

uint32_t buffer_read_uint32_be(buffer_t *buf) {
    if (buf->available - buf->pos >= 4) {
        uint8_t *p = &buf->data[buf->pos];
        buf->pos += 4;
        buf->total_read += 4;
        return (uint32_t)((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
    }
    uint8_t bytes[4];
    if (buffer_read_bytes(buf, bytes, 4) != 4) {
        return 0;
    }
    return (uint32_t)((bytes[0] << 24) | (bytes[1] << 16) |
                      (bytes[2] << 8) | bytes[3]);
}

bool buffer_eof(buffer_t *buf) {
    assert(buf != NULL);

    return buf->eof_reached && buf->pos >= buf->available;
}

size_t buffer_position(buffer_t *buf) {
    assert(buf != NULL);

    return buf->total_read;
}

void buffer_destroy(buffer_t *buf) {
    if (!buf) {
        return;
    }

    if (buf->owns_file && buf->input) {
        fclose(buf->input);
    }

    free(buf->data);
    free(buf);
}
