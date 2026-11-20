/*
 * setup.c - Installer and uninstaller for win31-calc95 (Win16, Windows 3.1).
 * Built twice: SETUP.EXE (default) and UNINSTAL.EXE (-DUNINSTALL).
 * Both run from the floppy and are not copied to the system.
 *
 * Install:
 *   1. <WINDOWS>\CALC.EXE (original) is renamed to CALC31.EXE.
 *      If CALC.EXE is already ours (re-install), it is only updated.
 *   2. CALC95.EXE from the floppy is copied as <WINDOWS>\CALC.EXE.
 *   3. The Program Manager item that runs CALC.EXE is replaced in place
 *      (DDE ReplaceItem keeps its position) with the icon of CALC31.EXE.
 *   4. Everything needed to undo it is recorded in WIN.INI [Calc95].
 * Uninstall reverts each step and removes [Calc95].
 */
#include <windows.h>
#include <ddeml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupres.h"

#define REC "Calc95"                 /* WIN.INI section with the install record */
#define NONE_MARK "\001"             /* "key was not present" marker */

static HINSTANCE g_hInst;
static int  g_strBase = STR_EN;
static char g_title[64];
static char g_winDir[144];

/* ---------- strings ---------- */
static void LoadStr(int id, char *buf, int len)
{
    if (!LoadString(g_hInst, g_strBase + id, buf, len))
        LoadString(g_hInst, STR_EN + id, buf, len);
}

static int Msg(int id, UINT flags)
{
    char text[400];
    LoadStr(id, text, sizeof(text));
    return MessageBox(NULL, text, g_title, flags | MB_TASKMODAL);
}

static void DetectLocale(void)
{
    char buf[16];
    GetProfileString("intl", "sLanguage", "enu", buf, sizeof(buf));
    if ((buf[0] == 'e' || buf[0] == 'E') && (buf[1] == 's' || buf[1] == 'S'))
        g_strBase = STR_ES;
}

/* ---------- files ---------- */
static void WinPath(char *out, const char *name)
{
    strcpy(out, g_winDir);
    if (out[strlen(out) - 1] != '\\') strcat(out, "\\");
    strcat(out, name);
}

static int FileExists(const char *path)
{
    OFSTRUCT of;
    return OpenFile(path, &of, OF_EXIST) != HFILE_ERROR;
}

/* Our calculator has the module name CALC95 in its resident names table */
static int IsOurCalc(const char *path)
{
    static char buf[4096];
    FILE *f = fopen(path, "rb");
    size_t n, i;
    if (!f) return 0;
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    for (i = 0; i + 7 <= n; i++)
        if (buf[i] == 6 && memcmp(buf + i + 1, "CALC95", 6) == 0) return 1;
    return 0;
}

#ifndef UNINSTALL
static int CopyFileTo(const char *src, const char *dst)
{
    static char buf[2048];
    FILE *in, *out;
    size_t n;
    int ok = 1;
    in = fopen(src, "rb");
    if (!in) return 0;
    out = fopen(dst, "wb");
    if (!out) { fclose(in); return 0; }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = 0; break; }
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    if (!ok) remove(dst);
    return ok;
}

#endif

/* Waits until no calculator (original or ours) is running */
static int CalculatorClosed(void)
{
    while (GetModuleHandle("CALC") || GetModuleHandle("CALC95")) {
        if (Msg(IDS_CLOSE_CALC, MB_RETRYCANCEL | MB_ICONEXCLAMATION) != IDRETRY) return 0;
    }
    return 1;
}

/* ---------- Program Manager (DDEML) ---------- */
static DWORD   g_dde;
static HCONV   g_conv;
static FARPROC g_ddeProc;

HDDEDATA CALLBACK __export DdeCallback(UINT type, UINT fmt, HCONV conv, HSZ h1, HSZ h2,
                                       HDDEDATA data, DWORD d1, DWORD d2)
{
    (void)type; (void)fmt; (void)conv; (void)h1; (void)h2; (void)data; (void)d1; (void)d2;
    return (HDDEDATA)NULL;
}

static int PmOpen(void)
{
    HSZ hsz;
    g_ddeProc = MakeProcInstance((FARPROC)DdeCallback, g_hInst);
    if (DdeInitialize(&g_dde, (PFNCALLBACK)g_ddeProc, APPCMD_CLIENTONLY, 0L) != DMLERR_NO_ERROR)
        return 0;
    hsz = DdeCreateStringHandle(g_dde, "PROGMAN", CP_WINANSI);
    g_conv = DdeConnect(g_dde, hsz, hsz, NULL);
    DdeFreeStringHandle(g_dde, hsz);
    return g_conv != NULL;
}

static void PmClose(void)
{
    if (g_conv) DdeDisconnect(g_conv);
    if (g_dde) DdeUninitialize(g_dde);
    if (g_ddeProc) FreeProcInstance(g_ddeProc);
    g_conv = NULL; g_dde = 0; g_ddeProc = NULL;
}

#ifndef UNINSTALL
/* Requests a text item; returns a global handle (caller frees) or NULL */
static HGLOBAL PmRequest(const char *item)
{
    HSZ hsz = DdeCreateStringHandle(g_dde, item, CP_WINANSI);
    HDDEDATA h = DdeClientTransaction(NULL, 0, g_conv, hsz, CF_TEXT, XTYP_REQUEST, 10000, NULL);
    HGLOBAL mem = NULL;
    DWORD size;
    DdeFreeStringHandle(g_dde, hsz);
    if (!h) return NULL;
    size = DdeGetData(h, NULL, 0, 0);
    mem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size + 1);
    if (mem) {
        LPSTR p = GlobalLock(mem);
        DdeGetData(h, (LPBYTE)p, size, 0);
        p[size] = '\0';
        GlobalUnlock(mem);
    }
    DdeFreeDataHandle(h);
    return mem;
}

#endif

static int PmExec(const char *cmd)
{
    HDDEDATA h = DdeClientTransaction((LPBYTE)cmd, (DWORD)strlen(cmd) + 1, g_conv, NULL, 0,
                                      XTYP_EXECUTE, 10000, NULL);
    if (h) DdeFreeDataHandle(h);
    return h != NULL;
}

#ifndef UNINSTALL
/* Copies line 'n' (0-based, CR/LF separated) of a far text into buf */
static int GetLine(LPSTR text, int n, char *buf, int len)
{
    int i = 0;
    while (n > 0 && *text) {
        if (*text == '\n') n--;
        text++;
    }
    if (!*text) return 0;
    while (*text && *text != '\r' && *text != '\n' && i < len - 1) buf[i++] = *text++;
    buf[i] = '\0';
    return 1;
}

/* Field 'n' of a comma-separated line with optional "quotes" */
static void GetField(const char *line, int n, char *out, int len)
{
    int i = 0, q = 0;
    while (n > 0 && *line) {
        if (*line == '"') q = !q;
        else if (*line == ',' && !q) n--;
        line++;
    }
    while (*line && (q || *line != ',') && i < len - 1) {
        if (*line == '"') q = !q;
        else out[i++] = *line;
        line++;
    }
    out[i] = '\0';
}

#endif

#ifndef UNINSTALL
/* Does this command line run CALC.EXE (with or without path/extension)? */
static int RunsCalc(const char *cmd)
{
    char exe[144], *base, *sp;
    strncpy(exe, cmd, sizeof(exe) - 1);
    exe[sizeof(exe) - 1] = '\0';
    sp = strchr(exe, ' ');
    if (sp) *sp = '\0';
    base = strrchr(exe, '\\');
    base = base ? base + 1 : exe;
    return lstrcmpi(base, "CALC.EXE") == 0 || lstrcmpi(base, "CALC") == 0;
}

typedef struct {
    char group[64], item[64], cmd[144], icon[144];
    int  iconIndex;
} PmItem;

/* Looks for the item that runs CALC.EXE in every group */
static int PmFindCalc(PmItem *it)
{
    HGLOBAL groups = PmRequest("Groups"), info;
    LPSTR g, gi;
    char name[64], line[400], f[144];
    int gi_n, n, found = 0;

    if (!groups) return 0;
    g = GlobalLock(groups);
    for (gi_n = 0; !found && GetLine(g, gi_n, name, sizeof(name)); gi_n++) {
        if (!name[0]) continue;
        info = PmRequest(name);
        if (!info) continue;
        gi = GlobalLock(info);
        for (n = 1; GetLine(gi, n, line, sizeof(line)); n++) {   /* line 0 = group header */
            GetField(line, 1, f, sizeof(f));
            if (RunsCalc(f)) {
                strcpy(it->group, name);
                GetField(line, 0, it->item, sizeof(it->item));
                strcpy(it->cmd, f);
                GetField(line, 3, it->icon, sizeof(it->icon));
                GetField(line, 6, f, sizeof(f));
                it->iconIndex = atoi(f);
                found = 1;
                break;
            }
        }
        GlobalUnlock(info);
        GlobalFree(info);
    }
    GlobalUnlock(groups);
    GlobalFree(groups);
    return found;
}

/* Finds the Accessories group by name (English or Spanish) */
static int PmFindAccessories(char *out, int len)
{
    HGLOBAL groups = PmRequest("Groups");
    LPSTR g;
    char name[64];
    int n, found = 0;
    if (!groups) return 0;
    g = GlobalLock(groups);
    for (n = 0; GetLine(g, n, name, sizeof(name)); n++) {
        if (lstrcmpi(name, "Accesorios") == 0 || lstrcmpi(name, "Accessories") == 0) {
            strncpy(out, name, len - 1); out[len - 1] = '\0';
            found = 1; break;
        }
    }
    GlobalUnlock(groups);
    GlobalFree(groups);
    return found;
}

#endif

#ifndef UNINSTALL
/* ====================================================================== */
/*                                 SETUP                                   */
/* ====================================================================== */
static int Install(void)
{
    char src[144], dst[144], bak[144], buf[400], val[64];
    int upgrade, backup = 0;
    PmItem it;

    GetModuleFileName(g_hInst, src, sizeof(src));
    strcpy(strrchr(src, '\\') + 1, "CALC95.EXE");
    if (!FileExists(src)) { Msg(IDS_NO_SOURCE, MB_OK | MB_ICONSTOP); return 0; }

    WinPath(dst, "CALC.EXE");
    WinPath(bak, "CALC31.EXE");
    upgrade = FileExists(dst) && IsOurCalc(dst);

    if (!upgrade && FileExists(bak)) {        /* never overwrite an existing backup */
        Msg(IDS_BACKUP_EXISTS, MB_OK | MB_ICONSTOP);
        return 0;
    }
    if (Msg(upgrade ? IDS_CONFIRM_UPGRADE : IDS_CONFIRM_INSTALL, MB_YESNO | MB_ICONQUESTION) != IDYES)
        return 0;
    if (!CalculatorClosed()) return 0;

    /* 1. keep the original as CALC31.EXE */
    if (!upgrade && FileExists(dst)) {
        if (rename(dst, bak) != 0) { Msg(IDS_ERR_RENAME, MB_OK | MB_ICONSTOP); return 0; }
        backup = 1;
    }
    /* 2. copy the new calculator */
    if (!CopyFileTo(src, dst)) {
        if (backup) rename(bak, dst);         /* roll back */
        Msg(IDS_ERR_COPY, MB_OK | MB_ICONSTOP);
        return 0;
    }
    if (upgrade) { Msg(IDS_DONE_UPGRADE, MB_OK | MB_ICONINFORMATION); return 1; }

    /* 4. record what is needed to undo it */
    WriteProfileString(REC, "Backup", backup ? "1" : "0");
    GetProfileString("SciCalc", "layout", NONE_MARK, val, sizeof(val));
    WriteProfileString(REC, "SciLayout", strcmp(val, NONE_MARK) == 0 ? "-" : val);
    GetProfileString("SciCalc", NULL, "", buf, sizeof(buf));     /* list of keys */
    WriteProfileString(REC, "SciSection", buf[0] ? "1" : "0");

    /* 3. Program Manager item */
    if (PmOpen()) {
        if (PmFindCalc(&it)) {
            sprintf(buf, "[ShowGroup(\"%s\",5)][ReplaceItem(\"%s\")][AddItem(\"%s\",\"%s\",\"%s\",0)]",
                    it.group, it.item, it.cmd, it.item, bak);
            PmExec(buf);
            WriteProfileString(REC, "Group", it.group);
            WriteProfileString(REC, "Item", it.item);
            WriteProfileString(REC, "Cmd", it.cmd);
            WriteProfileString(REC, "Icon", it.icon[0] ? it.icon : "-");
            sprintf(val, "%d", it.iconIndex);
            WriteProfileString(REC, "IconIndex", val);
            WriteProfileString(REC, "Created", "0");
        } else if (PmFindAccessories(it.group, sizeof(it.group))) {
            LoadStr(IDS_ITEM_NAME, it.item, sizeof(it.item));
            sprintf(buf, "[ShowGroup(\"%s\",5)][AddItem(\"%s\",\"%s\",\"%s\",0)]",
                    it.group, dst, it.item, backup ? bak : dst);
            PmExec(buf);
            WriteProfileString(REC, "Group", it.group);
            WriteProfileString(REC, "Item", it.item);
            WriteProfileString(REC, "Created", "1");
        } else {
            Msg(IDS_NO_GROUP, MB_OK | MB_ICONINFORMATION);
        }
        PmClose();
    } else {
        Msg(IDS_NO_PROGMAN, MB_OK | MB_ICONINFORMATION);
    }
    WriteProfileString(REC, "Installed", "1");
    Msg(IDS_DONE_INSTALL, MB_OK | MB_ICONINFORMATION);
    return 1;
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    (void)hPrev; (void)cmd; (void)show;
    g_hInst = hInst;
    DetectLocale();
    LoadStr(IDS_SETUP_TITLE, g_title, sizeof(g_title));
    GetWindowsDirectory(g_winDir, sizeof(g_winDir));
    Install();
    return 0;
}

#else
/* ====================================================================== */
/*                               UNINSTALL                                 */
/* ====================================================================== */
static int Uninstall(void)
{
    char dst[144], bak[144], tmp[144], buf[480], group[64], item[64], cmd[144], icon[144], val[64];
    int backup, created;

    if (!GetProfileInt(REC, "Installed", 0)) { Msg(IDS_NOT_INSTALLED, MB_OK | MB_ICONINFORMATION); return 0; }
    WinPath(dst, "CALC.EXE");
    WinPath(bak, "CALC31.EXE");
    backup = GetProfileInt(REC, "Backup", 0);

    if (FileExists(dst) && !IsOurCalc(dst)) {     /* someone already put another CALC.EXE there */
        Msg(IDS_NOT_OURS, MB_OK | MB_ICONSTOP);
        return 0;
    }
    if (backup && !FileExists(bak)) { Msg(IDS_NO_BACKUP, MB_OK | MB_ICONSTOP); return 0; }
    if (Msg(IDS_CONFIRM_UNINSTALL, MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
    if (!CalculatorClosed()) return 0;

    /* files: move ours aside first, so any failure can be rolled back */
    WinPath(tmp, "CALC95.$$$");
    remove(tmp);
    if (FileExists(dst) && rename(dst, tmp) != 0) { Msg(IDS_ERR_DELETE, MB_OK | MB_ICONSTOP); return 0; }
    if (backup && rename(bak, dst) != 0) {
        rename(tmp, dst);
        Msg(IDS_ERR_RENAME, MB_OK | MB_ICONSTOP);
        return 0;
    }
    remove(tmp);

    /* Program Manager item */
    GetProfileString(REC, "Group", "", group, sizeof(group));
    GetProfileString(REC, "Item", "", item, sizeof(item));
    created = GetProfileInt(REC, "Created", 0);
    if (group[0] && item[0] && PmOpen()) {
        if (created) {
            sprintf(buf, "[ShowGroup(\"%s\",5)][DeleteItem(\"%s\")]", group, item);
        } else {
            GetProfileString(REC, "Cmd", "CALC.EXE", cmd, sizeof(cmd));
            GetProfileString(REC, "Icon", "-", icon, sizeof(icon));
            if (strcmp(icon, "-") == 0)
                sprintf(buf, "[ShowGroup(\"%s\",5)][ReplaceItem(\"%s\")][AddItem(\"%s\",\"%s\")]",
                        group, item, cmd, item);
            else
                sprintf(buf, "[ShowGroup(\"%s\",5)][ReplaceItem(\"%s\")][AddItem(\"%s\",\"%s\",\"%s\",%d)]",
                        group, item, cmd, item, icon, GetProfileInt(REC, "IconIndex", 0));
        }
        PmExec(buf);
        PmClose();
    }

    /* WIN.INI: [SciCalc] layout as it was, and no install record */
    GetProfileString(REC, "SciLayout", "-", val, sizeof(val));
    WriteProfileString("SciCalc", "layout", strcmp(val, "-") == 0 ? NULL : (LPSTR)val);
    GetProfileString("SciCalc", NULL, "", buf, sizeof(buf));
    if (!buf[0] && !GetProfileInt(REC, "SciSection", 0))
        WriteProfileString("SciCalc", NULL, NULL);                 /* the section was not there */
    WriteProfileString(REC, NULL, NULL);

    Msg(IDS_DONE_UNINSTALL, MB_OK | MB_ICONINFORMATION);
    return 1;
}

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    (void)hPrev; (void)cmd; (void)show;
    g_hInst = hInst;
    DetectLocale();
    LoadStr(IDS_UNINST_TITLE, g_title, sizeof(g_title));
    GetWindowsDirectory(g_winDir, sizeof(g_winDir));
    Uninstall();
    return 0;
}
#endif
