/*
 * engine.h - Calculator engine (Standard mode), free of Windows dependencies.
 * Builds with Open Watcom (Win16) as well as with gcc (host-side tests).
 */
#ifndef ENGINE_H
#define ENGINE_H

#define ENG_MAXDIGITS 15   /* significant digits a double can hold */
#define ENG_BUFLEN    48

/* Binary operators */
#define OP_NONE 0
#define OP_ADD  1
#define OP_SUB  2
#define OP_MUL  3
#define OP_DIV  4

/* Errors */
#define ERR_NONE     0
#define ERR_DIVZERO  1
#define ERR_INVALID  2
#define ERR_OVERFLOW 3

/* Logical keys */
enum {
    K_0 = 0, K_1, K_2, K_3, K_4, K_5, K_6, K_7, K_8, K_9,
    K_DECIMAL, K_NEGATE,
    K_ADD, K_SUB, K_MUL, K_DIV, K_EQUALS,
    K_SQRT, K_PERCENT, K_RECIP,
    K_BACK, K_CE, K_C,
    K_MC, K_MR, K_MS, K_MPLUS,
    K_COUNT
};

typedef struct {
    double acc;          /* accumulator (left operand)                 */
    double cur;          /* displayed value when not typing            */
    char   entry[ENG_BUFLEN]; /* number being typed ('.' internally) */
    int    entering;     /* 1 = the display shows 'entry'               */
    int    newOperand;   /* 1 = a new operand follows the last operator */
    int    pendOp;       /* pending operator                            */
    int    lastOp;       /* used to repeat '='                          */
    double lastOperand;
    double mem;
    int    err;
} CalcEngine;

void   eng_init(CalcEngine *e);
void   eng_key(CalcEngine *e, int key);
double eng_value(const CalcEngine *e);
int    eng_has_memory(const CalcEngine *e);
/* Display text. On error returns "" and the caller shows the message. */
void   eng_display(const CalcEngine *e, char decChar, char *out, int outlen);

#endif
