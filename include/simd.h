#ifndef PELTIER_SIMD_H
#define PELTIER_SIMD_H

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __SSE2__
#include <emmintrin.h>

static inline bool has_edn_escape(const char *str, size_t len) {
    const char *p = str;
    size_t remaining = len;

    __m128i v_dquote  = _mm_set1_epi8('"');
    __m128i v_bslash  = _mm_set1_epi8('\\');
    __m128i v_newline = _mm_set1_epi8('\n');
    __m128i v_cr      = _mm_set1_epi8('\r');
    __m128i v_tab     = _mm_set1_epi8('\t');

    while (remaining >= 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i *)p);
        __m128i r = _mm_cmpeq_epi8(chunk, v_dquote);
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_bslash));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_newline));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_cr));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_tab));
        if (_mm_movemask_epi8(r)) return true;
        p += 16;
        remaining -= 16;
    }

    for (size_t i = 0; i < remaining; i++) {
        char c = p[i];
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t')
            return true;
    }
    return false;
}

static inline const char* find_edn_escape(const char *str, size_t len) {
    const char *p = str;
    size_t remaining = len;

    __m128i v_dquote  = _mm_set1_epi8('"');
    __m128i v_bslash  = _mm_set1_epi8('\\');
    __m128i v_newline = _mm_set1_epi8('\n');
    __m128i v_cr      = _mm_set1_epi8('\r');
    __m128i v_tab     = _mm_set1_epi8('\t');

    while (remaining >= 16) {
        __m128i chunk = _mm_loadu_si128((const __m128i *)p);
        __m128i r = _mm_cmpeq_epi8(chunk, v_dquote);
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_bslash));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_newline));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_cr));
        r = _mm_or_si128(r, _mm_cmpeq_epi8(chunk, v_tab));
        int mask = _mm_movemask_epi8(r);
        if (mask) return p + __builtin_ctz(mask);
        p += 16;
        remaining -= 16;
    }

    for (size_t i = 0; i < remaining; i++) {
        char c = p[i];
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c == '\t')
            return p + i;
    }
    return NULL;
}

#else

static inline bool has_edn_escape(const char *str, size_t len) {
    return memchr(str, '"',  len) != NULL ||
           memchr(str, '\\', len) != NULL ||
           memchr(str, '\n', len) != NULL ||
           memchr(str, '\r', len) != NULL ||
           memchr(str, '\t', len) != NULL;
}

static inline const char* find_edn_escape(const char *str, size_t len) {
    const char *best = NULL;
    const char *p;

#define UPDATE(c) \
    p = memchr(str, (c), len); \
    if (p && (!best || p < best)) best = p;

    UPDATE('"')
    UPDATE('\\')
    UPDATE('\n')
    UPDATE('\r')
    UPDATE('\t')
#undef UPDATE

    return best;
}

#endif

#endif // PELTIER_SIMD_H
