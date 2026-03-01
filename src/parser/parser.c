#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "parser.h"
#include "tokenization/tokenization.h"

static NodeExpr *parse_expr(Parser *p, int min_prec);

void parser_init(Parser *p, const TokenArray *arr) {
    p->tokens = arr->tokens;
    p->size = arr->size;
    p->pos = 0;
    p->arena = arena_create(1024 * 1024 * 4);  // 4 mb
}

void parser_free(Parser *p) {
    arena_destroy(p->arena);
}

static Token parser_peek(const Parser *p, size_t offset) {
    if (p->pos + offset >= p->size) {
        return (Token) { .type = TOKEN_EOF };
    }
    return p->tokens[p->pos + offset];
}

static Token parser_consume(Parser *p) {
    return p->tokens[p->pos++];
}

static Token parser_expect(Parser *p, TokenType type, const char *err) {
    if (parser_peek(p, PEEK_CURRENT).type == type) {
        return parser_consume(p);
    } else {
        fprintf(stderr, "%s", err);
        exit(EXIT_FAILURE);
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

        default: {
    return NULL;
}

    }
}

static int bin_prec(TokenType type) {
    switch (type) {
        case TOKEN_PLUS:
            return 1;
        case TOKEN_MULTI:
            return 2;
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

        if (curr_tok.type == TOKEN_EOF || prec < min_prec) {
            break;
        }

        Token op = parser_consume(p);

        NodeExpr *expr_rhs = parse_expr(p, prec + 1);
        if (!expr_rhs) {
            fprintf(stderr, "Unable to parse expression\n");
            exit(EXIT_FAILURE);
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

            case TOKEN_MULTI: {
                NodeBinExprMulti *bin_expr_multi = arena_alloc(p->arena, sizeof(NodeBinExprMulti));
                bin_expr_multi->lhs = expr_lhs;
                bin_expr_multi->rhs = expr_rhs;
                bin_expr->type = BIN_MULTI;
                bin_expr->data.multi = bin_expr_multi;
                break;
            }

            default: {
                fprintf(stderr, "Invalid binary operator\n");
                exit(EXIT_FAILURE);
            }

        }

        NodeExpr *new_expr = arena_alloc(p->arena, sizeof(NodeExpr));
        new_expr->type = EXPR_BIN;
        new_expr->data.bin = bin_expr;
        expr_lhs = new_expr;

    }
    return expr_lhs;
}

static NodeStmt *parse_stmt(Parser *p) {
    const Token t = parser_peek(p, PEEK_CURRENT);
    parser_consume(p);

    switch (t.type) {

        case TOKEN_EXIT: {

            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_EXIT;
            node_stmt->data.exit = arena_alloc(p->arena, sizeof(NodeStmtExit));
            parser_expect(p, TOKEN_OPEN_PAREN, "Expected '('\n");

            NodeExpr *node_expr = parse_expr(p, 0);
            if (node_expr) { 
                node_stmt->data.exit->expr = node_expr; 
                parser_expect(p, TOKEN_CLOSE_PAREN, "Expected ')'\n");
                parser_expect(p, TOKEN_SEMICOLON, "Expected ';'\n");
                return node_stmt;

            } else {
                fprintf(stderr, "Invalid expression\n");
                exit(EXIT_FAILURE);
            }
        }

        case TOKEN_LET: {

            NodeStmt *node_stmt = arena_alloc(p->arena, sizeof(NodeStmt));
            node_stmt->type = STMT_LET;
            node_stmt->data.let = arena_alloc(p->arena, sizeof(NodeStmtLet));
            node_stmt->data.let->ident = parser_expect(p, TOKEN_IDENT, "Expected Identifier\n");
            parser_expect(p, TOKEN_EQ, "Expected '='\n");

            NodeExpr *node_expr = parse_expr(p, 0);
            if (node_expr) {
                node_stmt->data.let->expr = node_expr;
                parser_expect(p, TOKEN_SEMICOLON, "Expected ';'\n");
                return node_stmt;

            } else {
                fprintf(stderr, "Invalid expression\n");
                exit(EXIT_FAILURE);
            }
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
    NodeStmtListBuilder ll = {0};

    while (parser_peek(p, PEEK_CURRENT).type != TOKEN_EOF) {

        NodeStmt *stmt = parse_stmt(p);

        if (stmt) {

            NodeStmtList *node = arena_alloc(p->arena, sizeof(NodeStmtList));
            node->stmt = stmt;
            node->next = NULL;

            if (!ll.head) {
                ll.head = node;
            } else {
                ll.tail->next = node;
            }
            ll.tail = node;
            ll.size++;

        } else {
            fprintf(stderr, "Invalid statement\n");
            exit(EXIT_FAILURE);
        }
    }

    prog.stmts = ll.head;
    return prog;
}
