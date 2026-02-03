#include "Unity/src/unity.h"
#include "../src/edn/edn_parser.h"
#include "../src/edn/edn_writer.h"
#include "../src/nippy/nippy_parser.h"
#include "../src/nippy/nippy_writer.h"
#include "../include/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMP_NIPPY_PATH "/tmp/peltier_test.nippy"
#define TMP_EDN_PATH "/tmp/peltier_test_out.edn"

void setUp(void) {}
void tearDown(void) {}

// Helper: Read file contents into string
static char* read_file_contents(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    // Trim trailing whitespace
    while (size > 0 && (buf[size-1] == '\n' || buf[size-1] == '\r' || buf[size-1] == ' ')) {
        buf[--size] = '\0';
    }

    return buf;
}

// Helper: Run round-trip test for a single EDN file
static void test_roundtrip_file(const char *edn_path) {
    arena_t *arena = arena_create(4096);
    TEST_ASSERT_NOT_NULL(arena);

    // Step 1: Parse EDN and write to Nippy
    FILE *edn_in = fopen(edn_path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(edn_in, "Failed to open input EDN file");

    FILE *nippy_out = fopen(TMP_NIPPY_PATH, "wb");
    TEST_ASSERT_NOT_NULL_MESSAGE(nippy_out, "Failed to create temp Nippy file");

    edn_parser_t *edn_parser = edn_parser_create(edn_in, arena);
    TEST_ASSERT_NOT_NULL(edn_parser);

    nippy_writer_t *nippy_writer = nippy_writer_create(nippy_out);
    TEST_ASSERT_NOT_NULL(nippy_writer);

    const parse_event_t *event;
    while ((event = edn_parser_next_event(edn_parser)) != NULL) {
        if (event->type == EVENT_EOF) break;
        if (event->type == EVENT_ERROR) {
            TEST_FAIL_MESSAGE(event->error_message ? event->error_message : "EDN parse error");
        }
        if (!nippy_writer_write_event(nippy_writer, event)) {
            TEST_FAIL_MESSAGE(nippy_writer_error(nippy_writer));
        }
    }

    nippy_writer_destroy(nippy_writer);
    edn_parser_destroy(edn_parser);
    fclose(nippy_out);
    fclose(edn_in);

    // Step 2: Parse Nippy and write back to EDN
    FILE *nippy_in = fopen(TMP_NIPPY_PATH, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(nippy_in, "Failed to open temp Nippy file");

    FILE *edn_out = fopen(TMP_EDN_PATH, "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(edn_out, "Failed to create output EDN file");

    nippy_parser_t *nippy_parser = nippy_parser_create(nippy_in, arena);
    TEST_ASSERT_NOT_NULL(nippy_parser);

    edn_writer_t *edn_writer = edn_writer_create(edn_out, false, 0);
    TEST_ASSERT_NOT_NULL(edn_writer);

    while ((event = nippy_parser_next_event(nippy_parser)) != NULL) {
        if (event->type == EVENT_EOF) break;
        if (event->type == EVENT_ERROR) {
            TEST_FAIL_MESSAGE(event->error_message ? event->error_message : "Nippy parse error");
        }
        edn_writer_write_event(edn_writer, event);
    }

    edn_writer_destroy(edn_writer);
    nippy_parser_destroy(nippy_parser);
    fclose(edn_out);
    fclose(nippy_in);

    // Step 3: Compare original EDN with output EDN
    char *original = read_file_contents(edn_path);
    char *result = read_file_contents(TMP_EDN_PATH);

    TEST_ASSERT_NOT_NULL_MESSAGE(original, "Failed to read original EDN");
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Failed to read result EDN");
    TEST_ASSERT_EQUAL_STRING(original, result);

    free(original);
    free(result);
    arena_destroy(arena);
}

// Individual test cases for each primitive type
void test_nil(void) {
    test_roundtrip_file("test-resources/primitive-edns/nil.edn");
}

void test_true(void) {
    test_roundtrip_file("test-resources/primitive-edns/true.edn");
}

void test_false(void) {
    test_roundtrip_file("test-resources/primitive-edns/false.edn");
}

void test_integer(void) {
    test_roundtrip_file("test-resources/primitive-edns/integer.edn");
}

void test_integer_negative(void) {
    test_roundtrip_file("test-resources/primitive-edns/integer-negative.edn");
}

void test_integer_zero(void) {
    test_roundtrip_file("test-resources/primitive-edns/integer-zero.edn");
}

void test_double(void) {
    test_roundtrip_file("test-resources/primitive-edns/double.edn");
}

void test_double_negative(void) {
    test_roundtrip_file("test-resources/primitive-edns/double-negative.edn");
}

void test_double_scientific(void) {
    test_roundtrip_file("test-resources/primitive-edns/double-scientific.edn");
}

void test_double_scientific_pos(void) {
    test_roundtrip_file("test-resources/primitive-edns/double-scientific-pos.edn");
}

void test_string(void) {
    test_roundtrip_file("test-resources/primitive-edns/string.edn");
}

void test_string_empty(void) {
    test_roundtrip_file("test-resources/primitive-edns/string-empty.edn");
}

void test_string_escape(void) {
    test_roundtrip_file("test-resources/primitive-edns/string-escape.edn");
}

void test_keyword(void) {
    test_roundtrip_file("test-resources/primitive-edns/keyword.edn");
}

void test_keyword_namespaced(void) {
    test_roundtrip_file("test-resources/primitive-edns/keyword-namespaced.edn");
}

void test_symbol(void) {
    test_roundtrip_file("test-resources/primitive-edns/symbol.edn");
}

void test_symbol_namespaced(void) {
    test_roundtrip_file("test-resources/primitive-edns/symbol-namespaced.edn");
}

void test_vector(void) {
    test_roundtrip_file("test-resources/primitive-edns/vector.edn");
}

void test_vector_empty(void) {
    test_roundtrip_file("test-resources/primitive-edns/vector-empty.edn");
}

void test_list(void) {
    test_roundtrip_file("test-resources/primitive-edns/list.edn");
}

void test_list_empty(void) {
    test_roundtrip_file("test-resources/primitive-edns/list-empty.edn");
}

void test_map(void) {
    test_roundtrip_file("test-resources/primitive-edns/map.edn");
}

void test_map_empty(void) {
    test_roundtrip_file("test-resources/primitive-edns/map-empty.edn");
}

void test_set(void) {
    test_roundtrip_file("test-resources/primitive-edns/set.edn");
}

void test_set_empty(void) {
    test_roundtrip_file("test-resources/primitive-edns/set-empty.edn");
}

int main(void) {
    UNITY_BEGIN();

    // Primitives
    RUN_TEST(test_nil);
    RUN_TEST(test_true);
    RUN_TEST(test_false);

    // Numbers
    RUN_TEST(test_integer);
    RUN_TEST(test_integer_negative);
    RUN_TEST(test_integer_zero);
    RUN_TEST(test_double);
    RUN_TEST(test_double_negative);
    RUN_TEST(test_double_scientific);
    RUN_TEST(test_double_scientific_pos);

    // Strings
    RUN_TEST(test_string);
    RUN_TEST(test_string_empty);
    RUN_TEST(test_string_escape);

    // Keywords
    RUN_TEST(test_keyword);
    RUN_TEST(test_keyword_namespaced);

    // Symbols
    RUN_TEST(test_symbol);
    RUN_TEST(test_symbol_namespaced);

    // Collections
    RUN_TEST(test_vector);
    RUN_TEST(test_vector_empty);
    RUN_TEST(test_list);
    RUN_TEST(test_list_empty);
    RUN_TEST(test_map);
    RUN_TEST(test_map_empty);
    RUN_TEST(test_set);
    RUN_TEST(test_set_empty);

    return UNITY_END();
}
