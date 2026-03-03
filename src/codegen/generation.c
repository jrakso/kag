#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "generation.h"
#include "vartable.h"

static void gen_expr(Generator *g, const NodeExpr *expr);
static void gen_stmt(Generator *g, const NodeStmt *stmt);

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

typedef struct Scope {
    VariableTable vars;
    size_t base_stack_size;
    struct Scope *parent;
} Scope;

static void begin_scope(Generator *g) {
    Scope *s = malloc(sizeof(Scope));
    if (!s) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    var_table_init(&s->vars);
    s->base_stack_size = g->stack_size;
    s->parent = g->current_scope;
    g->current_scope = s;
}

static void end_scope(Generator *g) {
    Scope *s = g->current_scope;
    size_t locals = g->stack_size - s->base_stack_size;
    if (locals > 0) {
        sb_append_fmt(&g->sb, "\tadd rsp, %zu\n", locals * 8);
        g->stack_size = s->base_stack_size;
    }
    g->current_scope = s->parent;
    var_table_free(&s->vars);
    free(s);
}

static char *create_label(size_t label_id) {
    int len = snprintf(NULL, 0, "label%zu", label_id);
    if (len < 0) {
        fprintf(stderr, "snprintf formatting error\n");
        exit(EXIT_FAILURE);
    }
    char *label = malloc(len + 1);
    if (!label) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(label, len + 1, "label%zu", label_id);
    return label;  // Caller frees
}

static const Variable *lookup_var(Generator *g, const char *name) {
    Scope *s = g->current_scope;
    while (s) {
        Variable *var = var_table_find(&s->vars, name);
        if (var)
            return var;
        s = s->parent;
    }
    return NULL;
}

void generator_init(Generator *g, const NodeProg *prog) {
    g->prog = prog;
    sb_init(&g->sb);
    g->stack_size = 0;
    g->label_count = 0;
    g->current_scope = NULL;  // Important
    begin_scope(g);
}

void generator_free(Generator *g) {
    while (g->current_scope) {
        end_scope(g);
    }
    sb_free(&g->sb);
}

static void gen_term(Generator *g, const NodeTerm *term) {
    switch (term->type) {      
        case TERM_INT_LIT: {
            sb_append_fmt(&g->sb, "\tmov rax, %s\n", term->data.int_lit->int_lit.value);
            push(g, "rax");
            break;
        }
        case TERM_IDENT: {
            const Variable *var = lookup_var(g, term->data.ident->ident.value);
            if (!var) {
                fprintf(stderr, "Undeclared identifier: %s\n", term->data.ident->ident.value);
                exit(EXIT_FAILURE);
            }
            push(g, "QWORD [rsp + %zu]", (g->stack_size - var->stack_loc - 1)*8);
            break;
        }
        case TERM_PAREN: {
            gen_expr(g, term->data.paren->expr);
            break;
        }
        default:
            break;
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

static void gen_scope(Generator *g, const NodeScope *scope) {
    begin_scope(g);
    for (size_t i = 0; i < scope->size; i++) {
        gen_stmt(g, scope->stmts[i]);
    }
    end_scope(g);
}

static void gen_if_pred(Generator *g, const NodeIfPred *pred, const char *end_label) {
    if (!pred) {
        return;
    }
    switch (pred->type) {
        case IFPRED_ELIF: {
            gen_expr(g, pred->data.elif->expr);
            pop(g, "rax");
            char *label = create_label(g->label_count++);
            sb_append(&g->sb, "\ttest rax, rax\n");
            sb_append_fmt(&g->sb, "\tjz %s\n", label);
            gen_scope(g, pred->data.elif->scope);
            sb_append_fmt(&g->sb, "\tjmp %s\n", end_label);
            if (pred->data.elif->pred) {
                sb_append_fmt(&g->sb, "%s:\n", label);
                gen_if_pred(g, pred->data.elif->pred, end_label);
            }
            free(label);
            break;
        }
        case IFPRED_ELSE: {
            gen_scope(g, pred->data.else_->scope);
            break;
        }
        default: {
            break;
        }
    } 
}

static void gen_stmt(Generator *g, const NodeStmt *stmt) {
    switch (stmt->type) {
        case STMT_EXIT: {
            gen_expr(g, stmt->data.exit->expr);
            sb_append(&g->sb, "\tmov rax, 60\n");
            pop(g, "rdi");
            sb_append(&g->sb, "\tsyscall\n");
            break;
        }
        case STMT_LET: {
            if (var_table_contains(&g->current_scope->vars, stmt->data.let->ident.value)) {
                fprintf(stderr, "Identifier already used: %s\n", stmt->data.let->ident.value);
                exit(EXIT_FAILURE);
            }
            var_table_append(&g->current_scope->vars, stmt->data.let->ident.value, g->stack_size);
            gen_expr(g, stmt->data.let->expr);
            break;
        }
        case STMT_SCOPE: {
            gen_scope(g, stmt->data.scope);
            break;
        }
        case STMT_IF: {
            char *label = create_label(g->label_count++);
            gen_expr(g, stmt->data.if_->expr);
            pop(g, "rax");
            sb_append(&g->sb, "\ttest rax, rax\n");
            sb_append_fmt(&g->sb, "\tjz %s\n", label);
            gen_scope(g, stmt->data.if_->scope);
            sb_append_fmt(&g->sb, "%s:\n", label);
            free(label);
            if (stmt->data.if_->pred) {
                char *end_label = create_label(g->label_count++);
                gen_if_pred(g, stmt->data.if_->pred, end_label);
                sb_append_fmt(&g->sb, "%s:\n", end_label);
                free(end_label);
            }
            break;
        }
        default: {
            break;
        }
    }
}

char *gen_prog(Generator *g) {
    sb_append(&g->sb, "global _start\n_start:\n");
    for (size_t i = 0; i < g->prog->size; i++) {
        gen_stmt(g, g->prog->stmts[i]);
    }
    // Default exit
    sb_append(&g->sb,
        "\tmov rax, 60\n"
        "\tmov rdi, 0\n"
        "\tsyscall\n");

    return g->sb.data;
}