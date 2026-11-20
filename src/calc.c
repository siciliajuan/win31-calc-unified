/*
 * calc.c - Calculator with a Windows 95-style interface for Windows 3.1 (Win16).
 * Standard and Scientific views, English/Spanish UI (Spanish when
 * WIN.INI [intl] sLanguage starts with "es"), decimal separator from
 * [intl] sDecimal. The last view is kept in WIN.INI [SciCalc] layout
 * (0 = Scientific, 1 = Standard), like the Windows 95 Calculator.
 *
 * The window frame and menu bar are the native Windows 3.1 ones; the
 * client area is custom-painted using the 16-color VGA palette.
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "engine.h"
#include "sci.h"
#include "resource.h"

/* ---------- VGA palette ---------- */
#define C_FACE    RGB(192,192,192)
#define C_SHADOW  RGB(128,128,128)
#define C_LIGHT   RGB(255,255,255)
#define C_DARK    RGB(0,0,0)
#define C_RED     RGB(255,0,0)
#define C_BLUE    RGB(0,0,255)
#define C_NAVY    RGB(0,0,128)
#define C_MAROON  RGB(128,0,0)
#define C_PURPLE  RGB(128,0,128)

typedef struct {
    int key;
    int x, y, w, h;
    const char *label;  /* NULL = localized Backspace label or decimal separator */
    COLORREF color;
} Button;

/* ---------- Standard view ----------
 * Measured against a Windows 95 Calculator screenshot (96 dpi):
 * 13 px top / 9 px left / 11 px right / 12 px bottom padding, 36x28 keys on a
 * 40 x 34 px grid, Back/CE/C at 48-49 px wide. */
#define STD_W 268
#define STD_H 221
#define X0 9

static Button stdButtons[] = {
    { K_BACK,  X0+ 93,  45, 48, 28, NULL,   C_MAROON },
    { K_CE,    X0+146,  45, 48, 28, "CE",   C_MAROON },
    { K_C,     X0+199,  45, 49, 28, "C",    C_MAROON },

    { K_MC,    X0,      79, 36, 28, "MC",   C_RED },
    { K_MR,    X0,     113, 36, 28, "MR",   C_RED },
    { K_MS,    X0,     147, 36, 28, "MS",   C_RED },
    { K_MPLUS, X0,     181, 36, 28, "M+",   C_RED },

    { K_7,      X0+ 52,  79, 36, 28, "7",    C_BLUE },
    { K_8,      X0+ 92,  79, 36, 28, "8",    C_BLUE },
    { K_9,      X0+132,  79, 36, 28, "9",    C_BLUE },
    { K_DIV,    X0+172,  79, 36, 28, "/",    C_RED  },
    { K_SQRT,   X0+212,  79, 36, 28, "sqrt", C_NAVY },

    { K_4,      X0+ 52, 113, 36, 28, "4",    C_BLUE },
    { K_5,      X0+ 92, 113, 36, 28, "5",    C_BLUE },
    { K_6,      X0+132, 113, 36, 28, "6",    C_BLUE },
    { K_MUL,    X0+172, 113, 36, 28, "*",    C_RED  },
    { K_PERCENT,X0+212, 113, 36, 28, "%",    C_NAVY },

    { K_1,      X0+ 52, 147, 36, 28, "1",    C_BLUE },
    { K_2,      X0+ 92, 147, 36, 28, "2",    C_BLUE },
    { K_3,      X0+132, 147, 36, 28, "3",    C_BLUE },
    { K_SUB,    X0+172, 147, 36, 28, "-",    C_RED  },
    { K_RECIP,  X0+212, 147, 36, 28, "1/x",  C_NAVY },

    { K_0,      X0+ 52, 181, 36, 28, "0",    C_BLUE },
    { K_NEGATE, X0+ 92, 181, 36, 28, "+/-",  C_BLUE },
    { K_DECIMAL,X0+132, 181, 36, 28, NULL,   C_BLUE },
    { K_ADD,    X0+172, 181, 36, 28, "+",    C_RED  },
    { K_EQUALS, X0+212, 181, 36, 28, "=",    C_RED  },
};
#define NSTD (sizeof(stdButtons) / sizeof(stdButtons[0]))

static const RECT rcStdDisplay = { X0, 13, X0 + 248, 39 };
static const RECT rcStdMemBox  = { X0, 45, X0 + 36, 71 };

/* ---------- Scientific view ----------
 * Measured against a Windows 95 Calculator (Scientific) screenshot:
 * key groups 16 px apart, same 40 x 34 grid, display over the digit block. */
#define SCIVIEW_W 493
#define SCIVIEW_H 289
#define SX 10
#define R0 82                 /* Back/CE/C row */
#define R1 116
#define R2 150
#define R3 184
#define R4 218
#define R5 252
#define CF 52                 /* function block */
#define CM 184                /* memory column */
#define CD 236                /* digit block */

static Button sciButtons[] = {
    { K_BACK,  SX+317, R0, 48, 28, NULL,  C_MAROON },
    { K_CE,    SX+370, R0, 48, 28, "CE",  C_MAROON },
    { K_C,     SX+423, R0, 49, 28, "C",   C_MAROON },

    { K_STA,   SX,     R1, 36, 28, "Sta", C_NAVY },
    { K_AVE,   SX,     R2, 36, 28, "Ave", C_NAVY },
    { K_SUM,   SX,     R3, 36, 28, "Sum", C_NAVY },
    { K_SDEV,  SX,     R4, 36, 28, "s",   C_NAVY },
    { K_DAT,   SX,     R5, 36, 28, "Dat", C_NAVY },

    { K_FE,    SX+CF,    R1, 36, 28, "F-E", C_PURPLE },
    { K_LPAREN,SX+CF+40, R1, 36, 28, "(",   C_PURPLE },
    { K_RPAREN,SX+CF+80, R1, 36, 28, ")",   C_PURPLE },
    { K_DMS,   SX+CF,    R2, 36, 28, "dms", C_PURPLE },
    { K_EXP,   SX+CF+40, R2, 36, 28, "Exp", C_PURPLE },
    { K_LN,    SX+CF+80, R2, 36, 28, "ln",  C_PURPLE },
    { K_SIN,   SX+CF,    R3, 36, 28, "sin", C_PURPLE },
    { K_POW,   SX+CF+40, R3, 36, 28, "x^y", C_PURPLE },
    { K_LOG,   SX+CF+80, R3, 36, 28, "log", C_PURPLE },
    { K_COS,   SX+CF,    R4, 36, 28, "cos", C_PURPLE },
    { K_CUBE,  SX+CF+40, R4, 36, 28, "x^3", C_PURPLE },
    { K_FACT,  SX+CF+80, R4, 36, 28, "n!",  C_PURPLE },
    { K_TAN,   SX+CF,    R5, 36, 28, "tan", C_PURPLE },
    { K_SQR,   SX+CF+40, R5, 36, 28, "x^2", C_PURPLE },
    { K_RECIP, SX+CF+80, R5, 36, 28, "1/x", C_PURPLE },

    { K_MC,    SX+CM, R1, 36, 28, "MC", C_RED },
    { K_MR,    SX+CM, R2, 36, 28, "MR", C_RED },
    { K_MS,    SX+CM, R3, 36, 28, "MS", C_RED },
    { K_MPLUS, SX+CM, R4, 36, 28, "M+", C_RED },
    { K_PI,    SX+CM, R5, 36, 28, "PI", C_NAVY },

    { K_7,      SX+CD,     R1, 36, 28, "7",   C_BLUE },
    { K_8,      SX+CD+40,  R1, 36, 28, "8",   C_BLUE },
    { K_9,      SX+CD+80,  R1, 36, 28, "9",   C_BLUE },
    { K_DIV,    SX+CD+120, R1, 36, 28, "/",   C_RED },
    { K_MOD,    SX+CD+160, R1, 36, 28, "Mod", C_RED },
    { K_AND,    SX+CD+200, R1, 36, 28, "And", C_RED },
    { K_4,      SX+CD,     R2, 36, 28, "4",   C_BLUE },
    { K_5,      SX+CD+40,  R2, 36, 28, "5",   C_BLUE },
    { K_6,      SX+CD+80,  R2, 36, 28, "6",   C_BLUE },
    { K_MUL,    SX+CD+120, R2, 36, 28, "*",   C_RED },
    { K_OR,     SX+CD+160, R2, 36, 28, "Or",  C_RED },
    { K_XOR,    SX+CD+200, R2, 36, 28, "Xor", C_RED },
    { K_1,      SX+CD,     R3, 36, 28, "1",   C_BLUE },
    { K_2,      SX+CD+40,  R3, 36, 28, "2",   C_BLUE },
    { K_3,      SX+CD+80,  R3, 36, 28, "3",   C_BLUE },
    { K_SUB,    SX+CD+120, R3, 36, 28, "-",   C_RED },
    { K_LSH,    SX+CD+160, R3, 36, 28, "Lsh", C_RED },
    { K_NOT,    SX+CD+200, R3, 36, 28, "Not", C_RED },
    { K_0,      SX+CD,     R4, 36, 28, "0",   C_BLUE },
    { K_NEGATE, SX+CD+40,  R4, 36, 28, "+/-", C_BLUE },
    { K_DECIMAL,SX+CD+80,  R4, 36, 28, NULL,  C_BLUE },
    { K_ADD,    SX+CD+120, R4, 36, 28, "+",   C_RED },
    { K_EQUALS, SX+CD+160, R4, 36, 28, "=",   C_RED },
    { K_INT,    SX+CD+200, R4, 36, 28, "Int", C_RED },
    { K_A,      SX+CD,     R5, 36, 28, "A",   C_NAVY },
    { K_B,      SX+CD+40,  R5, 36, 28, "B",   C_NAVY },
    { K_HEXC,   SX+CD+80,  R5, 36, 28, "C",   C_NAVY },
    { K_D,      SX+CD+120, R5, 36, 28, "D",   C_NAVY },
    { K_E,      SX+CD+160, R5, 36, 28, "E",   C_NAVY },
    { K_F,      SX+CD+200, R5, 36, 28, "F",   C_NAVY },
};
#define NSCI (sizeof(sciButtons) / sizeof(sciButtons[0]))

static const RECT rcSciDisplay  = { SX+CD, 10, SX+CD+236, 36 };
static const RECT rcBaseGroup   = { SX,     49, SX+259, 77 };
static const RECT rcWordGroup   = { SX+265, 49, SX+472, 77 };
static const RECT rcInvGroup    = { SX,     R0, SX+127, R0+27 };
static const RECT rcParenBox    = { SX+132, R0, SX+168, R0+27 };
static const RECT rcSciMemBox   = { SX+CM,  R0, SX+CM+36, R0+27 };

/* Radio buttons and check boxes (label text drawn 18 px to the right) */
#define CTL_RADIO 0
#define CTL_CHECK 1
typedef struct {
    int type, key, x, y, ids;   /* ids: label string, 0 = depends on the base */
} Control;

static Control sciControls[] = {
    { CTL_RADIO, K_HEX,     SX+10,  57, IDS_HEX },
    { CTL_RADIO, K_DECBASE, SX+71,  57, IDS_DEC },
    { CTL_RADIO, K_OCT,     SX+133, 57, IDS_OCT },
    { CTL_RADIO, K_BIN,     SX+191, 57, IDS_BIN },
    { CTL_RADIO, K_W0,      SX+275, 57, 0 },
    { CTL_RADIO, K_W1,      SX+341, 57, 0 },
    { CTL_RADIO, K_W2,      SX+410, 57, 0 },
    { CTL_CHECK, K_INV,     SX+10,  R0+7, IDS_INV },
    { CTL_CHECK, K_HYP,     SX+70,  R0+7, IDS_HYP },
};
#define NCTL (sizeof(sciControls) / sizeof(sciControls[0]))
#define CTL_HIT_W 56

/* ---------- Global state ---------- */
static HINSTANCE  g_hInst;
static HWND       g_hwnd;
static CalcEngine g_eng;
static SciEngine  g_sci;
static int        g_isSci;
static Button    *g_buttons;
static int        g_nButtons;
static HFONT      g_font;      /* display and labels: MS Sans Serif 8 pt */
static HFONT      g_btnFont;   /* keys: MS Sans Serif 10 pt bold */
static HBRUSH     g_brFace, g_brShadow, g_brLight, g_brDark;
static int        g_strBase = STR_BASE_EN;
static char       g_dec = '.';
static char       g_decLabel[2] = ".";
static char       g_thousand = ',';
static char       g_backLabel[24];
static int        g_pressed = -1;     /* index of the button pressed with the mouse */
static int        g_pressedIn = 0;    /* the cursor is still over it */
static HWND       g_statBox = NULL;
static int        g_statUsed;      /* the Statistics box has been opened at least once */
static int        g_startSci;      /* view at startup, to save WIN.INI only if it changed */
static FARPROC    g_statProc = NULL;

static void LoadStr(int id, char *buf, int len)
{
    if (!LoadString(g_hInst, g_strBase + id, buf, len))
        LoadString(g_hInst, STR_BASE_EN + id, buf, len);
}

/*
 * Icon: taken from the original Windows calculator, which the installer
 * keeps as <WINDOWS>\CALC31.EXE. Falls back to the built-in icon.
 */
static HICON g_extIcon = NULL;

static HICON LoadAppIcon(HINSTANCE hInst)
{
    char path[160];
    UINT n = GetWindowsDirectory(path, sizeof(path) - 16);
    HICON h;

    if (n > 0 && n < sizeof(path) - 16) {
        if (path[n - 1] != '\\') lstrcat(path, "\\");
        lstrcat(path, "CALC31.EXE");
        h = ExtractIcon(hInst, path, 0);
        if (h != NULL && h != (HICON)1) {
            g_extIcon = h;
            return h;
        }
    }
    return LoadIcon(hInst, "CALC");
}

static void DetectLocale(void)
{
    char buf[16];
    GetProfileString("intl", "sLanguage", "enu", buf, sizeof(buf));
    if ((buf[0] == 'e' || buf[0] == 'E') && (buf[1] == 's' || buf[1] == 'S'))
        g_strBase = STR_BASE_ES;
    GetProfileString("intl", "sDecimal", ".", buf, sizeof(buf));
    g_dec = buf[0] ? buf[0] : '.';
    g_decLabel[0] = g_dec;
    GetProfileString("intl", "sThousand", ",", buf, sizeof(buf));
    g_thousand = buf[0] ? buf[0] : ',';
}

/* ---------- About box ---------- */

/* 49053 -> "49.053" using the regional thousands separator */
static void FormatThousands(unsigned long n, char *out)
{
    char tmp[16];
    int len, i, j = 0;
    sprintf(tmp, "%lu", n);
    len = strlen(tmp);
    for (i = 0; i < len; i++) {
        out[j++] = tmp[i];
        if ((len - i - 1) > 0 && (len - i - 1) % 3 == 0) out[j++] = g_thousand;
    }
    out[j] = '\0';
}

static void SetDlgStr(HWND dlg, int ctl, int ids)
{
    char buf[128];
    LoadStr(ids, buf, sizeof(buf));
    SetDlgItemText(dlg, ctl, buf);
}

BOOL CALLBACK __export AboutProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    char fmt[48], num[24], buf[64];
    (void)lp;

    switch (msg) {
    case WM_INITDIALOG:
        LoadStr(IDS_ABOUT_TITLE, buf, sizeof(buf));
        SetWindowText(dlg, buf);
        SetDlgStr(dlg, IDOK, IDS_OK);
        SetDlgStr(dlg, IDC_LINE1, IDS_ABOUT_L1);
        SetDlgStr(dlg, IDC_LINE2, IDS_ABOUT_L2);
        SetDlgStr(dlg, IDC_LINE3, IDS_ABOUT_L3);
        SetDlgStr(dlg, IDC_LINE4, IDS_ABOUT_L4);
        SetDlgStr(dlg, IDC_LINE5, IDS_ABOUT_L5);
        SetDlgStr(dlg, IDC_MODE,
                  (GetWinFlags() & WF_ENHANCED) ? IDS_MODE_ENH : IDS_MODE_STD);
        SetDlgStr(dlg, IDC_MEMLBL, IDS_MEM_LABEL);
        SetDlgStr(dlg, IDC_RESLBL, IDS_RES_LABEL);

        LoadStr(IDS_MEM_FMT, fmt, sizeof(fmt));
        FormatThousands(GetFreeSpace(0) / 1024UL, num);
        sprintf(buf, fmt, num);
        SetDlgItemText(dlg, IDC_MEM, buf);

        LoadStr(IDS_RES_FMT, fmt, sizeof(fmt));
        sprintf(buf, fmt, GetFreeSystemResources(GFSR_SYSTEMRESOURCES));
        SetDlgItemText(dlg, IDC_RES, buf);
        return TRUE;

    case WM_COMMAND:
        if (wp == IDOK || wp == IDCANCEL) {
            EndDialog(dlg, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static void ShowAbout(HWND hwnd)
{
    FARPROC proc = MakeProcInstance((FARPROC)AboutProc, g_hInst);
    DialogBox(g_hInst, "ABOUTBOX", hwnd, (DLGPROC)proc);
    FreeProcInstance(proc);
}

/* ---------- Drawing ---------- */
static void FillSolid(HDC hdc, int x, int y, int w, int h, COLORREF c)
{
    HBRUSH br = NULL, own = NULL;
    RECT r;
    if (c == C_FACE) br = g_brFace;
    else if (c == C_SHADOW) br = g_brShadow;
    else if (c == C_LIGHT) br = g_brLight;
    else if (c == C_DARK) br = g_brDark;
    else br = own = CreateSolidBrush(c);
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    FillRect(hdc, &r, br);
    if (own) DeleteObject(own);
}

/* 1 px frame: tl = top/left, br = bottom/right */
static void Frame(HDC hdc, int x, int y, int w, int h, COLORREF tl, COLORREF br)
{
    FillSolid(hdc, x, y, w - 1, 1, tl);
    FillSolid(hdc, x, y, 1, h - 1, tl);
    FillSolid(hdc, x, y + h - 1, w, 1, br);
    FillSolid(hdc, x + w - 1, y, 1, h, br);
}

static void Sunken(HDC hdc, const RECT *r, COLORREF fill)
{
    int x = r->left, y = r->top, w = r->right - r->left, h = r->bottom - r->top;
    FillSolid(hdc, x + 2, y + 2, w - 4, h - 4, fill);
    Frame(hdc, x, y, w, h, C_SHADOW, C_LIGHT);
    Frame(hdc, x + 1, y + 1, w - 2, h - 2, C_DARK, C_FACE);
}

/* Etched group frame (gray line with a white line inside/below) */
static void Etched(HDC hdc, const RECT *r)
{
    int x = r->left, y = r->top, w = r->right - r->left, h = r->bottom - r->top;
    Frame(hdc, x + 1, y + 1, w - 1, h - 1, C_LIGHT, C_LIGHT);
    Frame(hdc, x, y, w - 1, h - 1, C_SHADOW, C_SHADOW);
}

static void DrawButton(HDC hdc, int i)
{
    const Button *b = &g_buttons[i];
    int down = (i == g_pressed && g_pressedIn);
    const char *label = b->label;
    RECT r;

    if (b->key == K_BACK) label = g_backLabel;
    else if (b->key == K_DECIMAL) label = g_decLabel;

    FillSolid(hdc, b->x, b->y, b->w, b->h, C_FACE);
    if (down) {   /* pressed: sunken edge, as in Windows 95 */
        Frame(hdc, b->x, b->y, b->w, b->h, C_DARK, C_LIGHT);
        Frame(hdc, b->x + 1, b->y + 1, b->w - 2, b->h - 2, C_SHADOW, C_FACE);
    } else {
        Frame(hdc, b->x, b->y, b->w, b->h, C_LIGHT, C_DARK);
        Frame(hdc, b->x + 1, b->y + 1, b->w - 2, b->h - 2, C_FACE, C_SHADOW);
    }

    r.left = b->x + (down ? 1 : 0);
    r.top = b->y + (down ? 1 : 0);
    r.right = r.left + b->w;
    r.bottom = r.top + b->h;
    SetTextColor(hdc, b->color);
    DrawText(hdc, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static int CurrentError(void)
{
    return g_isSci ? g_sci.err : g_eng.err;
}

static void DrawDisplay(HDC hdc)
{
    char text[96];
    const RECT *rc = g_isSci ? &rcSciDisplay : &rcStdDisplay;
    RECT r = *rc;
    int err = CurrentError();

    if (err) {
        int id = err == ERR_DIVZERO ? IDS_ERR_DIVZERO :
                 err == ERR_INVALID ? IDS_ERR_INVALID : IDS_ERR_OVERFLW;
        LoadStr(id, text, sizeof(text));
    } else if (g_isSci) {
        sci_display(&g_sci, g_dec, text, sizeof(text));
    } else {
        eng_display(&g_eng, g_dec, text, sizeof(text));
    }
    Sunken(hdc, rc, C_LIGHT);
    r.right -= 8;
    SetTextColor(hdc, C_DARK);
    DrawText(hdc, text, -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void DrawBox(HDC hdc, const RECT *rc, const char *text)
{
    RECT r = *rc;
    Sunken(hdc, rc, C_FACE);
    if (text && *text) {
        SetTextColor(hdc, C_DARK);
        DrawText(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

/* 12x12 Windows 95-style radio button */
static void DrawRadio(HDC hdc, int x, int y, int on)
{
    static const char *map[12] = {
        "....GGGG....",
        "..GGKKKKGG..",
        ".GKKWWWWKKL.",
        ".GKWWWWWWFL.",
        "GKWWWWWWWWFL",
        "GKWWWWWWWWFL",
        "GKWWWWWWWWFL",
        "GKWWWWWWWWFL",
        ".GKWWWWWWFL.",
        ".LFFWWWWFFL.",
        "..LLFFFFLL..",
        "....LLLL....",
    };
    int i, j;
    for (i = 0; i < 12; i++) {
        for (j = 0; j < 12; j++) {
            COLORREF c;
            switch (map[i][j]) {
            case 'G': c = C_SHADOW; break;
            case 'K': c = C_DARK; break;
            case 'W': c = C_LIGHT; break;
            case 'F': c = C_FACE; break;
            case 'L': c = C_LIGHT; break;
            default: continue;
            }
            if (on && i >= 4 && i <= 7 && j >= 4 && j <= 7 &&
                !((i == 4 || i == 7) && (j == 4 || j == 7)))
                c = C_DARK;
            SetPixel(hdc, x + j, y + i, c);
        }
    }
}

/* 13x13 Windows 95-style check box */
static void DrawCheck(HDC hdc, int x, int y, int on)
{
    RECT r;
    r.left = x; r.top = y; r.right = x + 13; r.bottom = y + 13;
    Sunken(hdc, &r, C_LIGHT);
    if (on) {
        static const char *mark[7] = {
            "......#", ".....##", "#...###", "##.###.", "#####..", ".###...", "..#....",
        };
        int i, j;
        for (i = 0; i < 7; i++)
            for (j = 0; j < 7; j++)
                if (mark[i][j] == '#') SetPixel(hdc, x + 3 + j, y + 3 + i, C_DARK);
    }
}

static int ControlOn(const Control *c)
{
    switch (c->key) {
    case K_HEX:     return g_sci.base == 16;
    case K_DECBASE: return g_sci.base == 10;
    case K_OCT:     return g_sci.base == 8;
    case K_BIN:     return g_sci.base == 2;
    case K_W0: return g_sci.base == 10 ? g_sci.angle == ANG_DEG  : g_sci.wordBits == 32;
    case K_W1: return g_sci.base == 10 ? g_sci.angle == ANG_RAD  : g_sci.wordBits == 16;
    case K_W2: return g_sci.base == 10 ? g_sci.angle == ANG_GRAD : g_sci.wordBits == 8;
    case K_INV: return g_sci.inv;
    case K_HYP: return g_sci.hyp;
    }
    return 0;
}

static int ControlLabel(const Control *c)
{
    if (c->ids) return c->ids;
    if (g_sci.base == 10) return c->key == K_W0 ? IDS_DEG : c->key == K_W1 ? IDS_RAD : IDS_GRAD;
    return c->key == K_W0 ? IDS_DWORD : c->key == K_W1 ? IDS_WORD : IDS_BYTE;
}

static void DrawControls(HDC hdc)
{
    unsigned i;
    char buf[32];
    Etched(hdc, &rcBaseGroup);
    Etched(hdc, &rcWordGroup);
    Etched(hdc, &rcInvGroup);
    SetTextColor(hdc, C_DARK);
    for (i = 0; i < NCTL; i++) {
        const Control *c = &sciControls[i];
        int size = c->type == CTL_RADIO ? 12 : 13;
        RECT r;
        if (c->type == CTL_RADIO) DrawRadio(hdc, c->x, c->y, ControlOn(c));
        else DrawCheck(hdc, c->x, c->y, ControlOn(c));
        LoadStr(ControlLabel(c), buf, sizeof(buf));
        r.left = c->x + 18; r.top = c->y - 1; r.right = c->x + CTL_HIT_W + 8; r.bottom = c->y + size + 1;
        DrawText(hdc, buf, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

static void Paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HFONT old = SelectObject(hdc, g_font);
    RECT tmp;
    int i;
    char paren[8];

    SetBkMode(hdc, TRANSPARENT);
    FillRect(hdc, &ps.rcPaint, g_brFace);   /* background only where needed */

    DrawDisplay(hdc);
    if (g_isSci) {
        if (g_sci.parens > 0) sprintf(paren, "(=%d", g_sci.parens); else paren[0] = '\0';
        DrawBox(hdc, &rcParenBox, paren);
        DrawBox(hdc, &rcSciMemBox, g_sci.mem != 0.0 ? "M" : "");
        DrawControls(hdc);
    } else {
        DrawBox(hdc, &rcStdMemBox, eng_has_memory(&g_eng) ? "M" : "");
    }
    SelectObject(hdc, g_btnFont);
    for (i = 0; i < g_nButtons; i++) {
        RECT br;
        br.left = g_buttons[i].x; br.top = g_buttons[i].y;
        br.right = br.left + g_buttons[i].w; br.bottom = br.top + g_buttons[i].h;
        if (IntersectRect(&tmp, &ps.rcPaint, &br)) DrawButton(hdc, i);
    }
    SelectObject(hdc, old);
    EndPaint(hwnd, &ps);
}

static void InvalidateButton(HWND hwnd, int i)
{
    RECT r;
    if (i < 0) return;
    r.left = g_buttons[i].x; r.top = g_buttons[i].y;
    r.right = r.left + g_buttons[i].w; r.bottom = r.top + g_buttons[i].h;
    InvalidateRect(hwnd, &r, FALSE);
}

static void Refresh(HWND hwnd)
{
    if (g_isSci) {
        InvalidateRect(hwnd, &rcSciDisplay, FALSE);
        InvalidateRect(hwnd, &rcParenBox, FALSE);
        InvalidateRect(hwnd, &rcSciMemBox, FALSE);
        InvalidateRect(hwnd, &rcBaseGroup, FALSE);
        InvalidateRect(hwnd, &rcWordGroup, FALSE);
        InvalidateRect(hwnd, &rcInvGroup, FALSE);
    } else {
        InvalidateRect(hwnd, &rcStdDisplay, FALSE);
        InvalidateRect(hwnd, &rcStdMemBox, FALSE);
    }
}

static int HitTest(int x, int y)
{
    int i;
    for (i = 0; i < g_nButtons; i++) {
        const Button *b = &g_buttons[i];
        if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) return i;
    }
    return -1;
}

static int HitControl(int x, int y)
{
    unsigned i;
    if (!g_isSci) return -1;
    for (i = 0; i < NCTL; i++) {
        const Control *c = &sciControls[i];
        if (x >= c->x - 2 && x < c->x + CTL_HIT_W && y >= c->y - 3 && y < c->y + 16) return c->key;
    }
    return -1;
}

/* ---------- Statistics box ---------- */
static int StatBoxOpen(void);
static void StatRefresh(void)
{
    char buf[80], fmt[16];
    int i;
    HWND list;
    if (!StatBoxOpen()) return;
    list = GetDlgItem(g_statBox, IDC_STATLIST);
    SendMessage(list, WM_SETREDRAW, FALSE, 0);
    SendMessage(list, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_sci.ndata; i++) {
        sci_format(&g_sci, g_sci.data[i], g_dec, buf, sizeof(buf));
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)(LPSTR)buf);
    }
    if (g_sci.ndata > 0)   /* like Windows 95: the newest value is selected */
        SendMessage(list, LB_SETCURSEL, g_sci.ndata - 1, 0);
    SendMessage(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, NULL, TRUE);
    LoadStr(IDS_STAT_N, fmt, sizeof(fmt));
    sprintf(buf, fmt, g_sci.ndata);
    SetDlgItemText(g_statBox, IDC_STATN, buf);
}

/* Windows 95-style push button for the Statistics box (owner-drawn) */
static void DrawStatButton(const DRAWITEMSTRUCT FAR *di)
{
    HDC hdc = di->hDC;
    RECT r = di->rcItem;
    char text[32];
    int down = (di->itemState & ODS_SELECTED) != 0;
    int x, y, w, h;
    HFONT old;

    FillRect(hdc, &r, g_brFace);
    if (di->CtlID == IDC_RET) {          /* default button: extra black outline */
        Frame(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top, C_DARK, C_DARK);
        InflateRect(&r, -1, -1);
    }
    x = r.left; y = r.top; w = r.right - r.left; h = r.bottom - r.top;
    if (down) {
        Frame(hdc, x, y, w, h, C_SHADOW, C_SHADOW);
    } else {
        Frame(hdc, x, y, w, h, C_LIGHT, C_DARK);
        Frame(hdc, x + 1, y + 1, w - 2, h - 2, C_FACE, C_SHADOW);
    }
    GetWindowText(di->hwndItem, text, sizeof(text));
    old = SelectObject(hdc, g_font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, C_DARK);
    if (down) OffsetRect(&r, 1, 1);
    DrawText(hdc, text, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (di->itemState & ODS_FOCUS) {
        RECT f = r;
        InflateRect(&f, -3, -3);
        DrawFocusRect(hdc, &f);
    }
    SelectObject(hdc, old);
}

BOOL CALLBACK __export StatProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    int sel;
    char buf[64];

    switch (msg) {
    case WM_INITDIALOG: {
        RECT lr;
        HWND list = GetDlgItem(dlg, IDC_STATLIST);
        g_statBox = dlg;
        LoadStr(IDS_STAT_TITLE, buf, sizeof(buf));
        SetWindowText(dlg, buf);
        SetDlgStr(dlg, IDC_RET, IDS_STAT_RET);
        SetDlgStr(dlg, IDC_LOAD, IDS_STAT_LOAD);
        SetDlgStr(dlg, IDC_CD, IDS_STAT_CD);
        SetDlgStr(dlg, IDC_CAD, IDS_STAT_CAD);
        SendDlgItemMessage(dlg, IDC_STATLIST, WM_SETFONT, (WPARAM)g_font, 0);
        SendDlgItemMessage(dlg, IDC_STATN, WM_SETFONT, (WPARAM)g_font, 0);
        /* leave 2 px around the list for the Windows 95 sunken border */
        GetWindowRect(list, &lr);
        ScreenToClient(dlg, (POINT FAR *)&lr.left);
        ScreenToClient(dlg, (POINT FAR *)&lr.right);
        MoveWindow(list, lr.left + 2, lr.top + 2, lr.right - lr.left - 4, lr.bottom - lr.top - 4, FALSE);
        return TRUE;
    }

    case WM_CTLCOLOR:   /* grey dialog background, like Windows 95 */
        if (HIWORD(lp) == CTLCOLOR_DLG || HIWORD(lp) == CTLCOLOR_STATIC || HIWORD(lp) == CTLCOLOR_BTN) {
            SetBkColor((HDC)wp, C_FACE);
            return (BOOL)g_brFace;
        }
        return FALSE;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT lr;
        HDC hdc = BeginPaint(dlg, &ps);
        GetWindowRect(GetDlgItem(dlg, IDC_STATLIST), &lr);
        ScreenToClient(dlg, (POINT FAR *)&lr.left);
        ScreenToClient(dlg, (POINT FAR *)&lr.right);
        InflateRect(&lr, 2, 2);
        Sunken(hdc, &lr, C_LIGHT);
        EndPaint(dlg, &ps);
        return TRUE;
    }

    case WM_DRAWITEM:
        DrawStatButton((const DRAWITEMSTRUCT FAR *)lp);
        return TRUE;

    case WM_COMMAND:
        sel = (int)SendDlgItemMessage(dlg, IDC_STATLIST, LB_GETCURSEL, 0, 0);
        switch (wp) {
        case IDOK:
        case IDC_RET:      /* like the original: close the box, back to the calculator */
            DestroyWindow(dlg);
            SetActiveWindow(g_hwnd);
            return TRUE;
        case IDC_LOAD:
            if (sel >= 0) { sci_stat_load(&g_sci, sel); Refresh(g_hwnd); }
            return TRUE;
        case IDC_CD:
            if (sel >= 0) { sci_stat_clear_one(&g_sci, sel); StatRefresh(); }
            return TRUE;
        case IDC_CAD:
            sci_stat_clear_all(&g_sci);
            StatRefresh();
            return TRUE;
        case IDCANCEL:
            DestroyWindow(dlg);
            return TRUE;
        }
        break;

    case WM_DESTROY:
        if (g_statBox == dlg) g_statBox = NULL;
        return TRUE;
    }
    return FALSE;
}

static int StatBoxOpen(void)
{
    if (g_statBox && !IsWindow(g_statBox)) g_statBox = NULL;
    return g_statBox != NULL;
}

static void OpenStatBox(void)
{
    RECT wr;
    HWND box;
    if (StatBoxOpen()) { ShowWindow(g_statBox, SW_SHOW); StatRefresh(); return; }
    if (!g_statProc) g_statProc = MakeProcInstance((FARPROC)StatProc, g_hInst);
    box = CreateDialog(g_hInst, "STATBOX", g_hwnd, (DLGPROC)g_statProc);
    if (!box) return;
    g_statBox = box;
    g_statUsed = 1;
    /* same place as in Windows 95: over the left part of the calculator */
    GetWindowRect(g_hwnd, &wr);
    SetWindowPos(box, NULL, wr.left + 57, wr.top + 105, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    StatRefresh();
}

/* ---------- Key dispatch ---------- */
static void Press(HWND hwnd, int key)
{
    if (!g_isSci) {
        eng_key(&g_eng, key);
        Refresh(hwnd);
        return;
    }
    if (key == K_STA) { OpenStatBox(); return; }
    if ((key == K_AVE || key == K_SUM || key == K_SDEV || key == K_DAT) && !g_statUsed) {
        MessageBeep(0);   /* like the original: inactive until Sta has been used once */
        return;
    }
    if (!sci_key_valid(&g_sci, key)) { MessageBeep(0); return; }
    sci_key(&g_sci, key);
    if (key == K_DAT || key == K_HEX || key == K_DECBASE || key == K_OCT || key == K_BIN ||
        key == K_W0 || key == K_W1 || key == K_W2)
        StatRefresh();
    Refresh(hwnd);
}

/* ---------- View switching ---------- */
static void SetClientSize(HWND hwnd, int w, int h)
{
    RECT wr, cr;
    GetWindowRect(hwnd, &wr);
    GetClientRect(hwnd, &cr);
    SetWindowPos(hwnd, NULL, 0, 0,
                 (wr.right - wr.left) + (w - cr.right),
                 (wr.bottom - wr.top) + (h - cr.bottom),
                 SWP_NOMOVE | SWP_NOZORDER);
}

static void ApplyView(HWND hwnd, int sci)
{
    HMENU menu = GetMenu(hwnd);
    g_isSci = sci;
    g_buttons = sci ? sciButtons : stdButtons;
    g_nButtons = sci ? (int)NSCI : (int)NSTD;
    g_pressed = -1;
    CheckMenuItem(menu, IDM_STANDARD, MF_BYCOMMAND | (sci ? MF_UNCHECKED : MF_CHECKED));
    CheckMenuItem(menu, IDM_SCIENTIFIC, MF_BYCOMMAND | (sci ? MF_CHECKED : MF_UNCHECKED));
    if (!sci && StatBoxOpen()) DestroyWindow(g_statBox);
    SetClientSize(hwnd, sci ? SCIVIEW_W : STD_W, sci ? SCIVIEW_H : STD_H);
    InvalidateRect(hwnd, NULL, FALSE);
}

static void SwitchView(HWND hwnd, int sci)
{
    if (sci == g_isSci) return;
    if (sci) {
        if (!g_eng.err) sci_set_value(&g_sci, eng_value(&g_eng));
        g_sci.mem = g_eng.mem;
    } else {
        if (!g_sci.err) eng_set_value(&g_eng, sci_value(&g_sci));
        g_eng.mem = g_sci.mem;
    }
    ApplyView(hwnd, sci);
}

/* ---------- Clipboard ---------- */
static void CurrentText(char *text, int len)
{
    if (g_isSci) sci_display(&g_sci, g_dec, text, len);
    else eng_display(&g_eng, g_dec, text, len);
}

static void CopyDisplay(HWND hwnd)
{
    char text[96];
    HGLOBAL h;
    LPSTR p;
    int n;

    if (CurrentError()) return;
    CurrentText(text, sizeof(text));
    n = strlen(text);
    if (n > 0 && text[n - 1] == g_dec) text[n - 1] = '\0';   /* "12." -> "12" */

    h = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (DWORD)strlen(text) + 1);
    if (!h) return;
    p = GlobalLock(h);
    lstrcpy(p, text);
    GlobalUnlock(h);
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        SetClipboardData(CF_TEXT, h);
        CloseClipboard();
    } else {
        GlobalFree(h);
    }
}

/* Keyboard characters shared by both views */
static int CharToKey(char c)
{
    if (c >= '0' && c <= '9') return K_0 + (c - '0');
    if (c == g_dec || c == '.' || c == ',') return K_DECIMAL;
    switch (c) {
    case '+': return K_ADD;
    case '-': return K_SUB;
    case '*': return K_MUL;
    case '/': return K_DIV;
    case '=': return K_EQUALS;
    case 'r': case 'R': return K_RECIP;
    }
    if (!g_isSci) {
        if (c == '%') return K_PERCENT;
        if (c == '@') return K_SQRT;
        return -1;
    }
    if (g_sci.base == 16) {
        if (c >= 'a' && c <= 'f') return K_A + (c - 'a');
        if (c >= 'A' && c <= 'F') return K_A + (c - 'A');
    }
    switch (c) {
    case 's': case 'S': return K_SIN;
    case 'o': case 'O': return K_COS;
    case 't': case 'T': return K_TAN;
    case 'n': case 'N': return K_LN;
    case 'l': case 'L': return K_LOG;
    case 'y': case 'Y': return K_POW;
    case 'm': case 'M': return K_DMS;
    case 'p': case 'P': return K_PI;
    case 'v': case 'V': return K_FE;
    case 'x': case 'X': return K_EXP;
    case 'i': case 'I': return K_INV;
    case 'h': case 'H': return K_HYP;
    case '!': return K_FACT;
    case '@': return K_SQR;
    case '#': return K_CUBE;
    case '(': return K_LPAREN;
    case ')': return K_RPAREN;
    case '%': return K_MOD;
    case '&': return K_AND;
    case '|': return K_OR;
    case '^': return K_XOR;
    case '<': return K_LSH;
    case '~': return K_NOT;
    case ';': return K_INT;
    }
    return -1;
}

static void PasteText(HWND hwnd)
{
    HGLOBAL h;
    LPSTR p;
    int k;

    if (!OpenClipboard(hwnd)) return;
    h = GetClipboardData(CF_TEXT);
    if (h && (p = GlobalLock(h)) != NULL) {
        for (; *p; p++) {
            k = CharToKey(*p);
            if (k < 0) continue;
            if (g_isSci) { if (sci_key_valid(&g_sci, k)) sci_key(&g_sci, k); }
            else eng_key(&g_eng, k);
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
    StatRefresh();
    Refresh(hwnd);
}

/* ---------- Window ---------- */
LRESULT CALLBACK __export WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        Paint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1;   /* Paint() draws it */

    case WM_LBUTTONDOWN: {
        int ctl = HitControl(LOWORD(lp), HIWORD(lp));
        if (ctl >= 0) { Press(hwnd, ctl); return 0; }
        g_pressed = HitTest(LOWORD(lp), HIWORD(lp));
        if (g_pressed >= 0) {
            g_pressedIn = 1;
            SetCapture(hwnd);
            InvalidateButton(hwnd, g_pressed);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_pressed >= 0) {
            int in = HitTest((short)LOWORD(lp), (short)HIWORD(lp)) == g_pressed;
            if (in != g_pressedIn) {
                g_pressedIn = in;
                InvalidateButton(hwnd, g_pressed);
            }
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_pressed >= 0) {
            int i = g_pressed, in = g_pressedIn;
            ReleaseCapture();
            g_pressed = -1;
            g_pressedIn = 0;
            InvalidateButton(hwnd, i);
            if (in) Press(hwnd, g_buttons[i].key);
        }
        return 0;

    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) < 0) {
            switch (wp) {
            case 'C': CopyDisplay(hwnd); break;
            case 'V': PasteText(hwnd); break;
            case 'L': Press(hwnd, K_MC); break;
            case 'R': Press(hwnd, K_MR); break;
            case 'M': Press(hwnd, K_MS); break;
            case 'P': Press(hwnd, K_MPLUS); break;
            case 'S': if (g_isSci) Press(hwnd, K_STA); break;
            case 'A': if (g_isSci) Press(hwnd, K_AVE); break;
            case 'T': if (g_isSci) Press(hwnd, K_SUM); break;
            case 'D': if (g_isSci) Press(hwnd, K_SDEV); break;
            }
            return 0;
        }
        switch (wp) {
        case VK_DELETE: Press(hwnd, K_CE); return 0;
        case VK_F9:     Press(hwnd, K_NEGATE); return 0;
        }
        if (g_isSci) {
            switch (wp) {
            case VK_INSERT: Press(hwnd, K_DAT); return 0;
            case VK_F2: Press(hwnd, K_W0); return 0;
            case VK_F3: Press(hwnd, K_W1); return 0;
            case VK_F4: Press(hwnd, K_W2); return 0;
            case VK_F5: Press(hwnd, K_HEX); return 0;
            case VK_F6: Press(hwnd, K_DECBASE); return 0;
            case VK_F7: Press(hwnd, K_OCT); return 0;
            case VK_F8: Press(hwnd, K_BIN); return 0;
            }
        }
        break;

    case WM_CHAR:
        if (GetKeyState(VK_CONTROL) < 0) return 0;
        switch (wp) {
        case '\r': Press(hwnd, K_EQUALS); return 0;
        case '\b': Press(hwnd, K_BACK); return 0;
        case 27:   Press(hwnd, K_C); return 0;
        default: {
            int k = CharToKey((char)wp);
            if (k >= 0) Press(hwnd, k);
            return 0;
        }
        }

    case WM_COMMAND:
        switch (wp) {
        case IDM_COPY:       CopyDisplay(hwnd); break;
        case IDM_PASTE:      PasteText(hwnd); break;
        case IDM_STANDARD:   SwitchView(hwnd, 0); break;
        case IDM_SCIENTIFIC: SwitchView(hwnd, 1); break;
        case IDM_ABOUT:      ShowAbout(hwnd); break;
        }
        return 0;

    case WM_INITMENUPOPUP:
        /* Gray out Paste when there is no text on the clipboard */
        EnableMenuItem((HMENU)wp, IDM_PASTE, MF_BYCOMMAND |
                       (IsClipboardFormatAvailable(CF_TEXT) ? MF_ENABLED : MF_GRAYED));
        return 0;

    case WM_DESTROY:
        if (g_isSci != g_startSci)
            WriteProfileString("SciCalc", "layout", g_isSci ? "0" : "1");
        if (StatBoxOpen()) DestroyWindow(g_statBox);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    MSG m;
    RECT r;
    char title[48];
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    (void)cmd;
    g_hInst = hInst;
    DetectLocale();
    LoadStr(IDS_TITLE, title, sizeof(title));
    LoadStr(IDS_BACKSPACE, g_backLabel, sizeof(g_backLabel));
    eng_init(&g_eng);
    sci_init(&g_sci);

    if (!hPrev) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInst;
        wc.hIcon = LoadAppIcon(hInst);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = "Calc95Class";
        if (!RegisterClass(&wc)) return 0;
    }

    /* Fixed pixel sizes (8 pt / 10 pt at 96 dpi) so the layout does not
     * depend on the display driver's font size setting. */
    g_font = CreateFont(-11, 0, 0, 0,
                        FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                        VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
    g_btnFont = CreateFont(-13, 0, 0, 0,
                        FW_BOLD, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                        VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
    g_brFace = CreateSolidBrush(C_FACE);
    g_brShadow = CreateSolidBrush(C_SHADOW);
    g_brLight = CreateSolidBrush(C_LIGHT);
    g_brDark = CreateSolidBrush(C_DARK);

    r.left = 0; r.top = 0; r.right = STD_W; r.bottom = STD_H;
    AdjustWindowRect(&r, style, TRUE);

    g_hwnd = CreateWindow("Calc95Class", title, style,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          r.right - r.left, r.bottom - r.top,
                          NULL,
                          LoadMenu(hInst, g_strBase == STR_BASE_ES ? "MENU_ES" : "MENU_EN"),
                          hInst, NULL);
    if (!g_hwnd) return 0;

    /* Restores the last view; also corrects the window size, since
     * AdjustWindowRect can be off by a pixel with a menu bar on Windows 3.1. */
    g_startSci = GetProfileInt("SciCalc", "layout", 1) == 0;
    ApplyView(g_hwnd, g_startSci);
    ShowWindow(g_hwnd, show);
    UpdateWindow(g_hwnd);

    while (GetMessage(&m, NULL, 0, 0)) {
        if (g_statBox && IsDialogMessage(g_statBox, &m)) continue;
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    if (g_statProc) FreeProcInstance(g_statProc);
    DeleteObject(g_font);
    DeleteObject(g_btnFont);
    DeleteObject(g_brFace);
    DeleteObject(g_brShadow);
    DeleteObject(g_brLight);
    DeleteObject(g_brDark);
    if (g_extIcon) DestroyIcon(g_extIcon);
    return m.wParam;
}
