#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "tokenization/tokenization.h"

static NodeExpr *parse_expr(Parser *p, int min_prec);
static NodeStmt *parse_stmt(Parser *p);

void parser_init(Parser *p, const TokenArray *arr) {
    p->tokens = arr->tokens;
    p->size = arr->size;
    p->pos = 0;
    p->arena = arena_create(1024 * 1024 * 4);  // 4 mb
}

void parser_free(Parser *p) {
    arena_destroy(p->arena);
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
    Token t = parser_peek(p, PEEK_PREV);
    fprintf(stderr, "line %d: parser error: expected %s\n", t.line, msg);
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

static NodeTerm *parse_term(Parser *p) {
    switch (parser_peek(p, PEEK_CURRENT).type) {
        case TOKEN_INT_LITERAL: {
            NodeTermIntLit *term_int_lit = arena_alloc(p->arena, sizeof(NodeTermIntLit));
            term_int_lit->int_lit = parser_consume(p);
            NodeTerm *term = arena_alloc(p->arena, sizeof(NodeTerm));
            term->type = TERM_INT_LIT;
            term->data.int_lit = term_int_lit;
            return term;
        }
        case TOKEN_IDENT: {
            NodeTermIdent *term_ident = arena_alloc(p->arena, sizeof(NodeTermIdent));
            term_ident->ident = parser_consume(p);
            NodeTerm *term = arena_alloc(p->arena, sizeof(NodeTerm));
            term->type = TERM_IDENT;
            term->data.ident = term_ident;
            return term;
        }
        case TOKEN_OPEN_PAREN: {
            parser_consume(p);
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) {
                parser_error(p, "expression");
            }
            parser_expect(p, TOKEN_CLOSE_PAREN);
            NodeTermParen *term_paren = arena_alloc(p->arena, sizeof(NodeTermParen));
            term_paren->expr = expr;
            NodeTerm *term = arena_alloc(p->arena, sizeof(NodeTerm));
            term->type = TERM_PAREN;
            term->data.paren = term_paren;
            return term;
        }
        default: {
            return NULL;
        }
    }
}

static int bin_prec(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
            return 0;
        case TOKEN_MINUS:
            return 0;
        case TOKEN_MULTI:
            return 1;
        case TOKEN_FSLASH:
            return 1;
        default:
            return -1;
    }
}

static NodeExpr *parse_expr(Parser *p, int min_prec) {
    NodeTerm *term_lhs = parse_term(p);
    if (!term_lhs) {
        return NULL;
    }
    NodeExpr *expr_lhs = arena_alloc(p->arena, sizeof(NodeExpr));
    expr_lhs->type = EXPR_TERM;
    expr_lhs->data.term = term_lhs;
    while (true) {
        Token curr_tok = parser_peek(p, PEEK_CURRENT);
        int prec = bin_prec(curr_tok.type);
        if (prec < min_prec) {
            break;
        }
        Token op = parser_consume(p);
        NodeExpr *expr_rhs = parse_expr(p, prec + 1);
        if (!expr_rhs) {
            parser_error(p, "expression");
        }
        NodeBinExpr *bin_expr = arena_alloc(p->arena, sizeof(NodeBinExpr));
        switch (op.type) {
            case TOKEN_PLUS: {
                NodeBinExprAdd *bin_expr_add = arena_alloc(p->arena, sizeof(NodeBinExprAdd));
                bin_expr_add->lhs = expr_lhs;
                bin_expr_add->rhs = expr_rhs;
                bin_expr->type = BIN_ADD;
                bin_expr->data.add = bin_expr_add;
                break;
            }
            case TOKEN_MINUS: {
                NodeBinExprSub *bin_expr_sub = arena_alloc(p->arena, sizeof(NodeBinExprSub));
                bin_expr_sub->lhs = expr_lhs;
                bin_expr_sub->rhs = expr_rhs;
                bin_expr->type = BIN_SUB;
                bin_expr->data.sub = bin_expr_sub;
                break;
            }
            case TOKEN_MULTI: {
                NodeBinExprMulti *bin_expr_multi = arena_alloc(p->arena, sizeof(NodeBinExprMulti));
                bin_expr_multi->lhs = expr_lhs;
                bin_expr_multi->rhs = expr_rhs;
                bin_expr->type = BIN_MULTI;
                bin_expr->data.multi = bin_expr_multi;
                break;
            }
            case TOKEN_FSLASH: {
                NodeBinExprDiv *bin_expr_div = arena_alloc(p->arena, sizeof(NodeBinExprDiv));
                bin_expr_div->lhs = expr_lhs;
                bin_expr_div->rhs = expr_rhs;
                bin_expr->type = BIN_DIV;
                bin_expr->data.div = bin_expr_div;
                break;
            }
            default: {
                parser_error(p, "valid binary operator");
            }
        }
        NodeExpr *new_expr = arena_alloc(p->arena, sizeof(NodeExpr));
        new_expr->type = EXPR_BIN;
        new_expr->data.bin = bin_expr;
        expr_lhs = new_expr;
    }
    return expr_lhs;
}

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

static NodeIfPred *parse_if_pred(Parser *p) {
    const Token t = parser_peek(p, PEEK_CURRENT);
    switch (t.type) {
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

static NodeStmt *parse_stmt(Parser *p) {
    const Token t = parser_peek(p, PEEK_CURRENT);
    switch (t.type) {
        case TOKEN_EXIT: {
            parser_consume(p);
            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_EXIT;
            node_stmt->data.exit = arena_alloc(p->arena, sizeof(NodeStmtExit));
            parser_expect(p, TOKEN_OPEN_PAREN);
            NodeExpr *node_expr = parse_expr(p, 0);
            if (!node_expr) { 
                parser_error(p, "expression");
            }
            node_stmt->data.exit->expr = node_expr; 
            parser_expect(p, TOKEN_CLOSE_PAREN);
            parser_expect(p, TOKEN_SEMICOLON);
            return node_stmt;
        }
        case TOKEN_LET: {
            parser_consume(p);
            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_LET;
            node_stmt->data.let = arena_alloc(p->arena, sizeof(NodeStmtLet));
            node_stmt->data.let->ident = parser_expect(p, TOKEN_IDENT);
            parser_expect(p, TOKEN_EQ);
            NodeExpr *node_expr = parse_expr(p, 0);
            if (!node_expr) {
                parser_error(p, "expression");
            }
            node_stmt->data.let->expr = node_expr;
            parser_expect(p, TOKEN_SEMICOLON);
            return node_stmt;
        }
        case TOKEN_IF: {
            parser_consume(p);
            parser_expect(p, TOKEN_OPEN_PAREN);
            NodeStmtIf *stmt_if = arena_alloc(p->arena, sizeof(NodeStmtIf));            
            NodeExpr *node_expr = parse_expr(p, 0);
            if (!node_expr) {
                parser_error(p, "expression");
            }
            stmt_if->expr = node_expr;
            parser_expect(p, TOKEN_CLOSE_PAREN);
            stmt_if->scope = parse_scope(p);
            stmt_if->pred = parse_if_pred(p);
            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_IF;
            node_stmt->data.if_ = stmt_if;
            return node_stmt;
        }
        case TOKEN_IDENT: {
            Token ident = parser_consume(p);
            parser_expect(p, TOKEN_EQ);
            NodeStmtAssign *assign = arena_alloc(p->arena, sizeof(NodeStmtAssign));
            assign->ident = ident;
            NodeExpr *expr = parse_expr(p, 0);
            if (!expr) {
                parser_error(p, "expression");
            }
            assign->expr = expr;
            parser_expect(p, TOKEN_SEMICOLON);
            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_ASSIGN;
            node_stmt->data.assign = assign;
            return node_stmt;
        }
        case TOKEN_OPEN_CURLY: {
            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_SCOPE;
            node_stmt->data.scope = parse_scope(p);  // Consumes open curly
            return node_stmt;
        }
        default: {
            return NULL;
        }

    }
}

NodeProg parse_prog(Parser *p) {
    if (!p) {
        return (NodeProg) {0};
    }
    NodeProg prog;
    prog.size = 0;
    prog.capacity = 4;
    prog.stmts = arena_alloc(p->arena, prog.capacity * sizeof(NodeStmt *));
    while (parser_peek(p, PEEK_CURRENT).type != TOKEN_EOF) {
        NodeStmt *stmt = parse_stmt(p);
        if (!stmt) {
            parser_error(p, "statement");
        }
        if (prog.size == prog.capacity) {
            size_t new_capacity = prog.capacity * 2;
            NodeStmt **new_array = arena_alloc(p->arena, new_capacity * sizeof(NodeStmt *));
            for (size_t i = 0; i < prog.size; i++) {
                new_array[i] = prog.stmts[i];
            }
            prog.stmts = new_array;
            prog.capacity = new_capacity;
        }
        prog.stmts[prog.size++] = stmt;
    }

    return prog;
}
