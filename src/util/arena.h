#pragma once

#include <stdlib.h>
#include <stddef.h>

typedef struct {
    unsigned char *base;
    size_t size;
    size_t offset;
} Arena;

void arena_init(Arena *arena, size_t size);
void *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);