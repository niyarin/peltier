CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS =

# LZ4 compression support (disabled by default)
# To enable: make USE_LZ4=1
USE_LZ4 ?= 0

ifeq ($(USE_LZ4),1)
    CFLAGS += -DUSE_LZ4 -D_POSIX_C_SOURCE=200809L
    LDFLAGS += -llz4
endif

SRC = src/cli.c \
      src/nippy/nippy_parser.c \
      src/nippy/nippy_writer.c \
      src/edn/edn_parser.c \
      src/edn/edn_writer.c \
      src/selector.c \
      src/arena.c \
      src/buffer.c \
      src/utils.c

OBJ = $(SRC:.c=.o)

TARGET = peltier

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)
	rm -f test/test_roundtrip test/test_regression

TEST_SRC = src/nippy/nippy_parser.c \
           src/nippy/nippy_writer.c \
           src/edn/edn_parser.c \
           src/edn/edn_writer.c \
           src/arena.c \
           src/buffer.c \
           src/utils.c

UNITY_SRC = test/Unity/src/unity.c

test/test_roundtrip: test/test_roundtrip.c $(UNITY_SRC) $(TEST_SRC)
	$(CC) -std=c11 -Wall -Iinclude -I. -o $@ $^

test/test_regression: test/test_regression.c $(UNITY_SRC) $(TEST_SRC)
	$(CC) -std=c11 -Wall -Iinclude -I. -o $@ $^

# Run all tests
test: test/test_roundtrip test/test_regression
	@echo "=== Running roundtrip tests ==="
	./test/test_roundtrip
	@echo ""
	@echo "=== Running regression tests ==="
	./test/test_regression

# Run only roundtrip tests
test-roundtrip: test/test_roundtrip
	./test/test_roundtrip

# Run only regression tests
test-regression: test/test_regression
	./test/test_regression

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

# Coverage
GCC_VERSION = $(shell $(CC) -dumpversion | cut -d. -f1)
GCOV = gcov-$(GCC_VERSION)
GCOV_TOOL = gcov-tool-$(GCC_VERSION)
COVFLAGS = -std=c11 -Wall -Iinclude -I. -fprofile-arcs -ftest-coverage -O0
COV_RT_DIR = test/cov-rt
COV_RG_DIR = test/cov-rg
COV_MERGED_DIR = test/cov-merged

test/test_roundtrip_cov: test/test_roundtrip.c $(UNITY_SRC) $(TEST_SRC)
	$(CC) $(COVFLAGS) -o $@ $^

test/test_regression_cov: test/test_regression.c $(UNITY_SRC) $(TEST_SRC)
	$(CC) $(COVFLAGS) -o $@ $^

coverage: test/test_roundtrip_cov test/test_regression_cov
	./test/test_roundtrip_cov
	./test/test_regression_cov
	@rm -rf $(COV_RT_DIR) $(COV_RG_DIR) $(COV_MERGED_DIR)
	@mkdir -p $(COV_RT_DIR) $(COV_RG_DIR)
	@cd test && for f in test_roundtrip_cov-*.gcda test_roundtrip_cov-*.gcno; do \
		cp "$$f" "../$(COV_RT_DIR)/$${f#test_roundtrip_cov-}"; done
	@cd test && for f in test_regression_cov-*.gcda test_regression_cov-*.gcno; do \
		cp "$$f" "../$(COV_RG_DIR)/$${f#test_regression_cov-}"; done
	@$(GCOV_TOOL) merge -o $(COV_MERGED_DIR) $(COV_RT_DIR) $(COV_RG_DIR)
	@cp $(COV_RT_DIR)/*.gcno $(COV_MERGED_DIR)/
	@$(GCOV) -o $(COV_MERGED_DIR) $(TEST_SRC) 2>/dev/null \
		| grep -E "^File '(src|include)|Lines executed" \
		| grep -v "Unity\|emmintrin"

clean-coverage:
	rm -f test/test_roundtrip_cov test/test_regression_cov
	rm -f test/*.gcda test/*.gcno test/*.gcov
	rm -rf $(COV_RT_DIR) $(COV_RG_DIR) $(COV_MERGED_DIR)
	rm -f *.gcov coverage.info
	rm -rf coverage-html

.PHONY: all clean test test-roundtrip test-regression install coverage clean-coverage
