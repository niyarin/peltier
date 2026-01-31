CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS =

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
