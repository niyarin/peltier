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
      src/selector.c \
      src/edn/edn_writer.c \
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
	rm -f tests/*.o tests/test_*

test: $(TARGET)
	@echo "Tests not yet implemented"

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean test install
