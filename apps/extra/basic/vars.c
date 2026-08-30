#include "basic.h"

void ctl_error(BasicState *s, const char *msg)
{
    if (s->ctl.lineno)
        printf("\n?%s IN LINE %d\n", msg, s->ctl.lineno);
    else
        printf("\n?%s\n", msg);
    s->ctl.stopped = 1;
}

int var_aget(BasicState *s, int vn, int idx, int *v)
{
    if (s->var.dim[vn] == 0)
    {
        ctl_error(s, "UNDIMENSIONED ARRAY");
        return -1;
    }

    if (idx < 0 || idx >= s->var.dim[vn])
    {
        ctl_error(s, "SUBSCRIPT OUT OF RANGE");
        return -1;
    }
    *v = s->var.arr[vn][idx];
    return 0;
}

int var_aset(BasicState *s, int vn, int idx, int val)
{
    if (s->var.dim[vn] == 0)
    {
        ctl_error(s, "UNDIMENSIONED ARRAY");
        return -1;
    }

    if (idx < 0 || idx >= s->var.dim[vn])
    {
        ctl_error(s, "SUBSCRIPT OUT OF RANGE");
        return -1;
    }

    s->var.arr[vn][idx] = val;
    return 0;
}

int var_read_str(BasicState *s, char *buf)
{
    if (s->lex.type == T_STR)
    {
        strcpy(buf, s->lex.buf);
        lex_next(s);
        return 1;
    }

    if (s->lex.type == T_VAR && s->lex.quote)
    {
        strcpy(buf, s->var.str[s->lex.num]);
        lex_next(s);
        return 1;
    }

    return 0;
}
