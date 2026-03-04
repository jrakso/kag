#pragma once

#include <stddef.h>
#include <stdarg.h>

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StringBuilder;

void sb_init(StringBuilder *sb);
void sb_append(StringBuilder *sb, const char *str);
void sb_append_fmt(StringBuilder *sb, const char *fmt, ...);
void sb_append_fmtv(StringBuilder *sb, const char *fmt, va_list args);
const char *sb_data(const StringBuilder *sb);
void sb_free(StringBuilder *sb);