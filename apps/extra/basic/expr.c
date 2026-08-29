#include "basic.h"

static int expr_err(BasicState *s)
{
    ctl_error(s, "SYNTAX ERROR");
    return 0;
}

static int expr_chk_sym(BasicState *s, char ch)
{
    if (s->lex.type != T_SYM || s->lex.buf[0] != ch)
    {
        ctl_error(s, "SYNTAX ERROR");
        return 0;
    }

    return 1;
}

static int prim(BasicState *s)
{
    if (s->lex.type == T_NUM)
    {
        int v = s->lex.num;

        lex_next(s);

        return v;
    }

    if (s->lex.type == T_VAR)
    {
        int vn = s->lex.num;

        int is_str = s->lex.quote;

        lex_next(s);

        if (s->lex.type == T_SYM && s->lex.buf[0] == '(')
        {
            if (is_str)
                return expr_err(s);

            int idx = expr_paren(s);

            if (s->ctl.stopped)
                return 0;

            int v;

            if (var_aget(s, vn, idx, &v) < 0)
                return 0;

            return v;
        }

        if (is_str)
            return expr_err(s);

        return s->var.val[vn];
    }

    if (s->lex.type == T_SYM && s->lex.buf[0] == '(')
        return expr_paren(s);

    if (s->lex.type == T_KEY)
    {
        int fn = s->lex.kw;

        lex_next(s);

        switch (fn)
        {
        case K_PEEK:
        case K_ABS:
        case K_SGN:
        case K_RND:
            break;
        default:
            exec_syntax_err(s);
            return 0;
        }

        int a = expr_paren(s);

        if (s->ctl.stopped)
            return 0;

        switch (fn)
        {
        case K_PEEK:
            return (*(volatile uint8_t *)a);
        case K_ABS:
            return a < 0 ? -a : a;
        case K_SGN:
            return a < 0 ? -1 : a > 0 ? 1 : 0;
        case K_RND:
        {
            int r = rand();

            return a > 0 ? (r % a) + 1 : 0;
        }
        }
    }

    if (s->lex.type == T_FN)
    {
        int fn_idx = s->lex.num;

        lex_next(s);

        int a = expr_paren(s);

        if (s->ctl.stopped)
            return 0;

        if (s->fn.param[fn_idx] < 0)
        {
            ctl_error(s, "UNDEFINED FUNCTION");
            return 0;
        }

        BasicLex saved_lex = s->lex;

        int pv = s->fn.param[fn_idx];

        int old_val = s->var.val[pv];

        s->var.val[pv] = a;

        s->lex.p = s->fn.body[fn_idx];

        lex_next(s);

        int r = expr_eval(s);

        s->var.val[pv] = old_val;

        /* Restore the whole lexer state: the body evaluation overwrote
         * lex.buf as well as p/type, and callers dispatch on buf[0]. */
        s->lex = saved_lex;

        return r;
    }

    return expr_err(s);
}

static int unary(BasicState *s)
{
    if (s->lex.type == T_SYM && s->lex.buf[0] == '-')
    {
        lex_next(s);
        return -unary(s);
    }

    if (s->lex.type == T_SYM && s->lex.buf[0] == '+')
    {
        lex_next(s);
        return unary(s);
    }

    return prim(s);
}

static int mul(BasicState *s)
{
    int v = unary(s);

    while (s->lex.type == T_SYM && (s->lex.buf[0] == '*' || s->lex.buf[0] == '/'))
    {
        int op = s->lex.buf[0];

        lex_next(s);

        int r = unary(s);

        if (op == '*')
            v *= r;
        else if (r)
            v /= r;
        else
        {
            ctl_error(s, "DIVISION BY ZERO");
            v = 0;
        }
    }

    return v;
}

static int add(BasicState *s)
{
    int v = mul(s);

    while (s->lex.type == T_SYM && (s->lex.buf[0] == '+' || s->lex.buf[0] == '-'))
    {
        int op = s->lex.buf[0];

        lex_next(s);

        if (op == '+')
            v += mul(s);
        else
            v -= mul(s);
    }

    return v;
}

static int rel(BasicState *s)
{
    int v = add(s);

    if (s->lex.type == T_SYM &&
        (s->lex.buf[0] == '=' || s->lex.buf[0] == '<' || s->lex.buf[0] == '>'))
    {
        char op0 = s->lex.buf[0], op1 = s->lex.buf[1];

        lex_next(s);

        int r = add(s);

        if (op0 == '=' && !op1)
            return v == r;
        if (op0 == '<' && !op1)
            return v < r;
        if (op0 == '>' && !op1)
            return v > r;
        if (op0 == '<' && op1 == '=')
            return v <= r;
        if (op0 == '>' && op1 == '=')
            return v >= r;
        if (op0 == '<' && op1 == '>')
            return v != r;
    }

    return v;
}

static int logical(BasicState *s)
{
    int v = rel(s);

    for (;;)
    {
        if (s->lex.type == T_KEY && s->lex.kw == K_AND)
        {
            lex_next(s);

            int r = rel(s);

            v = v && r;
        }
        else if (s->lex.type == T_KEY && s->lex.kw == K_OR)
        {
            lex_next(s);

            int r = rel(s);

            v = v || r;
        }
        else
            break;
    }

    return v;
}

int expr_eval(BasicState *s)
{
    return logical(s);
}

int expr_paren(BasicState *s)
{
    lex_next(s);

    int v = expr_eval(s);

    if (s->ctl.stopped)
        return 0;

    if (!expr_chk_sym(s, ')'))
        return 0;

    lex_next(s);

    return v;
}