#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "tokenization/tokenization.h"

static NodeExpr *parse_expr(Parser *p, int min_bp);
static NodeStmt *parse_stmt(Parser *p);

/* ------------------------------------------------ */
/* Parser helpers */
/* ------------------------------------------------ */

void parser_init(Parser *p, const TokenArray *tokens, Arena *arena) {
    p->tokens = tokens->tokens;
    p->size = tokens->size;
    p->pos = 0;
    p->arena = arena;
}

static Token parser_peek(const Parser *p, int offset) {
    if (p->pos + offset >= p->size) {
        return (Token) { .type = TOKEN_EOF };
    }
    return p->tokens[p->pos + offset];
}

static Token parser_consume(Parser *p) {
    return p->tokens[p->pos++];
}

static void parser_error(Parser *p, const char *msg) {
    Token tok = parser_peek(p, PEEK_PREV);
    fprintf(stderr, "line %d: parser error: expected %s\n", tok.line, msg);
    exit(EXIT_FAILURE);
}

static Token parser_expect(Parser *p, TokenType type) {
    if (parser_peek(p, PEEK_CURRENT).type == type) {
        return parser_consume(p);
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "'%s'", token_type_to_string(type));
        parser_error(p, msg);
        return (Token){0}; // unreachable
    }
}

/* ------------------------------------------------ */
/* Expression parsing (pratt) */
/* ------------------------------------------------ */

static bool infix_binding_power(TokenType type, int *left_bp, int *right_bp) {
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            *left_bp = 1;
            *right_bp = 2;
            return true;
        case TOKEN_MULTI:
        case TOKEN_FSLASH:
            *left_bp = 3;
            *right_bp = 4;
            return true;
        default:
            return false;
    }
}

static NodeExpr *make_bin_expr(Parser *p, TokenType op, NodeExpr *lhs, NodeExpr *rhs) {
    NodeBinExpr *bin = arena_alloc(p->arena, sizeof(NodeBinExpr));
    switch (op) {
        case TOKEN_PLUS: {
            NodeBinExprAdd *add = arena_alloc(p->arena, sizeof(NodeBinExprAdd));
            add->lhs = lhs;
            add->rhs = rhs;
            bin->type = BIN_ADD;
            bin->data.add = add;
            break;
        }
        case TOKEN_MINUS: {
            NodeBinExprSub *sub = arena_alloc(p->arena, sizeof(NodeBinExprSub));
            sub->lhs = lhs;
            sub->rhs = rhs;
            bin->type = BIN_SUB;
            bin->data.sub = sub;
            break;
        }
        case TOKEN_MULTI: {
            NodeBinExprMulti *multi = arena_alloc(p->arena, sizeof(NodeBinExprMulti));
            multi->lhs = lhs;
            multi->rhs = rhs;
            bin->type = BIN_MULTI;
            bin->data.multi = multi;
            break;
        }
        case TOKEN_FSLASH: {
            NodeBinExprDiv *div = arena_alloc(p->arena, sizeof(NodeBinExprDiv));
            div->lhs = lhs;
            div->rhs = rhs;
            bin->type = BIN_DIV;
            bin->data.div = div;
            break;
        }
        default:
            parser_error(p, "binary operator");
    }
    NodeExpr *expr = arena_alloc(p->arena, sizeof(NodeExpr));
    expr->type = EXPR_BIN;
    expr->data.bin = bin;
    return expr;
}

static NodeExpr *parse_prefix(Parser *p) {
    Token tok = parser_peek(p, PEEK_CURRENT);
    switch (tok.type) {
        case TOKEN_INT_LITERAL: {
            parser_consume(p);
            NodeTermIntLit *lit = arena_alloc(p->arena, sizeof(NodeTermIntLit));
            lit->int_lit = tok;
            NodeTerm *term = arena_alloc(p->arena, sizeof(NodeTerm));
            term->type = TERM_INT_LIT;
            term->data.int_lit = lit;
            NodeExpr *expr = arena_alloc(p->arena, sizeof(NodeExpr));
            expr->type = EXPR_TERM;
            expr->data.term = term;
            return expr;
        }
        case TOKEN_IDENT: {
            parser_consume(p);
            NodeTermIdent *ident = arena_alloc(p->arena, sizeof(NodeTermIdent));
            ident->ident = tok;
            NodeTerm *term = arena_alloc(p->arena, sizeof(NodeTerm));
            term->type = TERM_IDENT;
            term->data.ident = ident;
            NodeExpr *expr = arena_alloc(p->arena, sizeof(NodeExpr));
            expr->type = EXPR_TERM;
            expr->data.term = term;
            return expr;
        }
        case TOKEN_OPEN_PAREN: {
            parser_consume(p);
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) {
                parser_error(p, "expression");
            }
            parser_expect(p, TOKEN_CLOSE_PAREN);
            return expr;
        }
        default:
            parser_error(p, "expression");
            return NULL;
    }
}

static NodeExpr *parse_expr(Parser *p, int min_bp) {
    NodeExpr *lhs = parse_prefix(p);
    while (true) {
        Token tok = parser_peek(p, PEEK_CURRENT);
        int lbp = 0;
        int rbp = 0;
        if (!infix_binding_power(tok.type, &lbp, &rbp) || lbp < min_bp) {
            break;
        }
        parser_consume(p);
        NodeExpr *rhs = parse_expr(p, rbp);
        if (!rhs) {
            parser_error(p, "expression");
        }
        lhs = make_bin_expr(p, tok.type, lhs, rhs);
    }
    return lhs;
}

/* ------------------------------------------------ */
/* Scope parsing */
/* ------------------------------------------------ */

static NodeScope *parse_scope(Parser *p) {
    parser_expect(p, TOKEN_OPEN_CURLY);
    NodeScope *scope = arena_alloc(p->arena, sizeof(NodeScope));
    scope->size = 0;
    scope->capacity = 4;
    scope->stmts = arena_alloc(p->arena, scope->capacity * sizeof(NodeStmt *));
    while (parser_peek(p, PEEK_CURRENT).type != TOKEN_CLOSE_CURLY &&
            parser_peek(p, PEEK_CURRENT).type != TOKEN_EOF) {
        NodeStmt *stmt = parse_stmt(p);
        if (!stmt) {
            parser_error(p, "statement");
        }
        if (scope->size == scope->capacity) {
            size_t new_capacity = scope->capacity * 2;
            NodeStmt **new_array = arena_alloc(p->arena, new_capacity * sizeof(NodeStmt *));
            for (size_t i = 0; i < scope->size; i++) {
                new_array[i] = scope->stmts[i];
            }
            scope->stmts = new_array;
            scope->capacity = new_capacity;
        }
        scope->stmts[scope->size++] = stmt;
    }
    parser_expect(p, TOKEN_CLOSE_CURLY);
    return scope;
}

/* ------------------------------------------------ */
/* If predicate parsing */
/* ------------------------------------------------ */

static NodeIfPred *parse_if_pred(Parser *p) {
    const Token tok = parser_peek(p, PEEK_CURRENT);
    switch (tok.type) {
        case TOKEN_ELIF: {
            parser_consume(p);
            parser_expect(p, TOKEN_OPEN_PAREN);
            NodeIfPredElif *elif = arena_alloc(p->arena, sizeof(NodeIfPredElif));
            elif->expr = parse_expr(p, 0);
            if (!elif->expr) {
                parser_error(p, "expression");         
            }
            parser_expect(p, TOKEN_CLOSE_PAREN);
            elif->scope = parse_scope(p);
            elif->pred = parse_if_pred(p);  // Recursion
            NodeIfPred *pred = arena_alloc(p->arena, sizeof(NodeIfPred));
            pred->type = IFPRED_ELIF;
            pred->data.elif = elif; 
            return pred;
        }
        case TOKEN_ELSE: {
            parser_consume(p);
            NodeIfPredElse *else_ = arena_alloc(p->arena, sizeof(NodeIfPredElse));
            else_->scope = parse_scope(p);
            NodeIfPred *pred = arena_alloc(p->arena, sizeof(NodeIfPred));
            pred->type = IFPRED_ELSE;
            pred->data.else_ = else_; 
            return pred;
        }
        default: {
            return NULL;
        }
    }
}

/* ------------------------------------------------ */
/* Statement parsing */
/* ------------------------------------------------ */

static NodeStmt *parse_stmt(Parser *p) {
    const Token tok = parser_peek(p, PEEK_CURRENT);
    switch (tok.type) {
        case TOKEN_EXIT: {
            parser_consume(p);
            parser_expect(p, TOKEN_OPEN_PAREN);
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) { 
                parser_error(p, "expression");
            }
            parser_expect(p, TOKEN_CLOSE_PAREN);
            parser_expect(p, TOKEN_SEMICOLON);
            NodeStmt *stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            stmt->type = STMT_EXIT;
            stmt->data.exit = arena_alloc(p->arena, sizeof(NodeStmtExit));
            stmt->data.exit->expr = expr; 
            return stmt;
        }
        case TOKEN_LET: {
            parser_consume(p);
            Token ident = parser_expect(p, TOKEN_IDENT);
            parser_expect(p, TOKEN_EQ);
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) {
                parser_error(p, "expression");
            }
            NodeStmt *stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            stmt->type = STMT_LET;
            stmt->data.let = arena_alloc(p->arena, sizeof(NodeStmtLet));
            stmt->data.let->ident = ident;
            stmt->data.let->expr = expr;
            parser_expect(p, TOKEN_SEMICOLON);
            return stmt;
        }
        case TOKEN_IF: {
            parser_consume(p);
            parser_expect(p, TOKEN_OPEN_PAREN);           
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) {
                parser_error(p, "expression");
            }
            parser_expect(p, TOKEN_CLOSE_PAREN);
            NodeStmtIf *if_ = arena_alloc(p->arena, sizeof(NodeStmtIf));
            if_->expr = expr; 
            if_->scope = parse_scope(p);
            if_->pred = parse_if_pred(p);
            NodeStmt *stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            stmt->type = STMT_IF;
            stmt->data.if_ = if_;
            return stmt;
        }
        case TOKEN_IDENT: {
            Token ident = parser_consume(p);
            parser_expect(p, TOKEN_EQ);
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) {
                parser_error(p, "expression");
            }
            parser_expect(p, TOKEN_SEMICOLON);
            NodeStmtAssign *assign = arena_alloc(p->arena, sizeof(NodeStmtAssign));
            assign->ident = ident;
            assign->expr = expr;
            NodeStmt *stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            stmt->type = STMT_ASSIGN;
            stmt->data.assign = assign;
            return stmt;
        }
        case TOKEN_OPEN_CURLY: {
            NodeStmt *stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            stmt->type = STMT_SCOPE;
            stmt->data.scope = parse_scope(p);  // Consumes open curly
            return stmt;
        }
        default: {
            return NULL;
        }

    }
}

/* ------------------------------------------------ */
/* Program parsing */
/* ------------------------------------------------ */

void parse_prog(Parser *p, NodeProg *prog) {
    while (parser_peek(p, PEEK_CURRENT).type != TOKEN_EOF) {
        NodeStmt *stmt = parse_stmt(p);
        if (!stmt) {
            parser_error(p, "statement");
        }
        if (prog->size == prog->capacity) {
            size_t new_capacity = prog->capacity * 2;
            NodeStmt **new_array = arena_alloc(p->arena, new_capacity * sizeof(NodeStmt *));
            for (size_t i = 0; i < prog->size; i++) {
                new_array[i] = prog->stmts[i];
            }
            prog->stmts = new_array;
            prog->capacity = new_capacity;
        }
        prog->stmts[prog->size++] = stmt;
    }
}

/* ------------------------------------------------ */
/* Parser API */
/* ------------------------------------------------ */

NodeProg parse(const TokenArray *tokens, Arena *arena) {
    if (!tokens || !arena) {
        return (NodeProg) {0};
    }
    Parser p;
    parser_init(&p, tokens, arena);
    NodeProg prog;
    prog.size = 0;
    prog.capacity = 4;
    prog.stmts = arena_alloc(p.arena, prog.capacity * sizeof(NodeStmt *));
    parse_prog(&p, &prog);
    return prog;
}
