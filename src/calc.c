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

/* ---------- Geometry (pixels, VGA at 96 dpi) ---------- */
#define CLIENT_W 254
#define CLIENT_H 206

typedef struct {
    int key;
    int x, y, w, h;
    const char *label;  /* NULL = use the localized Backspace string */
    COLORREF color;
} Button;

static Button buttons[] = {
    { K_BACK,   54, 36, 62, 28, NULL,   C_RED },
    { K_CE,    120, 36, 62, 28, "CE",   C_RED },
    { K_C,     186, 36, 62, 28, "C",    C_RED },

    { K_MC,      6,  72, 36, 29, "MC", C_RED },
    { K_MR,      6, 105, 36, 29, "MR", C_RED },
    { K_MS,      6, 138, 36, 29, "MS", C_RED },
    { K_MPLUS,   6, 171, 36, 29, "M+", C_RED },

    { K_7,      52,  72, 36, 29, "7",    C_BLUE },
    { K_8,      92,  72, 36, 29, "8",    C_BLUE },
    { K_9,     132,  72, 36, 29, "9",    C_BLUE },
    { K_DIV,   172,  72, 36, 29, "/",    C_RED  },
    { K_SQRT,  212,  72, 36, 29, "sqrt", C_NAVY },

    { K_4,      52, 105, 36, 29, "4",    C_BLUE },
    { K_5,      92, 105, 36, 29, "5",    C_BLUE },
    { K_6,     132, 105, 36, 29, "6",    C_BLUE },
    { K_MUL,   172, 105, 36, 29, "*",    C_RED  },
    { K_PERCENT,212,105, 36, 29, "%",    C_NAVY },

    { K_1,      52, 138, 36, 29, "1",    C_BLUE },
    { K_2,      92, 138, 36, 29, "2",    C_BLUE },
    { K_3,     132, 138, 36, 29, "3",    C_BLUE },
    { K_SUB,   172, 138, 36, 29, "-",    C_RED  },
    { K_RECIP, 212, 138, 36, 29, "1/x",  C_NAVY },

    { K_0,      52, 171, 36, 29, "0",    C_BLUE },
    { K_NEGATE, 92, 171, 36, 29, "+/-",  C_BLUE },
    { K_DECIMAL,132,171, 36, 29, NULL,   C_BLUE },  /* decimal separator */
    { K_ADD,   172, 171, 36, 29, "+",    C_RED  },
    { K_EQUALS,212, 171, 36, 29, "=",    C_RED  },
};
#define NBUTTONS (sizeof(buttons) / sizeof(buttons[0]))

static const RECT rcDisplay = { 6, 6, 248, 28 };
static const RECT rcMemBox  = { 6, 37, 36, 63 };

/* ---------- Global state ---------- */
static HINSTANCE  g_hInst;
static CalcEngine g_eng;
static HFONT      g_font;
static int        g_strBase = STR_BASE_EN;
static char       g_dec = '.';
static char       g_decLabel[2] = ".";
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
    r.right -= 6;
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
        case IDM_ABOUT: {
            char title[64], text[160];
            LoadStr(IDS_ABOUT_TITLE, title, sizeof(title));
            LoadStr(IDS_ABOUT_TEXT, text, sizeof(text));
            MessageBox(hwnd, text, title, MB_OK | MB_ICONINFORMATION);
            break;
        }
        }
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
    HDC hdc;
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

    hdc = GetDC(NULL);
    g_font = CreateFont(-MulDiv(8, GetDeviceCaps(hdc, LOGPIXELSY), 72), 0, 0, 0,
                        FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                        VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
    ReleaseDC(NULL, hdc);

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
    if (g_extIcon) DestroyIcon(g_extIcon);
    return m.wParam;
}
