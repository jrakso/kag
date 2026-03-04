#include <stdio.h>
#include <stdlib.h>

#include "util/file.h"
#include "util/arena.h"
#include "util/strbuilder.h"
#include "tokenization/tokenization.h"
#include "parser/parser.h"
#include "codegen/generation.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <source_file>\n", argv[0]);
        return 1;
    }

    char *src = read_file(argv[1]);  // caller frees

    TokenArray tokens = tokenize(src);  // caller frees

    Arena arena;
    arena_init(&arena, 1024 * 1024 * 4);  // Caller frees

    NodeProg prog = parse(&tokens, &arena);

    StringBuilder sb = generate(&prog);  // Caller frees

    write_file("output.asm", sb.data);

    free(src);
    token_array_free(&tokens);
    sb_free(&sb);
    arena_free(&arena);

    return 0;
}
