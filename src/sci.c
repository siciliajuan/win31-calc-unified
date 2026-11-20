/*
 * sci.c - Windows 95-style Scientific calculator engine.
 *
 * Precedence (low to high): Or, Xor, And, Lsh/Rsh, + -, * / Mod, x^y.
 * In Hex/Oct/Bin every value is an integer wrapped to the word size
 * (two's complement, shown unsigned). Logical operators work on the
 * integer part in every base.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sci.h"

#define OP_MOD   5
#define OP_AND   6
#define OP_OR    7
#define OP_XOR   8
#define OP_LSH   9
#define OP_RSH  10
#define OP_POW  11
#define OP_ROOT 12
#define OP_PAREN (-1)

#define PI_VAL 3.14159265358979323846

static int bad(double x)
{
    return (x != x) || x > 1.0e308 || x < -1.0e308;
}

static int prec(int op)
{
    switch (op) {
    case OP_OR:  return 1;
    case OP_XOR: return 2;
    case OP_AND: return 3;
    case OP_LSH: case OP_RSH: return 4;
    case OP_ADD: case OP_SUB: return 5;
    case OP_MUL: case OP_DIV: case OP_MOD: return 6;
    case OP_POW: case OP_ROOT: return 7;
    }
    return 0;
}

/* ---------- integer helpers ---------- */

static double two_pow(int bits)
{
    return bits >= 32 ? 4294967296.0 : (double)(1UL << bits);
}

/* Truncate and wrap to a signed integer of 'bits' bits */
static double wrap(double v, int bits)
{
    double m = two_pow(bits);
    v = (v < 0) ? ceil(v) : floor(v);
    v = fmod(v, m);
    if (v < 0) v += m;
    if (v >= m / 2) v -= m;
    return v;
}

static long to_long(double v)
{
    return (long)wrap(v, 32);
}

static double norm(const SciEngine *e, double v)
{
    if (e->base != 10) return wrap(v, e->wordBits);
    return v;
}

/* ---------- state helpers ---------- */

static void set_error(SciEngine *e, int err)
{
    e->err = err;
    e->cur = 0.0;
    e->entering = 0;
    e->inExp = 0;
    e->newOperand = 0;
    e->nvals = e->nops = e->parens = 0;
    e->inv = e->hyp = 0;
    strcpy(e->entry, "0");
}

static void set_result(SciEngine *e, double v)
{
    if (bad(v)) { set_error(e, ERR_OVERFLOW); return; }
    v = norm(e, v);
    if (v == 0.0) v = 0.0;
    e->cur = v;
    e->entering = 0;
    e->inExp = 0;
}

void sci_init(SciEngine *e)
{
    memset(e, 0, sizeof(*e));
    e->base = 10;
    e->wordBits = 32;
    e->angle = ANG_DEG;
    strcpy(e->entry, "0");
}

static int digit_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static double parse_entry(const SciEngine *e)
{
    const char *p = e->entry;
    double v = 0.0, m;
    int neg = 0;

    if (e->base == 10) return atof(p);
    if (*p == '-') { neg = 1; p++; }
    for (; *p; p++) v = v * e->base + digit_value(*p);
    m = two_pow(e->wordBits);
    if (neg) v = -v;
    v = fmod(v, m);
    if (v < 0) v += m;
    if (v >= m / 2) v -= m;
    return v;
}

double sci_value(const SciEngine *e)
{
    return e->entering ? parse_entry(e) : e->cur;
}

void sci_set_value(SciEngine *e, double v)
{
    e->err = ERR_NONE;
    set_result(e, v);
    e->newOperand = 1;
}

static int max_digits(const SciEngine *e)
{
    switch (e->base) {
    case 16: return e->wordBits / 4;
    case 8:  return (e->wordBits + 2) / 3;
    case 2:  return e->wordBits;
    }
    return ENG_MAXDIGITS;
}

static int count_mantissa_digits(const char *s)
{
    int n = 0;
    for (; *s && *s != 'e'; s++)
        if (digit_value(*s) >= 0) n++;
    return n;
}

/* ---------- validity ---------- */

int sci_key_valid(const SciEngine *e, int key)
{
    int dec = (e->base == 10);

    if (key >= K_0 && key <= K_9) return key < e->base;
    if (key >= K_A && key <= K_F) return e->base == 16;
    switch (key) {
    case K_DECIMAL: case K_EXP: case K_FE: case K_DMS:
    case K_SIN: case K_COS: case K_TAN: case K_LN: case K_LOG:
        return dec;
    case K_LPAREN: return e->parens < SCI_MAXPAREN;
    case K_RPAREN: return e->parens > 0;
    }
    return 1;
}

/* ---------- binary operators ---------- */

static int apply_op(SciEngine *e, int op, double a, double b, double *r)
{
    long la, lb;

    switch (op) {
    case OP_ADD: *r = a + b; return 1;
    case OP_SUB: *r = a - b; return 1;
    case OP_MUL: *r = a * b; return 1;
    case OP_DIV:
        if (b == 0.0) { set_error(e, ERR_DIVZERO); return 0; }
        *r = a / b;
        if (e->base != 10) *r = (*r < 0) ? ceil(*r) : floor(*r);
        return 1;
    case OP_MOD:
        if (b == 0.0) { set_error(e, ERR_DIVZERO); return 0; }
        *r = fmod(a, b); return 1;
    case OP_POW:
        if (a < 0.0 && b != floor(b)) { set_error(e, ERR_INVALID); return 0; }
        if (a == 0.0 && b < 0.0) { set_error(e, ERR_DIVZERO); return 0; }
        *r = pow(a, b); return 1;
    case OP_ROOT:
        if (b == 0.0) { set_error(e, ERR_INVALID); return 0; }
        if (a < 0.0) {
            /* odd integer roots of negative numbers are real */
            if (b == floor(b) && fmod(fabs(b), 2.0) == 1.0) { *r = -pow(-a, 1.0 / b); return 1; }
            set_error(e, ERR_INVALID); return 0;
        }
        *r = pow(a, 1.0 / b); return 1;
    }
    la = to_long(a); lb = to_long(b);
    switch (op) {
    case OP_AND: *r = (double)(la & lb); break;
    case OP_OR:  *r = (double)(la | lb); break;
    case OP_XOR: *r = (double)(la ^ lb); break;
    case OP_LSH: *r = (lb >= 32 || lb <= -32) ? 0.0 : (lb >= 0 ? (double)(long)((unsigned long)la << lb) : (double)(la >> -lb)); break;
    case OP_RSH: *r = (lb >= 32 || lb <= -32) ? 0.0 : (lb >= 0 ? (double)(la >> lb) : (double)(long)((unsigned long)la << -lb)); break;
    default: *r = b; break;
    }
    return 1;
}

/* Pops one operator and applies it; returns 0 on error */
static int reduce_one(SciEngine *e)
{
    double a, b, r;
    int op = e->ops[--e->nops];
    b = e->vals[--e->nvals];
    a = e->vals[--e->nvals];
    if (!apply_op(e, op, a, b, &r)) return 0;
    if (bad(r)) { set_error(e, ERR_OVERFLOW); return 0; }
    e->vals[e->nvals++] = norm(e, r);
    return 1;
}

static void push_operand(SciEngine *e, double v)
{
    if (e->nvals < SCI_MAXSTACK) e->vals[e->nvals++] = v;
}

static void binary(SciEngine *e, int op)
{
    if (e->err) return;
    if (!e->newOperand && e->nops > 0 && e->ops[e->nops - 1] != OP_PAREN) {
        e->ops[e->nops - 1] = op;               /* replace the pending operator */
        return;
    }
    push_operand(e, sci_value(e));
    while (e->nops > 0 && e->ops[e->nops - 1] != OP_PAREN) {
        int top = e->ops[e->nops - 1];
        int right = (op == OP_POW || op == OP_ROOT);
        if (prec(top) > prec(op) || (!right && prec(top) == prec(op))) {
            if (!reduce_one(e)) return;
        } else break;
    }
    if (e->nops < SCI_MAXSTACK) e->ops[e->nops++] = op;
    set_result(e, e->vals[e->nvals - 1]);
    e->newOperand = 0;
}

static void close_paren(SciEngine *e)
{
    double v;
    if (e->err || e->parens == 0) return;
    push_operand(e, sci_value(e));
    while (e->nops > 0 && e->ops[e->nops - 1] != OP_PAREN)
        if (!reduce_one(e)) return;
    if (e->nops > 0) e->nops--;              /* drop the '(' marker */
    e->parens--;
    v = e->vals[--e->nvals];
    set_result(e, v);
    e->newOperand = 1;
}

static void equals(SciEngine *e)
{
    double v;
    if (e->err) return;
    if (!e->newOperand && e->nops > 0 && e->ops[e->nops - 1] != OP_PAREN)
        push_operand(e, e->vals[e->nvals - 1]);  /* "5 + =" -> 10 */
    else
        push_operand(e, sci_value(e));
    while (e->nops > 0) {
        if (e->ops[e->nops - 1] == OP_PAREN) { e->nops--; continue; }
        if (!reduce_one(e)) return;
    }
    v = e->vals[e->nvals - 1];
    e->nvals = 0;
    e->parens = 0;
    set_result(e, v);
    e->newOperand = 0;
}

/* ---------- unary functions ---------- */

static double to_rad(const SciEngine *e, double x)
{
    if (e->angle == ANG_DEG) return x * PI_VAL / 180.0;
    if (e->angle == ANG_GRAD) return x * PI_VAL / 200.0;
    return x;
}

static double from_rad(const SciEngine *e, double x)
{
    if (e->angle == ANG_DEG) return x * 180.0 / PI_VAL;
    if (e->angle == ANG_GRAD) return x * 200.0 / PI_VAL;
    return x;
}

/* Lanczos approximation of Gamma(x), for n! of non-integers */
static double gamma_fn(double x)
{
    static const double g[] = { 676.5203681218851, -1259.1392167224028,
        771.32342877765313, -176.61503916999185, 12.507343278686905,
        -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7 };
    double a, t;
    int i;
    if (x < 0.5) return PI_VAL / (sin(PI_VAL * x) * gamma_fn(1.0 - x));
    x -= 1.0;
    a = 0.99999999999980993;
    t = x + 7.5;
    for (i = 0; i < 8; i++) a += g[i] / (x + i + 1);
    return sqrt(2.0 * PI_VAL) * pow(t, x + 0.5) * exp(-t) * a;
}

static int unary(SciEngine *e, int key, double x, double *r)
{
    int inv = e->inv, hyp = e->hyp;
    double t;

    switch (key) {
    case K_SIN:
    case K_COS:
    case K_TAN:
        if (hyp) {
            if (!inv) {
                if (fabs(x) > 709.0) return ERR_OVERFLOW;
                *r = key == K_SIN ? sinh(x) : key == K_COS ? cosh(x) : tanh(x);
            } else if (key == K_SIN) {
                *r = log(x + sqrt(x * x + 1.0));
            } else if (key == K_COS) {
                if (x < 1.0) return ERR_INVALID;
                *r = log(x + sqrt(x * x - 1.0));
            } else {
                if (x <= -1.0 || x >= 1.0) return ERR_INVALID;
                *r = 0.5 * log((1.0 + x) / (1.0 - x));
            }
        } else if (!inv) {
            t = to_rad(e, x);
            if (key == K_TAN && fabs(cos(t)) < 1e-15) return ERR_INVALID;
            *r = key == K_SIN ? sin(t) : key == K_COS ? cos(t) : tan(t);
            if (fabs(*r) < 1e-15) *r = 0.0;
        } else {
            if (key != K_TAN && (x < -1.0 || x > 1.0)) return ERR_INVALID;
            *r = from_rad(e, key == K_SIN ? asin(x) : key == K_COS ? acos(x) : atan(x));
        }
        return ERR_NONE;
    case K_LN:
        if (inv) { if (x > 709.0) return ERR_OVERFLOW; *r = exp(x); }
        else { if (x <= 0.0) return ERR_INVALID; *r = log(x); }
        return ERR_NONE;
    case K_LOG:
        if (inv) { if (x > 308.0) return ERR_OVERFLOW; *r = pow(10.0, x); }
        else { if (x <= 0.0) return ERR_INVALID; *r = log10(x); }
        return ERR_NONE;
    case K_SQR:
        if (inv) { if (x < 0.0) return ERR_INVALID; *r = sqrt(x); }
        else *r = x * x;
        return ERR_NONE;
    case K_CUBE:
        if (inv) *r = (x < 0.0) ? -pow(-x, 1.0 / 3.0) : pow(x, 1.0 / 3.0);
        else *r = x * x * x;
        return ERR_NONE;
    case K_RECIP:
        if (x == 0.0) return ERR_DIVZERO;
        *r = 1.0 / x;
        return ERR_NONE;
    case K_FACT:
        if (x < 0.0 && x == floor(x)) return ERR_INVALID;
        if (x > 170.0) return ERR_OVERFLOW;
        if (x == floor(x)) {
            double f = 1.0;
            int i;
            for (i = 2; i <= (int)x; i++) f *= i;
            *r = f;
        } else {
            *r = gamma_fn(x + 1.0);
        }
        return ERR_NONE;
    case K_DMS:
        if (!inv) {   /* decimal degrees -> d.mmss */
            double a = fabs(x), d = floor(a), m, s;
            m = floor((a - d) * 60.0);
            s = ((a - d) * 60.0 - m) * 60.0;
            *r = d + m / 100.0 + s / 10000.0;
        } else {      /* d.mmss -> decimal degrees */
            double a = fabs(x), d = floor(a), mm, m, s;
            mm = (a - d) * 100.0;
            m = floor(mm + 1e-9);
            s = (mm - m) * 100.0;
            *r = d + m / 60.0 + s / 3600.0;
        }
        if (x < 0.0) *r = -*r;
        return ERR_NONE;
    case K_INT:
        t = (x < 0) ? ceil(x) : floor(x);
        *r = inv ? x - t : t;
        return ERR_NONE;
    case K_NOT:
        *r = (double)(~to_long(x));
        return ERR_NONE;
    case K_PERCENT:   /* not on the Scientific keypad; kept for paste */
        *r = x / 100.0;
        return ERR_NONE;
    }
    *r = x;
    return ERR_NONE;
}

/* ---------- statistics ---------- */

static double stat_value(SciEngine *e, int key, int *err)
{
    double sum = 0.0, sum2 = 0.0, mean;
    int i, n = e->ndata;
    for (i = 0; i < n; i++) { sum += e->data[i]; sum2 += e->data[i] * e->data[i]; }
    *err = ERR_NONE;
    switch (key) {
    case K_SUM: return e->inv ? sum2 : sum;
    case K_AVE:
        if (n == 0) { *err = ERR_DIVZERO; return 0.0; }
        return e->inv ? sum2 / n : sum / n;
    case K_SDEV:
        if (n == 0 || (!e->inv && n < 2)) { *err = ERR_DIVZERO; return 0.0; }
        mean = sum / n;
        {
            double var = (sum2 - n * mean * mean) / (e->inv ? n : n - 1);
            if (var < 0.0) var = 0.0;
            return sqrt(var);
        }
    }
    return 0.0;
}

void sci_stat_clear_one(SciEngine *e, int index)
{
    if (index < 0 || index >= e->ndata) return;
    memmove(&e->data[index], &e->data[index + 1], (size_t)(e->ndata - index - 1) * sizeof(double));
    e->ndata--;
}

void sci_stat_clear_all(SciEngine *e)
{
    e->ndata = 0;
}

void sci_stat_load(SciEngine *e, int index)
{
    if (index < 0 || index >= e->ndata) return;
    sci_set_value(e, e->data[index]);
}

/* ---------- base / mode changes ---------- */

static void set_base(SciEngine *e, int base)
{
    double v = sci_value(e);
    if (e->err) return;
    e->base = base;
    e->fe = (base == 10) ? e->fe : 0;
    set_result(e, v);
    e->newOperand = 1;
}

static void set_word(SciEngine *e, int which)
{
    double v;
    if (e->base == 10) { e->angle = which; return; }
    v = sci_value(e);
    e->wordBits = which == 0 ? 32 : which == 1 ? 16 : 8;
    set_result(e, v);
    e->newOperand = 1;
}

/* ---------- entry ---------- */

static void start_entry(SciEngine *e)
{
    if (e->err) e->err = ERR_NONE;
    if (!e->entering) {
        strcpy(e->entry, "0");
        e->entering = 1;
        e->inExp = 0;
    }
    e->newOperand = 1;
}

static void add_digit(SciEngine *e, int d)
{
    size_t len;
    char c = (char)(d < 10 ? '0' + d : 'A' + d - 10);

    start_entry(e);
    len = strlen(e->entry);
    if (e->inExp) {
        char *p = strchr(e->entry, 'e');
        if (strlen(p) >= 5) return;                 /* "e+ddd" */
        if (strcmp(p + 2, "0") == 0) { p[2] = c; return; }
        e->entry[len] = c; e->entry[len + 1] = '\0';
        return;
    }
    if (count_mantissa_digits(e->entry) >= max_digits(e)) return;
    if (strcmp(e->entry, "0") == 0) { e->entry[0] = c; return; }
    if (strcmp(e->entry, "-0") == 0) { e->entry[1] = c; return; }
    e->entry[len] = c; e->entry[len + 1] = '\0';
}

void sci_key(SciEngine *e, int key)
{
    double v, r;
    int err;
    size_t len;

    if (!sci_key_valid(e, key)) return;

    if (key >= K_0 && key <= K_9) { add_digit(e, key - K_0); return; }
    if (key >= K_A && key <= K_F) { add_digit(e, 10 + key - K_A); return; }

    switch (key) {
    case K_DECIMAL:
        start_entry(e);
        if (!e->inExp && strchr(e->entry, '.') == NULL) strcat(e->entry, ".");
        return;
    case K_EXP:
        start_entry(e);
        if (!e->inExp) { strcat(e->entry, "e+0"); e->inExp = 1; }
        return;
    case K_NEGATE:
        if (e->err) return;
        if (e->entering && e->inExp) {
            char *p = strchr(e->entry, 'e');
            p[1] = (p[1] == '+') ? '-' : '+';
        } else if (e->entering) {
            if (e->entry[0] == '-') memmove(e->entry, e->entry + 1, strlen(e->entry));
            else if (parse_entry(e) != 0.0 || strchr(e->entry, '.') != NULL) {
                memmove(e->entry + 1, e->entry, strlen(e->entry) + 1);
                e->entry[0] = '-';
            }
        } else {
            set_result(e, -e->cur);
            e->newOperand = 1;
        }
        return;
    case K_BACK:
        if (e->err || !e->entering) return;
        len = strlen(e->entry);
        if (e->inExp) {
            char *p = strchr(e->entry, 'e');
            if (strlen(p + 2) > 1) p[strlen(p) - 1] = '\0';     /* drop an exponent digit */
            else if (p[2] != '0') p[2] = '0';
            else { *p = '\0'; e->inExp = 0; }                    /* leave exponent entry */
        } else if (len > 0) {
            e->entry[len - 1] = '\0';
        }
        if (e->entry[0] == '\0' || strcmp(e->entry, "-") == 0) strcpy(e->entry, "0");
        return;
    case K_CE:
        e->err = ERR_NONE;
        strcpy(e->entry, "0");
        e->entering = 1; e->inExp = 0;
        e->newOperand = 1;
        return;
    case K_C:
        e->err = ERR_NONE;
        e->cur = 0.0; e->entering = 0; e->inExp = 0; e->newOperand = 0;
        e->nvals = e->nops = e->parens = 0;
        e->inv = e->hyp = 0;
        strcpy(e->entry, "0");
        return;

    case K_ADD: binary(e, OP_ADD); break;
    case K_SUB: binary(e, OP_SUB); break;
    case K_MUL: binary(e, OP_MUL); break;
    case K_DIV: binary(e, OP_DIV); break;
    case K_MOD: binary(e, OP_MOD); break;
    case K_AND: binary(e, OP_AND); break;
    case K_OR:  binary(e, OP_OR);  break;
    case K_XOR: binary(e, OP_XOR); break;
    case K_LSH: binary(e, e->inv ? OP_RSH : OP_LSH); break;
    case K_POW: binary(e, e->inv ? OP_ROOT : OP_POW); break;
    case K_EQUALS: equals(e); break;

    case K_LPAREN:
        if (e->err) return;
        if (e->nops < SCI_MAXSTACK) e->ops[e->nops++] = OP_PAREN;
        e->parens++;
        e->entering = 0;
        e->newOperand = 0;
        return;
    case K_RPAREN: close_paren(e); break;

    case K_SIN: case K_COS: case K_TAN: case K_LN: case K_LOG:
    case K_SQR: case K_CUBE: case K_RECIP: case K_FACT: case K_DMS:
    case K_INT: case K_NOT: case K_PERCENT:
        if (e->err) return;
        v = sci_value(e);
        err = unary(e, key, v, &r);
        if (err) { set_error(e, err); return; }
        set_result(e, r);
        e->newOperand = 1;
        break;

    case K_PI:
        if (e->err) e->err = ERR_NONE;
        set_result(e, e->inv ? 2.0 * PI_VAL : PI_VAL);
        e->newOperand = 1;
        break;

    case K_FE:
        if (e->entering) { e->cur = parse_entry(e); e->entering = 0; e->inExp = 0; }
        e->fe = !e->fe;
        return;
    case K_INV: e->inv = !e->inv; return;
    case K_HYP: e->hyp = !e->hyp; return;

    case K_HEX:     set_base(e, 16); return;
    case K_DECBASE: set_base(e, 10); return;
    case K_OCT:     set_base(e, 8);  return;
    case K_BIN:     set_base(e, 2);  return;
    case K_W0: set_word(e, 0); return;
    case K_W1: set_word(e, 1); return;
    case K_W2: set_word(e, 2); return;

    case K_MC: e->mem = 0.0; return;
    case K_MR:
        if (e->err) e->err = ERR_NONE;
        set_result(e, e->mem);
        e->newOperand = 1;
        return;
    case K_MS:
        if (e->err) return;
        e->mem = sci_value(e);
        e->cur = e->mem; e->entering = 0; e->inExp = 0;
        return;
    case K_MPLUS:
        if (e->err) return;
        v = sci_value(e);
        if (bad(e->mem + v)) { set_error(e, ERR_OVERFLOW); return; }
        e->mem = norm(e, e->mem + v);
        e->cur = v; e->entering = 0; e->inExp = 0;
        return;

    case K_DAT:
        if (e->err) return;
        v = sci_value(e);
        if (e->ndata < SCI_MAXDATA) e->data[e->ndata++] = v;
        e->cur = v; e->entering = 0; e->inExp = 0;
        e->newOperand = 0;
        return;
    case K_AVE: case K_SUM: case K_SDEV:
        if (e->err) return;
        v = stat_value(e, key, &err);
        if (err) { set_error(e, err); return; }
        set_result(e, v);
        e->newOperand = 1;
        break;
    case K_STA:
        return;   /* handled by the UI (opens the Statistics box) */
    }
    /* Inv and Hyp apply to a single operation */
    e->inv = 0;
    e->hyp = 0;
}

/* ---------- formatting ---------- */

static void fmt_dec(const SciEngine *e, double v, char *buf)
{
    char *p;
    if (e->fe) {
        char m[40];
        int ex;
        sprintf(m, "%.14e", v);
        p = strchr(m, 'e');
        ex = atoi(p + 1);
        *p = '\0';
        if (strchr(m, '.')) {               /* trim trailing zeros */
            char *q = m + strlen(m) - 1;
            while (*q == '0') *q-- = '\0';
        }
        sprintf(buf, "%se%c%d", m, ex < 0 ? '-' : '+', ex < 0 ? -ex : ex);
        return;
    }
    sprintf(buf, "%.15g", v);
    p = strchr(buf, 'e');
    if (p != NULL) {
        if (memchr(buf, '.', (size_t)(p - buf)) == NULL) {
            memmove(p + 1, p, strlen(p) + 1);
            *p = '.';
        }
    } else if (strchr(buf, '.') == NULL) {
        strcat(buf, ".");
    }
}

static void fmt_int(const SciEngine *e, double v, char *buf)
{
    double m = two_pow(e->wordBits), u = fmod(v, m);
    char tmp[40];
    int n = 0, i;
    if (u < 0) u += m;
    if (u == 0) { strcpy(buf, "0"); return; }
    while (u >= 1.0) {
        double q = floor(u / e->base);
        int d = (int)(u - q * e->base);
        tmp[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
        u = q;
    }
    for (i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = '\0';
}

void sci_format(const SciEngine *e, double v, char decChar, char *out, int outlen)
{
    char buf[64];
    char *p;
    if (e->base == 10) fmt_dec(e, v, buf); else fmt_int(e, v, buf);
    for (p = buf; *p; p++) if (*p == '.') *p = decChar;
    strncpy(out, buf, (size_t)outlen - 1);
    out[outlen - 1] = '\0';
}

void sci_display(const SciEngine *e, char decChar, char *out, int outlen)
{
    char buf[SCI_BUFLEN + 4];
    char *p;

    if (e->err) { out[0] = '\0'; return; }
    if (!e->entering) { sci_format(e, e->cur, decChar, out, outlen); return; }
    strcpy(buf, e->entry);
    if (e->base == 10) {
        p = strchr(buf, 'e');
        if (p == NULL) {
            if (strchr(buf, '.') == NULL) strcat(buf, ".");
        } else if (memchr(buf, '.', (size_t)(p - buf)) == NULL) {
            memmove(p + 1, p, strlen(p) + 1);
            *p = '.';
        }
    }
    for (p = buf; *p; p++) if (*p == '.') *p = decChar;
    strncpy(out, buf, (size_t)outlen - 1);
    out[outlen - 1] = '\0';
}
