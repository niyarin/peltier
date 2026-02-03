#include "Unity/src/unity.h"
#include "../src/edn/edn_parser.h"
#include "../src/edn/edn_writer.h"
#include "../src/nippy/nippy_parser.h"
#include "../src/nippy/nippy_writer.h"
#include "../include/arena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMP_NIPPY_PATH "/tmp/peltier_regression_test.nippy"
#define TMP_EDN_PATH "/tmp/peltier_regression_test.edn"

void setUp(void) {}
void tearDown(void) {}

// Helper: Freeze EDN to nippy using peltier
static int peltier_freeze(const char *edn_path, const char *nippy_path) {
    arena_t *arena = arena_create(4096);
    if (!arena) return -1;

    FILE *edn_in = fopen(edn_path, "r");
    if (!edn_in) {
        arena_destroy(arena);
        return -1;
    }

    FILE *nippy_out = fopen(nippy_path, "wb");
    if (!nippy_out) {
        fclose(edn_in);
        arena_destroy(arena);
        return -1;
    }

    edn_parser_t *parser = edn_parser_create(edn_in, arena);
    nippy_writer_t *writer = nippy_writer_create(nippy_out);

    if (!parser || !writer) {
        if (parser) edn_parser_destroy(parser);
        if (writer) nippy_writer_destroy(writer);
        fclose(edn_in);
        fclose(nippy_out);
        arena_destroy(arena);
        return -1;
    }

    int result = 0;
    const parse_event_t *event;
    while ((event = edn_parser_next_event(parser)) != NULL) {
        if (event->type == EVENT_EOF) break;
        if (event->type == EVENT_ERROR) {
            result = -1;
            break;
        }
        if (!nippy_writer_write_event(writer, event)) {
            result = -1;
            break;
        }
    }

    nippy_writer_destroy(writer);
    edn_parser_destroy(parser);
    fclose(edn_in);
    fclose(nippy_out);
    arena_destroy(arena);
    return result;
}

// Helper: Thaw nippy to EDN using peltier
static int peltier_thaw(const char *nippy_path, const char *edn_path) {
    arena_t *arena = arena_create(4096);
    if (!arena) return -1;

    FILE *nippy_in = fopen(nippy_path, "rb");
    if (!nippy_in) {
        arena_destroy(arena);
        return -1;
    }

    FILE *edn_out = fopen(edn_path, "w");
    if (!edn_out) {
        fclose(nippy_in);
        arena_destroy(arena);
        return -1;
    }

    nippy_parser_t *parser = nippy_parser_create(nippy_in, arena);
    edn_writer_t *writer = edn_writer_create(edn_out, false, 0);

    if (!parser || !writer) {
        if (parser) nippy_parser_destroy(parser);
        if (writer) edn_writer_destroy(writer);
        fclose(nippy_in);
        fclose(edn_out);
        arena_destroy(arena);
        return -1;
    }

    int result = 0;
    const parse_event_t *event;
    while ((event = nippy_parser_next_event(parser)) != NULL) {
        if (event->type == EVENT_EOF) break;
        if (event->type == EVENT_ERROR) {
            result = -1;
            break;
        }
        edn_writer_write_event(writer, event);
    }

    edn_writer_destroy(writer);
    nippy_parser_destroy(parser);
    fclose(nippy_in);
    fclose(edn_out);
    arena_destroy(arena);
    return result;
}

// Test: peltier freeze → thaw round-trip produces valid output
static void test_freeze_roundtrip(const char *name) {
    char edn_path[256];
    snprintf(edn_path, sizeof(edn_path), "test-resources/edns/%s.edn", name);

    // Freeze with peltier
    int freeze_result = peltier_freeze(edn_path, TMP_NIPPY_PATH);
    TEST_ASSERT_EQUAL_MESSAGE(0, freeze_result, "peltier freeze failed");

    // Thaw back with peltier
    int thaw_result = peltier_thaw(TMP_NIPPY_PATH, TMP_EDN_PATH);
    TEST_ASSERT_EQUAL_MESSAGE(0, thaw_result, "peltier thaw of own output failed");
}

// Test: peltier can thaw Clojure-generated nippy
static void test_thaw_compatibility(const char *name) {
    char clj_nippy_path[256];
    snprintf(clj_nippy_path, sizeof(clj_nippy_path), "test-resources/nippies/%s.nippy", name);

    // Thaw with peltier
    int thaw_result = peltier_thaw(clj_nippy_path, TMP_EDN_PATH);
    TEST_ASSERT_EQUAL_MESSAGE(0, thaw_result, "peltier thaw failed");
}

//=============================================================================
// Regression test: Scientific notation (Issue: E-4 parsed as two tokens)
//=============================================================================
void test_regression_scientific_notation_roundtrip(void) {
    test_freeze_roundtrip("regression-scientific-notation");
}

void test_regression_scientific_notation_thaw(void) {
    test_thaw_compatibility("regression-scientific-notation");
}

//=============================================================================
// Regression test: Signed integers (Issue: v3.3.0 uses 0x64 signed encoding)
//=============================================================================
void test_regression_signed_integers_roundtrip(void) {
    test_freeze_roundtrip("regression-signed-integers");
}

void test_regression_signed_integers_thaw(void) {
    test_thaw_compatibility("regression-signed-integers");
}

//=============================================================================
// Regression test: v3.3.0 type codes (Issue: 0x69/0x6E/0x6F/0x70 vs old codes)
//=============================================================================
void test_regression_v330_types_roundtrip(void) {
    test_freeze_roundtrip("regression-v330-types");
}

void test_regression_v330_types_thaw(void) {
    test_thaw_compatibility("regression-v330-types");
}

//=============================================================================
// Main
//=============================================================================
int main(void) {
    UNITY_BEGIN();

    // Scientific notation regression (Issue: 1.5E-4 was parsed as two tokens)
    RUN_TEST(test_regression_scientific_notation_roundtrip);
    RUN_TEST(test_regression_scientific_notation_thaw);

    // Signed integer encoding regression (Issue: v3.3.0 uses 0x64/0x65/0x66)
    RUN_TEST(test_regression_signed_integers_roundtrip);
    RUN_TEST(test_regression_signed_integers_thaw);

    // v3.3.0 type codes regression (Issue: 0x69/0x6E/0x6F/0x70 for str/vec/set/map)
    RUN_TEST(test_regression_v330_types_roundtrip);
    RUN_TEST(test_regression_v330_types_thaw);

    return UNITY_END();
}
