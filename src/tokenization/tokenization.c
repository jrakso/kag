#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "tokenization.h"

static void token_array_init(TokenArray *arr) {
    arr->size = 0;
    arr->capacity = 4;
    arr->tokens = malloc(sizeof(Token) * arr->capacity);  // caller frees with token_array_free
    if (!arr->tokens) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

static void token_array_append(TokenArray *arr, Token t) {
    if (arr->size == arr->capacity) {
        arr->capacity *= 2;
        Token *new_tokens = realloc(arr->tokens, sizeof(Token) * arr->capacity);
        if (!new_tokens) {
            perror("realloc");
            token_array_free(arr);
            exit(EXIT_FAILURE);
        }
        arr->tokens = new_tokens;
    }
    arr->tokens[arr->size++] = t;
}

void token_array_free(TokenArray *arr) {
    if (!arr) return;
    for (size_t i = 0; i < arr->size; i++)
        free(arr->tokens[i].value);  // frees values allocated by get_token_value
    free(arr->tokens);  // frees tokens allocated in token_array_init
    arr->tokens = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

static char *copy_token_value(const char *src, size_t start, size_t end) {
    size_t len = end - start;
    char *value = malloc(len + 1);  // caller frees with token_array_free
    if (!value) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memcpy(value, src + start, len);
    value[len] = '\0';
    return value;
}

static char tokenizer_peek(const Tokenizer *t, size_t offset) {
    if (t->pos + offset >= t->len || t->src[t->pos + offset] == '\0') {
        return '\0';
    } else {
        return t->src[t->pos + offset];
    }
}

static char tokenizer_consume(Tokenizer *t) {
    return t->src[t->pos++];
}

int bin_prec(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
            return 1;
        case TOKEN_MULTI:
            return 2;
        default:
            return -1;
    }
}

TokenArray tokenize(const char *src) {
    Tokenizer t = { .src = src, .len = strlen(src), .pos = START };
    TokenArray tokens;
    token_array_init(&tokens);  // caller frees with token_array_free

    while (tokenizer_peek(&t, PEEK_CURRENT) != '\0') {
        const char c = tokenizer_peek(&t, PEEK_CURRENT);

        if (isalpha(c)) {
            size_t start = t.pos;
            tokenizer_consume(&t);
            while (isalnum(tokenizer_peek(&t, PEEK_CURRENT)))
                tokenizer_consume(&t);
            size_t end = t.pos;
            char *value = copy_token_value(t.src, start, end);

            if (strcmp(value, "exit") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_EXIT, .value = NULL });
                free(value);  // prevent leak
            } else if (strcmp(value, "let") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_LET, .value = NULL });
                free(value);  // prevent leak)
            } else {
                token_array_append(&tokens, (Token){ .type = TOKEN_IDENT, .value = value });
            }
        }

        else if (isdigit(c)) {
            size_t start = t.pos;
            tokenizer_consume(&t);
            while (isdigit(tokenizer_peek(&t, PEEK_CURRENT)))
                tokenizer_consume(&t);
            size_t end = t.pos;
            char *value = copy_token_value(t.src, start, end);
            token_array_append(&tokens, (Token){ .type = TOKEN_INT_LITERAL, .value = value });
        }

        else if (c == '(') {
            tokenizer_consume(&t);
            token_array_append(&tokens, (Token){ .type = TOKEN_OPEN_PAREN, .value = NULL });
        }

        else if (c == ')') {
            tokenizer_consume(&t);
            token_array_append(&tokens, (Token){ .type = TOKEN_CLOSE_PAREN, .value = NULL });
        }

        else if (c == '=') {
            tokenizer_consume(&t);
            token_array_append(&tokens, (Token){ .type = TOKEN_EQ, .value = NULL });
        }

        else if (c == '+') {
            tokenizer_consume(&t);
            token_array_append(&tokens, (Token){ .type = TOKEN_PLUS, .value = NULL });
        }

        else if (c == '*') {
            tokenizer_consume(&t);
            token_array_append(&tokens, (Token){ .type = TOKEN_MULTI, .value = NULL });
        }


        else if (c == ';') {
            tokenizer_consume(&t);
            token_array_append(&tokens, (Token){ .type = TOKEN_SEMICOLON, .value = NULL });
        }

        else if (isspace(c)) {
            tokenizer_consume(&t);
        }

        else {
            fprintf(stderr, "Unexpected character '%c' at index %zu\n",
                    tokenizer_peek(&t, PEEK_CURRENT), t.pos);
            token_array_free(&tokens);
            exit(EXIT_FAILURE);
        }
    }
    return tokens;
}