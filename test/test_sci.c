/* Host-side Scientific engine tests: gcc -I../src test_sci.c ../src/sci.c -lm */
#include <stdio.h>
#include <string.h>
#include "sci.h"

static int fails = 0, total = 0;

/* One token per key, separated by spaces */
static int map(const char *t)
{
    static const struct { const char *n; int k; } m[] = {
        {"+",K_ADD},{"-",K_SUB},{"*",K_MUL},{"/",K_DIV},{"=",K_EQUALS},{".",K_DECIMAL},
        {"neg",K_NEGATE},{"back",K_BACK},{"ce",K_CE},{"c",K_C},{"(",K_LPAREN},{")",K_RPAREN},
        {"mod",K_MOD},{"and",K_AND},{"or",K_OR},{"xor",K_XOR},{"lsh",K_LSH},{"not",K_NOT},{"int",K_INT},
        {"sin",K_SIN},{"cos",K_COS},{"tan",K_TAN},{"ln",K_LN},{"log",K_LOG},{"pow",K_POW},
        {"sqr",K_SQR},{"cube",K_CUBE},{"recip",K_RECIP},{"fact",K_FACT},{"dms",K_DMS},{"pi",K_PI},
        {"exp",K_EXP},{"fe",K_FE},{"inv",K_INV},{"hyp",K_HYP},
        {"hex",K_HEX},{"dec",K_DECBASE},{"oct",K_OCT},{"bin",K_BIN},{"w0",K_W0},{"w1",K_W1},{"w2",K_W2},
        {"ms",K_MS},{"mr",K_MR},{"mc",K_MC},{"m+",K_MPLUS},{"dat",K_DAT},{"ave",K_AVE},{"sum",K_SUM},{"s",K_SDEV},
        {"A",K_A},{"B",K_B},{"C",K_HEXC},{"D",K_D},{"E",K_E},{"F",K_F}
    };
    unsigned i;
    if (strlen(t) == 1 && t[0] >= '0' && t[0] <= '9') return K_0 + (t[0] - '0');
    for (i = 0; i < sizeof m / sizeof m[0]; i++) if (strcmp(m[i].n, t) == 0) return m[i].k;
    printf("unknown token %s\n", t);
    return -1;
}

static void t(const char *keys, const char *expect, int experr)
{
    SciEngine e; char out[80], buf[256], *tok;
    sci_init(&e);
    strcpy(buf, keys);
    for (tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) sci_key(&e, map(tok));
    sci_display(&e, '.', out, sizeof out);
    total++;
    if (e.err != experr || (experr == 0 && strcmp(out, expect) != 0)) {
        fails++;
        printf("FAIL  %-40s -> '%s' err=%d (expected '%s' err=%d)\n", keys, out, e.err, expect, experr);
    }
}

int main(void)
{
    /* precedence and parentheses */
    t("2 + 3 * 4 =", "14.", 0);
    t("2 * 3 + 4 =", "10.", 0);
    t("2 + 3 * 4 +", "14.", 0);
    t("( 2 + 3 ) * 4 =", "20.", 0);
    t("2 * ( 3 + 4 =", "14.", 0);
    t("2 pow 3 pow 2 =", "512.", 0);          /* right associative */
    t("2 + 3 pow 2 * 2 =", "20.", 0);
    t("1 0 - 4 - 3 =", "3.", 0);
    t("5 + =", "10.", 0);
    t("5 + * 3 =", "15.", 0);
    t("1 / 0 =", "", 1);
    t("1 / 0 = 2 + 2 =", "4.", 0);
    /* functions */
    t("3 0 sin", "0.5", 0);
    t("9 0 cos", "0.", 0);
    t("inv 1 sin", "90.", 0);
    t("w1 pi cos", "-1.", 0);
    t("w2 1 0 0 sin", "1.", 0);
    t("1 0 0 log", "2.", 0);
    t("2 inv log", "100.", 0);
    t("1 ln", "0.", 0);
    t("5 fact", "120.", 0);
    t("4 sqr", "16.", 0);
    t("1 6 inv sqr", "4.", 0);
    t("2 7 inv cube", "3.", 0);
    t("8 inv pow 3 =", "2.", 0);
    t("4 recip", "0.25", 0);
    t("0 hyp sin", "0.", 0);
    t("1 . 5 dms", "1.3", 0);
    t("1 . 3 inv dms", "1.5", 0);
    t("3 . 7 5 int", "3.", 0);
    t("3 . 7 5 inv int", "0.75", 0);
    t("1 neg ln", "", 2);
    t("1 . 5 exp 3", "1.5e+3", 0);
    t("1 . 5 exp 3 =", "1500.", 0);
    t("1 . 5 exp 3 neg =", "0.0015", 0);
    t("1 2 3 fe", "1.23e+2", 0);
    t("pi", "3.14159265358979", 0);
    /* bases and logic */
    t("2 5 5 hex", "FF", 0);
    t("hex F F + 1 =", "100", 0);
    t("hex F F w2", "FF", 0);
    t("hex F F + 1 = w2", "0", 0);
    t("hex F F F F F F F F dec", "-1.", 0);
    t("1 0 bin", "1010", 0);
    t("6 and 3 =", "2.", 0);
    t("6 or 3 =", "7.", 0);
    t("6 xor 3 =", "5.", 0);
    t("1 lsh 4 =", "16.", 0);
    t("1 6 inv lsh 2 =", "4.", 0);
    t("0 not", "-1.", 0);
    t("hex 0 not", "FFFFFFFF", 0);
    t("1 7 mod 5 =", "2.", 0);
    t("1 + 2 * 3 and 4 =", "4.", 0);       /* (1+6) and 4 */
    t("oct 9", "0", 0);                     /* invalid digit ignored */
    t("hex 7 / 2 =", "3", 0);
    /* memory and stats */
    t("5 ms c mr", "5.", 0);
    t("1 dat 2 dat 3 dat ave", "2.", 0);
    t("1 dat 2 dat 3 dat sum", "6.", 0);
    t("2 dat 4 dat 4 dat 4 dat 5 dat 5 dat 7 dat 9 dat inv s", "2.", 0);
    t("1 dat 2 dat 3 dat inv sum", "14.", 0);
    t("ave", "", 1);
    printf("%d/%d tests OK\n", total - fails, total);
    return fails != 0;
}
