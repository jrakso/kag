#pragma once

#include "parser/parser.h"
#include "util/strbuilder.h"
#include "vartable.h"

typedef struct Scope Scope;

typedef struct {
    const NodeProg *prog;
    StringBuilder *sb;
    size_t stack_size;
    size_t label_count;
    Scope *current_scope;
    int nested_level;
} Generator;

StringBuilder generate(const NodeProg *prog);
