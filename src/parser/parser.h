#pragma once

#include "tokenization/tokenization.h"
#include "helpers/arena.h"

// Forward declarations for pointer references
typedef struct NodeExpr NodeExpr;
typedef struct NodeBinExpr NodeBinExpr;
typedef struct NodeBinExprMulti NodeBinExprMulti;
typedef struct NodeBinExprDiv NodeBinExprDiv;
typedef struct NodeBinExprAdd NodeBinExprAdd;
typedef struct NodeBinExprSub NodeBinExprSub;
typedef struct NodeStmt NodeStmt;
typedef struct NodeTerm NodeTerm;

// ─── Enums ─────────────────────────────────────────────
typedef enum {
    BIN_INVALID,
    BIN_MULTI,
    BIN_DIV,
    BIN_ADD,
    BIN_SUB
} NodeBinExprType;

typedef enum {
    TERM_INT_LIT,
    TERM_IDENT,
    TERM_PAREN
} NodeTermType;

typedef enum {
    EXPR_TERM,
    EXPR_BIN
} NodeExprType;

typedef enum {
    STMT_INVALID,
    STMT_EXIT,
    STMT_LET,
    STMT_SCOPE,
    STMT_IF
} NodeStmtType;

// ─── Expression Nodes ──────────────────────────────────
typedef struct {
    Token int_lit;
} NodeTermIntLit;

typedef struct {
    Token ident;
} NodeTermIdent;

typedef struct {
    NodeExpr *expr;
} NodeTermParen;

struct NodeBinExpr {
    NodeBinExprType type;
    union {
        NodeBinExprMulti *multi;
        NodeBinExprDiv *div;
        NodeBinExprAdd *add;
        NodeBinExprSub *sub;
    } data;
};

struct NodeTerm {
    NodeTermType type;
    union {
        NodeTermIntLit *int_lit;
        NodeTermIdent *ident;
        NodeTermParen *paren;
    } data;
};

struct NodeExpr {
    NodeExprType type;
    union {
        NodeBinExpr *bin;
        NodeTerm *term;
    } data;
};

struct NodeBinExprMulti {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprDiv {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprAdd {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

struct NodeBinExprSub {
    NodeExpr *lhs;
    NodeExpr *rhs;
};

// ─── Statement Nodes ───────────────────────────────────
typedef struct {
    NodeExpr *expr;
} NodeStmtExit;

typedef struct {
    Token ident;
    NodeExpr *expr;
} NodeStmtLet;

typedef struct {
    NodeStmt **stmts;
    size_t size;
    size_t capacity;
} NodeScope;

typedef struct {
    NodeExpr *expr;
    NodeScope *scope;
} NodeStmtIf;

struct NodeStmt {
    NodeStmtType type;
    union {
        NodeStmtExit *exit;
        NodeStmtLet *let;
        NodeScope *scope;
        NodeStmtIf *if_;
    } data;
};

// ─── Program Node ──────────────────────────────────────
typedef struct {
    NodeStmt **stmts;
    size_t size;
    size_t capacity;
} NodeProg;

// ─── Parser ────────────────────────────────────────────
typedef struct {
    const Token *tokens;
    size_t size;
    size_t pos;
    Arena *arena;
} Parser;

// ─── Function Declarations ─────────────────────────────
void parser_init(Parser *p, const TokenArray *arr);
NodeProg parse_prog(Parser *p);
void parser_free(Parser *p);
