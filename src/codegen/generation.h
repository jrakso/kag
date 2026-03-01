#pragma once

#include "parser/parser.h"
#include "helpers/strbuilder.h"
#include "vartable.h"

typedef struct Scope Scope;

typedef struct {
    const NodeProg *prog;
    StringBuilder sb;
    size_t stack_size;
    Scope *current_scope;
} Generator;

void generator_init(Generator *g, const NodeProg *prog);
char *gen_prog(Generator *g);
void generator_free(Generator *g);
