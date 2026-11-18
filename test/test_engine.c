/* Host-side engine tests: gcc -I../src test_engine.c ../src/engine.c -lm */
#include <stdio.h>
#include <string.h>
#include "engine.h"

static int fails = 0, total = 0;

static int map(char c)
{
    switch (c) {
    case '+': return K_ADD; case '-': return K_SUB; case '*': return K_MUL;
    case '/': return K_DIV; case '=': return K_EQUALS; case '.': return K_DECIMAL;
    case 'n': return K_NEGATE; case 'q': return K_SQRT; case '%': return K_PERCENT;
    case 'r': return K_RECIP; case 'b': return K_BACK; case 'e': return K_CE;
    case 'c': return K_C; case 'L': return K_MC; case 'R': return K_MR;
    case 'M': return K_MS; case 'P': return K_MPLUS;
    }
    if (c >= '0' && c <= '9') return c - '0';
    return -1;
}

static void t(const char *keys, char dec, const char *expect, int experr)
{
    CalcEngine e; char out[64]; const char *k;
    eng_init(&e);
    for (k = keys; *k; k++) if (*k != ' ') eng_key(&e, map(*k));
    eng_display(&e, dec, out, sizeof out);
    total++;
    if (e.err != experr || (experr == 0 && strcmp(out, expect) != 0)) {
        fails++;
        printf("FAIL  %-24s -> '%s' err=%d (expected '%s' err=%d)\n", keys, out, e.err, expect, experr);
    }
}

int main(void)
{
    t("", '.', "0.", 0);
    t("", ',', "0,", 0);
    t("12+3=", '.', "15.", 0);
    t("2+3*4=", '.', "20.", 0);          /* chained, no precedence */
    t("2+3=", '.', "5.", 0);
    t("2+3==", '.', "8.", 0);            /* '=' repeats */
    t("5+=", '.', "10.", 0);
    t("5+-3=", '.', "2.", 0);            /* operator replaced */
    t("1.5*2=", ',', "3,", 0);
    t("1.5", ',', "1,5", 0);
    t("0.1+0.2=", '.', "0.3", 0);
    t("1/0=", '.', "", 1);
    t("1/0=5+1=", '.', "6.", 0);         /* input resumes after an error */
    t("4q", '.', "2.", 0);
    t("4nq", '.', "", 2);
    t("0r", '.', "", 1);
    t("4r", '.', "0.25", 0);
    t("50+10%", '.', "5.", 0);
    t("50+10%=", '.', "55.", 0);
    t("10%", '.', "0.", 0);
    t("123b", '.', "12.", 0);
    t("1bb", '.', "0.", 0);
    t("12+34e5=", '.', "17.", 0);
    t("12+34c", '.', "0.", 0);
    t("5n", '.', "-5.", 0);
    t("5=n", '.', "-5.", 0);
    t("5M c R", '.', "5.", 0);
    t("5M 3P c R", '.', "8.", 0);
    t("5M L R", '.', "0.", 0);
    t("1234567890123456789", '.', "123456789012345.", 0);
    t("99999999*99999999=", '.', "9.9999998e+15", 0);
    t("10000000*10000000*10000000=", '.', "1.e+21", 0);
    t("2*3=7=", '.', "21.", 0);          /* '=' with a new number */
    t("1..2", '.', "1.2", 0);
    t("3*q", '.', "1.73205080756888", 0);
    printf("%d/%d tests OK\n", total - fails, total);
    return fails != 0;
}
