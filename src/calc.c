/*
 * calc.c - Calculator with a Windows 95-style interface for Windows 3.1 (Win16).
 * Phase 1: Standard mode + i18n (English by default, Spanish when
 * WIN.INI [intl] sLanguage starts with "es"). The decimal separator is
 * read from [intl] sDecimal. Both are read at startup only.
 *
 * The window frame and menu bar are the native Windows 3.1 ones; the
 * client area is custom-painted using the 16-color VGA palette.
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include "engine.h"
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

/* ---------- Geometry (pixels, VGA at 96 dpi) ---------- */
/* Measured against a Windows 95 Calculator screenshot (96 dpi):
 * 9 px left / 11 px right / 12 px bottom padding, 36x28 keys on a
 * 40 x 34 px grid, Back/CE/C at 48-49 px wide. */
#define CLIENT_W 268
#define CLIENT_H 228
#define X0 9

typedef struct {
    int key;
    int x, y, w, h;
    const char *label;  /* NULL = use the localized Backspace string */
    COLORREF color;
} Button;

static Button buttons[] = {
    { K_BACK,  X0+ 93,  52, 48, 28, NULL,   C_MAROON },
    { K_CE,    X0+146,  52, 48, 28, "CE",   C_MAROON },
    { K_C,     X0+199,  52, 49, 28, "C",    C_MAROON },

    { K_MC,    X0,      86, 36, 28, "MC",   C_RED },
    { K_MR,    X0,     120, 36, 28, "MR",   C_RED },
    { K_MS,    X0,     154, 36, 28, "MS",   C_RED },
    { K_MPLUS, X0,     188, 36, 28, "M+",   C_RED },

    { K_7,      X0+ 52,  86, 36, 28, "7",    C_BLUE },
    { K_8,      X0+ 92,  86, 36, 28, "8",    C_BLUE },
    { K_9,      X0+132,  86, 36, 28, "9",    C_BLUE },
    { K_DIV,    X0+172,  86, 36, 28, "/",    C_RED  },
    { K_SQRT,   X0+212,  86, 36, 28, "sqrt", C_NAVY },

    { K_4,      X0+ 52, 120, 36, 28, "4",    C_BLUE },
    { K_5,      X0+ 92, 120, 36, 28, "5",    C_BLUE },
    { K_6,      X0+132, 120, 36, 28, "6",    C_BLUE },
    { K_MUL,    X0+172, 120, 36, 28, "*",    C_RED  },
    { K_PERCENT,X0+212, 120, 36, 28, "%",    C_NAVY },

    { K_1,      X0+ 52, 154, 36, 28, "1",    C_BLUE },
    { K_2,      X0+ 92, 154, 36, 28, "2",    C_BLUE },
    { K_3,      X0+132, 154, 36, 28, "3",    C_BLUE },
    { K_SUB,    X0+172, 154, 36, 28, "-",    C_RED  },
    { K_RECIP,  X0+212, 154, 36, 28, "1/x",  C_NAVY },

    { K_0,      X0+ 52, 188, 36, 28, "0",    C_BLUE },
    { K_NEGATE, X0+ 92, 188, 36, 28, "+/-",  C_BLUE },
    { K_DECIMAL,X0+132, 188, 36, 28, NULL,   C_BLUE },  /* decimal separator */
    { K_ADD,    X0+172, 188, 36, 28, "+",    C_RED  },
    { K_EQUALS, X0+212, 188, 36, 28, "=",    C_RED  },
};
#define NBUTTONS (sizeof(buttons) / sizeof(buttons[0]))

static const RECT rcDisplay = { X0, 20, X0 + 248, 46 };
static const RECT rcMemBox  = { X0, 52, X0 + 36, 78 };

/* ---------- Global state ---------- */
static HINSTANCE  g_hInst;
static CalcEngine g_eng;
static HFONT      g_font;      /* display: MS Sans Serif 8 pt */
static HFONT      g_btnFont;   /* keys: MS Sans Serif 10 pt bold */
static int        g_strBase = STR_BASE_EN;
static char       g_dec = '.';
static char       g_decLabel[2] = ".";
static char       g_thousand = ',';
static char       g_backLabel[24];
static int        g_pressed = -1;     /* index of the button pressed with the mouse */
static int        g_pressedIn = 0;    /* the cursor is still over it */

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
    HBRUSH br = CreateSolidBrush(c);
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
    FillRect(hdc, &r, br);
    DeleteObject(br);
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

static void DrawButton(HDC hdc, int i)
{
    const Button *b = &buttons[i];
    int down = (i == g_pressed && g_pressedIn);
    const char *label = b->label;
    RECT r;

    if (b->key == K_BACK) label = g_backLabel;
    else if (b->key == K_DECIMAL) label = g_decLabel;

    FillSolid(hdc, b->x, b->y, b->w, b->h, C_FACE);
    if (down) {
        Frame(hdc, b->x, b->y, b->w, b->h, C_DARK, C_DARK);
        Frame(hdc, b->x + 1, b->y + 1, b->w - 2, b->h - 2, C_SHADOW, C_SHADOW);
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

static void DrawDisplay(HDC hdc)
{
    char text[96];
    RECT r = rcDisplay;

    if (g_eng.err) {
        int id = g_eng.err == ERR_DIVZERO ? IDS_ERR_DIVZERO :
                 g_eng.err == ERR_INVALID ? IDS_ERR_INVALID : IDS_ERR_OVERFLW;
        LoadStr(id, text, sizeof(text));
    } else {
        eng_display(&g_eng, g_dec, text, sizeof(text));
    }
    Sunken(hdc, &rcDisplay, C_LIGHT);
    r.right -= 8;
    SetTextColor(hdc, C_DARK);
    DrawText(hdc, text, -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

static void DrawMemBox(HDC hdc)
{
    RECT r = rcMemBox;
    Sunken(hdc, &rcMemBox, C_FACE);
    if (eng_has_memory(&g_eng)) {
        SetTextColor(hdc, C_DARK);
        DrawText(hdc, "M", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void Paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HFONT old = SelectObject(hdc, g_font);
    RECT rc, tmp;
    unsigned i;

    GetClientRect(hwnd, &rc);
    SetBkMode(hdc, TRANSPARENT);

    /* background only where needed, to avoid flicker */
    if (IntersectRect(&tmp, &ps.rcPaint, &rc)) {
        HBRUSH br = CreateSolidBrush(C_FACE);
        FillRect(hdc, &ps.rcPaint, br);
        DeleteObject(br);
    }
    DrawDisplay(hdc);
    DrawMemBox(hdc);
    SelectObject(hdc, g_btnFont);
    for (i = 0; i < NBUTTONS; i++) {
        RECT br;
        br.left = buttons[i].x; br.top = buttons[i].y;
        br.right = br.left + buttons[i].w; br.bottom = br.top + buttons[i].h;
        if (IntersectRect(&tmp, &ps.rcPaint, &br)) DrawButton(hdc, (int)i);
    }
    SelectObject(hdc, old);
    EndPaint(hwnd, &ps);
}

static void InvalidateButton(HWND hwnd, int i)
{
    RECT r;
    if (i < 0) return;
    r.left = buttons[i].x; r.top = buttons[i].y;
    r.right = r.left + buttons[i].w; r.bottom = r.top + buttons[i].h;
    InvalidateRect(hwnd, &r, FALSE);
}

static void Refresh(HWND hwnd)
{
    InvalidateRect(hwnd, &rcDisplay, FALSE);
    InvalidateRect(hwnd, &rcMemBox, FALSE);
}

static int HitTest(int x, int y)
{
    unsigned i;
    for (i = 0; i < NBUTTONS; i++) {
        const Button *b = &buttons[i];
        if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) return (int)i;
    }
    return -1;
}

static void Press(HWND hwnd, int key)
{
    eng_key(&g_eng, key);
    Refresh(hwnd);
}

/* ---------- Clipboard ---------- */
static void CopyDisplay(HWND hwnd)
{
    char text[96];
    HGLOBAL h;
    LPSTR p;
    int n;

    if (g_eng.err) return;
    eng_display(&g_eng, g_dec, text, sizeof(text));
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
    case '%': return K_PERCENT;
    case '@': return K_SQRT;
    case 'r': case 'R': return K_RECIP;
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
            if (k >= 0) eng_key(&g_eng, k);
        }
        GlobalUnlock(h);
    }
    CloseClipboard();
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

    case WM_LBUTTONDOWN:
        g_pressed = HitTest(LOWORD(lp), HIWORD(lp));
        if (g_pressed >= 0) {
            g_pressedIn = 1;
            SetCapture(hwnd);
            InvalidateButton(hwnd, g_pressed);
        }
        return 0;

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
            if (in) Press(hwnd, buttons[i].key);
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
            }
            return 0;
        }
        switch (wp) {
        case VK_DELETE: Press(hwnd, K_CE); return 0;
        case VK_F9:     Press(hwnd, K_NEGATE); return 0;
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
        case IDM_COPY:  CopyDisplay(hwnd); break;
        case IDM_PASTE: PasteText(hwnd); break;
        case IDM_ABOUT: ShowAbout(hwnd); break;
        }
        return 0;

    case WM_INITMENUPOPUP:
        /* Gray out Paste when there is no text on the clipboard */
        EnableMenuItem((HMENU)wp, IDM_PASTE, MF_BYCOMMAND |
                       (IsClipboardFormatAvailable(CF_TEXT) ? MF_ENABLED : MF_GRAYED));
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    HWND hwnd;
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

    r.left = 0; r.top = 0; r.right = CLIENT_W; r.bottom = CLIENT_H;
    AdjustWindowRect(&r, style, TRUE);

    hwnd = CreateWindow("Calc95Class", title, style,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        r.right - r.left, r.bottom - r.top,
                        NULL,
                        LoadMenu(hInst, g_strBase == STR_BASE_ES ? "MENU_ES" : "MENU_EN"),
                        hInst, NULL);
    if (!hwnd) return 0;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    DeleteObject(g_font);
    DeleteObject(g_btnFont);
    if (g_extIcon) DestroyIcon(g_extIcon);
    return m.wParam;
}
