#pragma once

#include <stddef.h>
#include <stdbool.h>

#define START        0
#define PEEK_CURRENT 0
#define PEEK_NEXT    1
#define PEEK_PREV   -1

typedef enum {
    TOKEN_EOF,
    TOKEN_EXIT,
    TOKEN_INT_LITERAL,
    TOKEN_SEMICOLON,
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_IDENT,
    TOKEN_LET,
    TOKEN_EQ,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTI,
    TOKEN_FSLASH,
    TOKEN_OPEN_CURLY,
    TOKEN_CLOSE_CURLY,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE
} TokenType;

typedef struct {
    TokenType type;
    int line;
    char *value;
} Token;

typedef struct {
    Token *tokens;
    size_t size;
    size_t capacity;
} TokenArray;

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
} Tokenizer;

TokenArray tokenize(const char *src);  // caller frees with token_array_free
void token_array_free(TokenArray *arr);  // frees tokens and token values
char *token_type_to_string(TokenType type);
