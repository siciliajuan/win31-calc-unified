/*
 * engine.c - Windows 95-style Standard calculator logic:
 * left-to-right chained evaluation, repeatable '=',
 * percentage relative to the accumulator, MC/MR/MS/M+ memory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "engine.h"

static int bad(double x)
{
    return (x != x) || x > 1.0e308 || x < -1.0e308;
}

static void set_error(CalcEngine *e, int err)
{
    e->err = err;
    e->acc = 0.0;
    e->cur = 0.0;
    e->pendOp = OP_NONE;
    e->lastOp = OP_NONE;
    e->entering = 0;
    e->newOperand = 0;
    strcpy(e->entry, "0");
}

static void set_result(CalcEngine *e, double x)
{
    if (bad(x)) { set_error(e, ERR_OVERFLOW); return; }
    if (x == 0.0) x = 0.0;          /* drop -0 */
    e->cur = x;
    e->entering = 0;
}

void eng_init(CalcEngine *e)
{
    memset(e, 0, sizeof(*e));
    strcpy(e->entry, "0");
}

double eng_value(const CalcEngine *e)
{
    if (e->entering) return atof(e->entry);
    return e->cur;
}

int eng_has_memory(const CalcEngine *e)
{
    return e->mem != 0.0;
}

static int count_digits(const char *s)
{
    int n = 0;
    for (; *s; s++) if (*s >= '0' && *s <= '9') n++;
    return n;
}

static void start_entry(CalcEngine *e)
{
    if (e->err) e->err = ERR_NONE;
    if (!e->entering) {
        strcpy(e->entry, "0");
        e->entering = 1;
    }
    e->newOperand = 1;
}

/* Returns 0 on error (division by zero) */
static int apply(CalcEngine *e, int op, double a, double b, double *res)
{
    switch (op) {
    case OP_ADD: *res = a + b; break;
    case OP_SUB: *res = a - b; break;
    case OP_MUL: *res = a * b; break;
    case OP_DIV:
        if (b == 0.0) { set_error(e, ERR_DIVZERO); return 0; }
        *res = a / b; break;
    default: *res = b; break;
    }
    return 1;
}

static void binary_op(CalcEngine *e, int op)
{
    double v, r;
    if (e->err) return;
    if (!e->newOperand && e->pendOp != OP_NONE) {
        e->pendOp = op;                 /* repeated operator: replace it */
        return;
    }
    v = eng_value(e);
    if (e->pendOp != OP_NONE) {
        if (!apply(e, e->pendOp, e->acc, v, &r)) return;
        set_result(e, r);
        if (e->err) return;
        e->acc = r;
    } else {
        e->acc = v;
        set_result(e, v);
    }
    e->pendOp = op;
    e->lastOp = OP_NONE;
    e->newOperand = 0;
}

static void equals(CalcEngine *e)
{
    double v, r;
    if (e->err) return;
    v = eng_value(e);
    if (e->pendOp != OP_NONE) {
        if (!e->newOperand) v = e->acc;          /* "5 + =" -> 10 */
        e->lastOp = e->pendOp;
        e->lastOperand = v;
        if (!apply(e, e->pendOp, e->acc, v, &r)) return;
        e->pendOp = OP_NONE;
    } else if (e->lastOp != OP_NONE) {
        if (!apply(e, e->lastOp, v, e->lastOperand, &r)) return;
    } else {
        r = v;
    }
    set_result(e, r);
    if (e->err) return;
    e->acc = r;
    e->newOperand = 0;
}

static void unary(CalcEngine *e, int key)
{
    double v, r;
    if (e->err) return;
    v = eng_value(e);
    switch (key) {
    case K_SQRT:
        if (v < 0.0) { set_error(e, ERR_INVALID); return; }
        r = sqrt(v); break;
    case K_RECIP:
        if (v == 0.0) { set_error(e, ERR_DIVZERO); return; }
        r = 1.0 / v; break;
    case K_PERCENT:
        r = (e->pendOp != OP_NONE) ? e->acc * v / 100.0 : 0.0; break;
    default:
        return;
    }
    set_result(e, r);
    e->newOperand = 1;
}

void eng_key(CalcEngine *e, int key)
{
    size_t len;

    if (key >= K_0 && key <= K_9) {
        start_entry(e);
        if (count_digits(e->entry) >= ENG_MAXDIGITS) return;
        if (strcmp(e->entry, "0") == 0) e->entry[0] = (char)('0' + key);
        else if (strcmp(e->entry, "-0") == 0) e->entry[1] = (char)('0' + key);
        else {
            len = strlen(e->entry);
            e->entry[len] = (char)('0' + key);
            e->entry[len + 1] = '\0';
        }
        return;
    }

    switch (key) {
    case K_DECIMAL:
        start_entry(e);
        if (strchr(e->entry, '.') == NULL && count_digits(e->entry) < ENG_MAXDIGITS)
            strcat(e->entry, ".");
        break;

    case K_NEGATE:
        if (e->err) return;
        if (e->entering) {
            if (e->entry[0] == '-') memmove(e->entry, e->entry + 1, strlen(e->entry));
            else if (atof(e->entry) != 0.0 || strchr(e->entry, '.') != NULL) {
                memmove(e->entry + 1, e->entry, strlen(e->entry) + 1);
                e->entry[0] = '-';
            }
        } else {
            set_result(e, -e->cur);
            e->newOperand = 1;
        }
        break;

    case K_ADD: binary_op(e, OP_ADD); break;
    case K_SUB: binary_op(e, OP_SUB); break;
    case K_MUL: binary_op(e, OP_MUL); break;
    case K_DIV: binary_op(e, OP_DIV); break;
    case K_EQUALS: equals(e); break;

    case K_SQRT: case K_RECIP: case K_PERCENT:
        unary(e, key); break;

    case K_BACK:
        if (e->err || !e->entering) return;
        len = strlen(e->entry);
        if (len > 0) e->entry[len - 1] = '\0';
        if (e->entry[0] == '\0' || strcmp(e->entry, "-") == 0) strcpy(e->entry, "0");
        break;

    case K_CE:
        e->err = ERR_NONE;
        strcpy(e->entry, "0");
        e->entering = 1;
        e->newOperand = 1;
        break;

    case K_C:
        {
            double m = e->mem;
            eng_init(e);
            e->mem = m;
        }
        break;

    case K_MC: e->mem = 0.0; break;
    case K_MR:
        if (e->err) e->err = ERR_NONE;
        set_result(e, e->mem);
        e->newOperand = 1;
        break;
    case K_MS:
        if (e->err) return;
        e->mem = eng_value(e);
        e->entering = 0; e->cur = e->mem;
        break;
    case K_MPLUS:
        if (e->err) return;
        {
            double v = eng_value(e);
            double m = e->mem + v;
            if (bad(m)) { set_error(e, ERR_OVERFLOW); return; }
            e->mem = m;
            e->entering = 0; e->cur = v;
        }
        break;
    }
}

void eng_display(const CalcEngine *e, char decChar, char *out, int outlen)
{
    char buf[ENG_BUFLEN + 8];
    char *p;

    if (e->err) { out[0] = '\0'; return; }

    if (e->entering) {
        strcpy(buf, e->entry);
        if (strchr(buf, '.') == NULL) strcat(buf, ".");
    } else {
        sprintf(buf, "%.15g", e->cur);
        p = strchr(buf, 'e');
        if (p != NULL) {
            /* "1e+16" -> "1.e+16" */
            if (memchr(buf, '.', (size_t)(p - buf)) == NULL) {
                memmove(p + 1, p, strlen(p) + 1);
                *p = '.';
            }
        } else if (strchr(buf, '.') == NULL) {
            strcat(buf, ".");
        }
    }
    for (p = buf; *p; p++) if (*p == '.') *p = decChar;
    strncpy(out, buf, (size_t)outlen - 1);
    out[outlen - 1] = '\0';
}
