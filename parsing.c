#include <stdio.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "mpc.h"

typedef struct lval {
    int type;

    long num;

    char* err;
    char* sym;

    int count;
    struct lval** cell;

} lval;

enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(sizeof(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_sexpr() {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr() {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM: break;
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;

        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            // also free the memory allocated to contain the pointers
            free(v->cell);
            break;
    }

    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    // array index grows from Zero
    v->cell[v->count-1] = x;
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    lval* x= NULL;
    if (strcmp(t->tag, ">")     == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr") == 0) { x = lval_qexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    switch (v->type) {
        case LVAL_NUM:
            printf("%li", v->num);
            break;
        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lval* lval_pop(lval* v, int i) {
    // find the item at "i"
    lval* x = v->cell[i];

    // shift the memory folling the item at "i" over the top of it
    memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));

    // decrease the count of items in the list
    v->count--;

    // reallocate the memory used
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval* builtin_op(lval* a, char* op) {
    // ensure all arguments are numbers
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operator on non number!");
        }
    }

    // pop the first element
    lval* x = lval_pop(a, 0);

    // if no arguments and sub then perform unary negation
    if ((strcmp(op, "-") == 0) && a->count == 0) { x->num = -x->num; }

    // while there are still elements remaining
    while (a->count > 0) {
        // pop the next element
        lval* y = lval_pop(a, 0);

        // perform operation
        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division By Zero");
                break;
            }
            x->num /= y->num;
        }

        // delete element now finished with
        lval_del(y);
    }

    // delete input expression and return result
    lval_del(a);
    return x;
}

lval* lval_eval(lval* v);

lval* lval_eval_sexpr(lval* v) {
    // evaluate children
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    // error checking
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    // empty expression
    if (v->count == 0) { return v; }

    // single expression
    if (v->count == 1) { return lval_take(v, 0); }

    // ensure first element is symbol
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression Does not start with symbol");
    }

    // call builtin with operator
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    // evaluate sexpressions
    if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
    // all other lval types remain the same
    return v;
}


int main(int argc, char** argv) {
    /* create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");


    // define them with the following language
    mpca_lang(MPC_LANG_DEFAULT,
        "                                                     \
            number   : /-?[0-9]+/;                            \
            symbol   : '+' | '-' | '*' | '/';                 \
            sexpr    : '(' + <expr>* ')' ;                    \
            qexpr    : '{' + <expr>* '}' ;                    \
            expr     : <number> | '(' <operator> <expr>+ ')'; \
            lispy    : /^/ <operator> <expr>+ /$/;            \
        ",
        Number, Symbol , Sexpr, Qexpr, Expr, Lispy);


    puts("Lispy version 0.0.0.1");
    puts("Press Ctrl+c to Exit.\n");

    // never ending loop
    while (1) {
        /* Output for prompt */
        char *input = readline("lispy> ");

        /* add input to history */
        add_history(input);

        /* Attemp to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            mpc_ast_print(r.output);

            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* free retrived input */
        free(input);
    }


    // undefine and delete our Parsers
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}
