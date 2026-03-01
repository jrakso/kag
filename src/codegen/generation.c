#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "generation.h"

static void gen_expr(Generator *g, const NodeExpr *expr);

static void push(Generator *g, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    sb_append(&g->sb, "\tpush ");
    sb_append_fmtv(&g->sb, fmt, args);
    sb_append(&g->sb, "\n");
    va_end(args);
    g->stack_size++;
}

static void pop(Generator *g, const char *reg) {
    sb_append_fmt(&g->sb, "\tpop %s\n", reg);
    g->stack_size--;
}

void generator_init(Generator *g, const NodeProg *prog) {
    g->prog = prog;
    sb_init(&g->sb);
    g->stack_size = 0;
    var_table_init(&g->vars);
}

void generator_free(Generator *g) {
    sb_free(&g->sb);
    var_table_free(&g->vars);
}

static void gen_term(Generator *g, const NodeTerm *term) {
    switch (term->type) {
        
        case TERM_INT_LIT:
            sb_append_fmt(&g->sb, "\tmov rax, %s\n", term->data.int_lit->int_lit.value);
            push(g, "rax");
            break;

        case TERM_IDENT:
            if (!var_table_contains(&g->vars, term->data.ident->ident.value)) {
                fprintf(stderr, "Undeclared identifier: %s\n", term->data.ident->ident.value);
                exit(EXIT_FAILURE);
            }
            const Variable *var = var_table_find(&g->vars, term->data.ident->ident.value);
            push(g, "QWORD [rsp + %zu]", (g->stack_size - var->stack_loc - 1)*8);
            break;

        default: {
            break;
        }

    }
}

static void gen_bin_expr(Generator *g, const NodeBinExpr *bin_expr) {
    switch (bin_expr->type) {

        case BIN_ADD:
            gen_expr(g, bin_expr->data.add->rhs);
            gen_expr(g, bin_expr->data.add->lhs);
            pop(g, "rax");
            pop(g, "rbx");
            sb_append(&g->sb, "\tadd rax, rbx\n");
            push(g, "rax");
            break;

        case BIN_SUB:
            gen_expr(g, bin_expr->data.sub->rhs);
            gen_expr(g, bin_expr->data.sub->lhs);
            pop(g, "rax");
            pop(g, "rbx");
            sb_append(&g->sb, "\tsub rax, rbx\n");
            push(g, "rax");
            break;

        case BIN_MULTI:
            gen_expr(g, bin_expr->data.multi->rhs);
            gen_expr(g, bin_expr->data.multi->lhs);
            pop(g, "rax");
            pop(g, "rbx");
            sb_append(&g->sb, "\tmul rbx\n");
            push(g, "rax");
            break;

        case BIN_DIV:
            gen_expr(g, bin_expr->data.div->rhs);
            gen_expr(g, bin_expr->data.div->lhs);
            pop(g, "rax");
            pop(g, "rbx");
            sb_append(&g->sb, "\tdiv rbx\n");
            push(g, "rax");
            break;

        default: 
            break;

    }
}

static void gen_expr(Generator *g, const NodeExpr *expr) {
    switch (expr->type) {

        case EXPR_TERM:
            gen_term(g, expr->data.term);
            break;

        case EXPR_BIN:
            gen_bin_expr(g, expr->data.bin);
            break;

        default: 
            break;

    }

}

static void gen_stmt(Generator *g, const NodeStmt *stmt) {
    switch (stmt->type) {

        case STMT_EXIT:
            gen_expr(g, stmt->data.exit->expr);
            sb_append(&g->sb, "\tmov rax, 60\n");
            pop(g, "rdi");
            sb_append(&g->sb, "\tsyscall\n");
            break;

        case STMT_LET:
            if (var_table_contains(&g->vars, stmt->data.let->ident.value)) {
                fprintf(stderr, "Identifier already used: %s\n", stmt->data.let->ident.value);
                exit(EXIT_FAILURE);
            }
            var_table_append(&g->vars, stmt->data.let->ident.value, g->stack_size);
            gen_expr(g, stmt->data.let->expr);
            break;

        default:
            break;
    }
}

char *gen_prog(Generator *g) {
    sb_append(&g->sb, "global _start\n_start:\n");

    NodeStmtList *stmt = g->prog->stmts;
    while (stmt) {
        gen_stmt(g, stmt->stmt);
        stmt = stmt->next;
    }

    // sb_append(g->sb,
    //     "\tmov rax, 60\n"
    //     "\tmov rdi, 0\n"
    //     "\tsyscall\n");

    return g->sb.data;
}