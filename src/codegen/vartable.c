#include "vartable.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

void var_table_init(VariableTable *vt) {
  vt->capacity = 4;
  vt->size = 0;
  vt->vars = malloc(vt->capacity * sizeof(Variable));
  if (!vt->vars) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
}

void var_table_free(VariableTable *vt) {
    for (size_t i = 0; i < vt->size; i++) {
        free(vt->vars[i].name);
    }
    free(vt->vars);
    vt->vars = NULL;
    vt->size = 0;
    vt->capacity = 0;
}

void var_table_append(VariableTable *vt, const char *name, size_t stack_size) {
    if (vt->size == vt->capacity) {
        vt->capacity *= 2;
        vt->vars = realloc(vt->vars, vt->capacity * sizeof(Variable));
        if (!vt->vars) {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
    }
    vt->vars[vt->size].name = strdup(name);
    vt->vars[vt->size].stack_loc = stack_size;
    vt->size++;
}

Variable *var_table_find(VariableTable *vt, const char *name) {
    for (size_t i = 0; i < vt->size; i++) {
        if (strcmp(vt->vars[i].name, name) == 0) {
            return &vt->vars[i];
        }
    }
    return NULL;
}

bool var_table_contains(VariableTable *vt, const char *name) {
    for (size_t i = 0; i < vt->size; i++) {
        if (strcmp(vt->vars[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}