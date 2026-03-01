#include "arena.h"
#include <stdlib.h>
#include <stdio.h>

Arena *arena_create(size_t size) {
    Arena *arena = malloc(sizeof(Arena));
    if (!arena) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    arena->base = malloc(size);
    if (!arena->base) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    arena->size = size;
    arena->offset = 0;
    return arena;
}

void *arena_alloc(Arena *arena, size_t size) {
    if (arena->offset + size > arena->size) {
        fprintf(stderr, "Arena out of memory\n");
        exit(EXIT_FAILURE);
    }
    void *ptr = arena->base + arena->offset;
    arena->offset += size;
    return ptr;
}

void arena_reset(Arena *arena) {
    arena->offset = 0;
}

void arena_destroy(Arena *arena) {
    free(arena->base);
    free(arena);
}