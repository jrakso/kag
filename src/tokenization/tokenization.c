#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "tokenization.h"

/* ------------------------------------------------ */
/* TokenArray helpers */
/* ------------------------------------------------ */

static void token_array_init(TokenArray *tokens) {
    tokens->size = 0;
    tokens->capacity = 4;
    tokens->tokens = malloc(sizeof(Token) * tokens->capacity);  // caller frees with token_array_free
    if (!tokens->tokens) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
}

static void token_array_append(TokenArray *tokens, Token tok) {
    if (tokens->size == tokens->capacity) {
        tokens->capacity *= 2;
        Token *new_tokens = realloc(tokens->tokens, sizeof(Token) * tokens->capacity);
        if (!new_tokens) {
            perror("realloc");
            token_array_free(tokens);
            exit(EXIT_FAILURE);
        }
        tokens->tokens = new_tokens;
    }
    tokens->tokens[tokens->size++] = tok;
}

/* ------------------------------------------------ */
/* Tokenizer helpers */
/* ------------------------------------------------ */

static char *slice_string(const char *src, size_t start, size_t end) {
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

static char peek(const Tokenizer *t, size_t offset) {
    if (t->pos + offset >= t->len || t->src[t->pos + offset] == '\0') {
        return '\0';
    } else {
        return t->src[t->pos + offset];
    }
}

static char consume(Tokenizer *t) {
    return t->src[t->pos++];
}

/* ------------------------------------------------ */
/* Tokenizer API */
/* ------------------------------------------------ */

TokenArray tokenize(const char *src) {
    Tokenizer t = { .src = src, .len = strlen(src), .pos = START };
    TokenArray tokens;
    token_array_init(&tokens);  // caller frees with token_array_free
    int line_count = 1;
    while (peek(&t, PEEK_CURRENT) != '\0') {
        const char c = peek(&t, PEEK_CURRENT);

        if (isalpha(c)) {
            size_t start = t.pos;
            consume(&t);
            while (isalnum(peek(&t, PEEK_CURRENT)))
                consume(&t);
            size_t end = t.pos;
            char *value = slice_string(t.src, start, end);
            if (strcmp(value, "exit") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_EXIT, .line = line_count, .value = NULL });
                free(value);  // prevent leak
            } else if (strcmp(value, "let") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_LET, .line = line_count, .value = NULL });
                free(value);  // prevent leak)
            } else if (strcmp(value, "if") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_IF, .line = line_count, .value = NULL });
                free(value);  // prevent leak)
            } else if (strcmp(value, "elif") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_ELIF, .line = line_count, .value = NULL });
                free(value);  // prevent leak)
            } else if (strcmp(value, "else") == 0) {
                token_array_append(&tokens, (Token){ .type = TOKEN_ELSE, .line = line_count, .value = NULL });
                free(value);  // prevent leak)
            } else {
                token_array_append(&tokens, (Token){ .type = TOKEN_IDENT, .line = line_count, .value = value });
            }
        }

        else if (isdigit(c)) {
            size_t start = t.pos;
            consume(&t);
            while (isdigit(peek(&t, PEEK_CURRENT)))
                consume(&t);
            size_t end = t.pos;
            char *value = slice_string(t.src, start, end);
            token_array_append(&tokens, (Token){ .type = TOKEN_INT_LITERAL, .line = line_count, .value = value });
        }

        else if (c == '"') {
            consume(&t);
            size_t start = t.pos;
            while (peek(&t, PEEK_CURRENT) != '\0' && peek(&t, PEEK_CURRENT) != '\n' && peek(&t, PEEK_CURRENT) != '"') {
                consume(&t);
            }
            if (peek(&t, PEEK_CURRENT) != '"') {
                fprintf(stderr, "line %d: tokenizer error: unterminated string literal\n", line_count);
                token_array_free(&tokens);
                exit(EXIT_FAILURE);
            }
            size_t end = t.pos;
            consume(&t);
            char *value = slice_string(t.src, start, end);
            token_array_append(&tokens, (Token){ .type = TOKEN_STRING_LITERAL, .line = line_count, .value = value });
        }
        
        else if (c == '/' && peek(&t, PEEK_NEXT) == '/') {
            consume(&t);
            consume(&t);
            while (peek(&t, PEEK_CURRENT) != '\0' && peek(&t, PEEK_CURRENT) != '\n') {
                consume(&t);
            }
        } 
        
        else {
            switch (c) {
                case '(':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_OPEN_PAREN, .line = line_count, .value = NULL });
                    break;
                case ')':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_CLOSE_PAREN, .line = line_count, .value = NULL });
                    break;
                case '=':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_EQ, .line = line_count, .value = NULL });
                    break;
                case '+':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_PLUS, .line = line_count, .value = NULL });
                    break;
                case '-':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_MINUS, .line = line_count, .value = NULL });
                    break;
                case '*':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_MULTI, .line = line_count, .value = NULL });
                    break;
                case '/':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_FSLASH, .line = line_count, .value = NULL });
                    break;
                case ';':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_SEMICOLON, .line = line_count, .value = NULL });
                    break;
                case '{':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_OPEN_CURLY, .line = line_count, .value = NULL });
                    break;
                case '}':
                    consume(&t);
                    token_array_append(&tokens, (Token){ .type = TOKEN_CLOSE_CURLY, .line = line_count, .value = NULL });
                    break;
                case '\n':
                    consume(&t);
                    line_count++;
                    break;
                default:
                    if (isspace(c)) {
                        consume(&t);
                    } else {
                        fprintf(stderr, "line %d: tokenizer error: invalid token '%c'\n", line_count, c);
                        token_array_free(&tokens);
                        exit(EXIT_FAILURE);                    
                    }
                    break;
            }
        }
    }
    return tokens;
}

void token_array_free(TokenArray *tokens) {
    if (!tokens) return;
    for (size_t i = 0; i < tokens->size; i++)
        free(tokens->tokens[i].value);  // frees values allocated by get_token_value
    free(tokens->tokens);  // frees tokens allocated in token_array_init
    tokens->tokens = NULL;
    tokens->size = 0;
    tokens->capacity = 0;
}

char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EOF:
            return "end of file";
        case TOKEN_EXIT:
            return "exit";
        case TOKEN_INT_LITERAL:
            return "integer literal";
        case TOKEN_SEMICOLON:
            return ";";
        case TOKEN_OPEN_PAREN:
            return "(";
        case TOKEN_CLOSE_PAREN:
            return ")";
        case TOKEN_IDENT:
            return "identifier";
        case TOKEN_LET:
            return "let";
        case TOKEN_EQ:
            return "=";
        case TOKEN_PLUS:
            return "+";
        case TOKEN_MINUS:
            return "-";
        case TOKEN_MULTI:
            return "*";
        case TOKEN_FSLASH:
            return "/";
        case TOKEN_OPEN_CURLY:
            return "{";
        case TOKEN_CLOSE_CURLY:
            return "}";
        case TOKEN_IF:
            return "if";
        case TOKEN_ELIF:
            return "elif";
        case TOKEN_ELSE:
            return "else";
        default:
            return "unknown token";
    }
}
