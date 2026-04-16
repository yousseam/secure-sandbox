CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
INCLUDE := -Iinclude
LIBS := -lseccomp

SRC := $(shell find src -name "*.c")
BIN := bin/sandbox

all: $(BIN)

$(BIN): $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) $(LIBS) -o $(BIN)

clean:
	rm -rf bin

.PHONY: all clean
