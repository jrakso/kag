CC = gcc

CFLAGS = -Wall -Wextra -ggdb \
	-Isrc \
	-Isrc/helpers \
	-Isrc/tokenization \
	-Isrc/parser \
	-Isrc/codegen

# Find all .c files in src/
SRC := $(shell find src -name "*.c")

# Convert src/folder/file.c to build/folder/file.o
OBJ := $(patsubst src/%.c, build/%.o, $(SRC))

BIN = build/main

all: $(BIN)

# Link
$(BIN): $(OBJ)
	@mkdir -p build
	@$(CC) $(OBJ) -o $(BIN)

# Compile (place .o inside build/, mirroring src/)
build/%.o: src/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

run: all
	@if [ -z "$(FILE)" ]; then echo "Usage: make run FILE=program.kag"; exit 1; fi
	@./$(BIN) $(FILE)
	@nasm -f elf64 output.asm -o build/output.o
	@ld build/output.o -o build/output
	@./build/output; \
	code=$$?; \
	echo "program exited with code $$code"

clean:
	@rm -rf build
	@echo "Cleaned build files"
