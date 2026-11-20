/*
 * sci.h - Scientific mode engine (Windows 95 behavior), free of Windows
 * dependencies. Operator precedence, parentheses, number bases with
 * word sizes, angle units, Inv/Hyp modifiers and statistics.
 */
#ifndef SCI_ENGINE_H
#define SCI_ENGINE_H

#include "engine.h"

#define SCI_MAXSTACK  64
#define SCI_MAXPAREN  25
#define SCI_MAXDATA   200
#define SCI_BUFLEN    64

#define ANG_DEG  0
#define ANG_RAD  1
#define ANG_GRAD 2

typedef struct {
    /* display / entry */
    double cur;
    char   entry[SCI_BUFLEN];   /* digits in the current base ('.' and 'e' in Dec) */
    int    entering;
    int    inExp;               /* typing the exponent after Exp */
    int    newOperand;
    int    err;

    /* modes */
    int    base;                /* 10, 16, 8 or 2 */
    int    wordBits;            /* 32, 16 or 8 (non-decimal bases) */
    int    angle;               /* ANG_DEG / ANG_RAD / ANG_GRAD */
    int    inv, hyp;            /* one-shot modifiers */
    int    fe;                  /* force scientific notation */

    /* expression stacks; OP 0 in ops[] marks an open parenthesis */
    double vals[SCI_MAXSTACK];
    int    nvals;
    int    ops[SCI_MAXSTACK];
    int    nops;
    int    parens;

    double mem;

    double data[SCI_MAXDATA];
    int    ndata;
} SciEngine;

void   sci_init(SciEngine *e);
void   sci_key(SciEngine *e, int key);       /* returns silently on invalid keys */
int    sci_key_valid(const SciEngine *e, int key);
double sci_value(const SciEngine *e);
void   sci_set_value(SciEngine *e, double v); /* used when switching views */
void   sci_display(const SciEngine *e, char decChar, char *out, int outlen);
void   sci_format(const SciEngine *e, double v, char decChar, char *out, int outlen);

/* Statistics box operations */
void   sci_stat_clear_one(SciEngine *e, int index);
void   sci_stat_clear_all(SciEngine *e);
void   sci_stat_load(SciEngine *e, int index);

#endif
