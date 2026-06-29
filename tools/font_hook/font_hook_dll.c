#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef HFONT (WINAPI *PFN_CreateFontA)(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCSTR);
typedef HFONT (WINAPI *PFN_CreateFontW)(int,int,int,int,int,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,DWORD,LPCWSTR);
typedef HFONT (WINAPI *PFN_CreateFontIndirectA)(const LOGFONTA *);
typedef HFONT (WINAPI *PFN_CreateFontIndirectW)(const LOGFONTW *);
typedef HGDIOBJ (WINAPI *PFN_SelectObject)(HDC,HGDIOBJ);
typedef BOOL (WINAPI *PFN_TextOutA)(HDC,int,int,LPCSTR,int);
typedef BOOL (WINAPI *PFN_TextOutW)(HDC,int,int,LPCWSTR,int);
typedef int (WINAPI *PFN_DrawTextA)(HDC,LPCSTR,int,LPRECT,UINT);
typedef int (WINAPI *PFN_DrawTextW)(HDC,LPCWSTR,int,LPRECT,UINT);

static PFN_CreateFontA real_CreateFontA;
static PFN_CreateFontW real_CreateFontW;
static PFN_CreateFontIndirectA real_CreateFontIndirectA;
static PFN_CreateFontIndirectW real_CreateFontIndirectW;
static PFN_SelectObject real_SelectObject;
static PFN_TextOutA real_TextOutA;
static PFN_TextOutW real_TextOutW;
static PFN_DrawTextA real_DrawTextA;
static PFN_DrawTextW real_DrawTextW;

static CRITICAL_SECTION g_log_lock;
static FILE *g_log;

static void log_line(const char *fmt, ...)
{
    va_list ap;
    if (g_log == NULL) {
        return;
    }
    EnterCriticalSection(&g_log_lock);
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_log_lock);
}

static void log_hfont(const char *tag, HFONT font)
{
    LOGFONTA lf;
    if (font != NULL && GetObjectA(font, sizeof(lf), &lf) == sizeof(lf)) {
        log_line("%s HFONT=%p lfFaceName='%s' height=%ld width=%ld weight=%ld charset=0x%02x quality=%u pitch=0x%02x italic=%u underline=%u",
                 tag,
                 font,
                 lf.lfFaceName,
                 (long)lf.lfHeight,
                 (long)lf.lfWidth,
                 (long)lf.lfWeight,
                 (unsigned)(unsigned char)lf.lfCharSet,
                 (unsigned)(unsigned char)lf.lfQuality,
                 (unsigned)(unsigned char)lf.lfPitchAndFamily,
                 (unsigned)(unsigned char)lf.lfItalic,
                 (unsigned)(unsigned char)lf.lfUnderline);
    } else {
        log_line("%s HFONT=%p GetObjectA failed err=%lu", tag, font, GetLastError());
    }
}

static void log_hdc_face(const char *tag, HDC hdc)
{
    char face[LF_FACESIZE];
    TEXTMETRICA tm;
    face[0] = '\0';
    GetTextFaceA(hdc, sizeof(face), face);
    if (GetTextMetricsA(hdc, &tm)) {
        log_line("%s HDC=%p selectedFace='%s' tmHeight=%ld tmAveWidth=%ld tmWeight=%ld tmCharSet=0x%02x tmPitchFamily=0x%02x",
                 tag,
                 hdc,
                 face,
                 (long)tm.tmHeight,
                 (long)tm.tmAveCharWidth,
                 (long)tm.tmWeight,
                 (unsigned)(unsigned char)tm.tmCharSet,
                 (unsigned)(unsigned char)tm.tmPitchAndFamily);
    } else {
        log_line("%s HDC=%p selectedFace='%s' GetTextMetricsA failed err=%lu", tag, hdc, face, GetLastError());
    }
}

HFONT WINAPI hook_CreateFontA(int h,int w,int e,int o,int weight,DWORD italic,DWORD underline,DWORD strike,DWORD charset,DWORD outp,DWORD clipp,DWORD quality,DWORD pitch,LPCSTR face)
{
    HFONT font = real_CreateFontA(h,w,e,o,weight,italic,underline,strike,charset,outp,clipp,quality,pitch,face);
    log_line("CreateFontA face='%s' height=%d weight=%d charset=0x%02lx quality=%lu pitch=0x%02lx -> %p",
             face ? face : "(null)", h, weight, charset, quality, pitch, font);
    log_hfont("  created", font);
    return font;
}

HFONT WINAPI hook_CreateFontW(int h,int w,int e,int o,int weight,DWORD italic,DWORD underline,DWORD strike,DWORD charset,DWORD outp,DWORD clipp,DWORD quality,DWORD pitch,LPCWSTR face)
{
    HFONT font = real_CreateFontW(h,w,e,o,weight,italic,underline,strike,charset,outp,clipp,quality,pitch,face);
    log_line("CreateFontW height=%d weight=%d charset=0x%02lx quality=%lu pitch=0x%02lx -> %p", h, weight, charset, quality, pitch, font);
    log_hfont("  created", font);
    return font;
}

HFONT WINAPI hook_CreateFontIndirectA(const LOGFONTA *lf)
{
    HFONT font = real_CreateFontIndirectA(lf);
    if (lf != NULL) {
        log_line("CreateFontIndirectA face='%s' height=%ld weight=%ld charset=0x%02x quality=%u -> %p",
                 lf->lfFaceName, (long)lf->lfHeight, (long)lf->lfWeight,
                 (unsigned)(unsigned char)lf->lfCharSet, (unsigned)(unsigned char)lf->lfQuality, font);
    }
    log_hfont("  created", font);
    return font;
}

HFONT WINAPI hook_CreateFontIndirectW(const LOGFONTW *lf)
{
    HFONT font = real_CreateFontIndirectW(lf);
    if (lf != NULL) {
        log_line("CreateFontIndirectW height=%ld weight=%ld charset=0x%02x quality=%u -> %p",
                 (long)lf->lfHeight, (long)lf->lfWeight,
                 (unsigned)(unsigned char)lf->lfCharSet, (unsigned)(unsigned char)lf->lfQuality, font);
    }
    log_hfont("  created", font);
    return font;
}

HGDIOBJ WINAPI hook_SelectObject(HDC hdc, HGDIOBJ obj)
{
    HGDIOBJ old_obj = real_SelectObject(hdc, obj);
    if (GetObjectType(obj) == OBJ_FONT) {
        log_hfont("SelectObject font", (HFONT)obj);
        log_hdc_face("  after select", hdc);
    }
    return old_obj;
}

BOOL WINAPI hook_TextOutA(HDC hdc, int x, int y, LPCSTR s, int c)
{
    log_hdc_face("TextOutA", hdc);
    return real_TextOutA(hdc, x, y, s, c);
}

BOOL WINAPI hook_TextOutW(HDC hdc, int x, int y, LPCWSTR s, int c)
{
    log_hdc_face("TextOutW", hdc);
    return real_TextOutW(hdc, x, y, s, c);
}

int WINAPI hook_DrawTextA(HDC hdc, LPCSTR s, int c, LPRECT r, UINT format)
{
    log_hdc_face("DrawTextA", hdc);
    return real_DrawTextA(hdc, s, c, r, format);
}

int WINAPI hook_DrawTextW(HDC hdc, LPCWSTR s, int c, LPRECT r, UINT format)
{
    log_hdc_face("DrawTextW", hdc);
    return real_DrawTextW(hdc, s, c, r, format);
}

static void patch_iat_entry(void **slot, void *hook, void **real_slot)
{
    DWORD old_protect;
    if (*slot == hook) {
        return;
    }
    if (*real_slot == NULL) {
        *real_slot = *slot;
    }
    if (VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old_protect)) {
        *slot = hook;
        VirtualProtect(slot, sizeof(*slot), old_protect, &old_protect);
    }
}

static void patch_imports(HMODULE module)
{
    unsigned char *base = (unsigned char *)module;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;

    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }
    if (nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0) {
        return;
    }
    imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imp->Name != 0; imp++) {
        IMAGE_THUNK_DATA *orig;
        IMAGE_THUNK_DATA *thunk;
        const char *dll_name = (const char *)(base + imp->Name);
        if (_stricmp(dll_name, "gdi32.dll") != 0 && _stricmp(dll_name, "user32.dll") != 0) {
            continue;
        }
        orig = (IMAGE_THUNK_DATA *)(base + imp->OriginalFirstThunk);
        thunk = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk);
        if (orig == NULL || thunk == NULL) {
            continue;
        }
        for (; orig->u1.AddressOfData != 0; orig++, thunk++) {
            IMAGE_IMPORT_BY_NAME *by_name;
            const char *name;
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                continue;
            }
            by_name = (IMAGE_IMPORT_BY_NAME *)(base + orig->u1.AddressOfData);
            name = (const char *)by_name->Name;
            if (strcmp(name, "CreateFontA") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_CreateFontA, (void **)&real_CreateFontA);
            else if (strcmp(name, "CreateFontW") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_CreateFontW, (void **)&real_CreateFontW);
            else if (strcmp(name, "CreateFontIndirectA") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_CreateFontIndirectA, (void **)&real_CreateFontIndirectA);
            else if (strcmp(name, "CreateFontIndirectW") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_CreateFontIndirectW, (void **)&real_CreateFontIndirectW);
            else if (strcmp(name, "SelectObject") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_SelectObject, (void **)&real_SelectObject);
            else if (strcmp(name, "TextOutA") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_TextOutA, (void **)&real_TextOutA);
            else if (strcmp(name, "TextOutW") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_TextOutW, (void **)&real_TextOutW);
            else if (strcmp(name, "DrawTextA") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_DrawTextA, (void **)&real_DrawTextA);
            else if (strcmp(name, "DrawTextW") == 0) patch_iat_entry((void **)&thunk->u1.Function, hook_DrawTextW, (void **)&real_DrawTextW);
        }
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH];
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        InitializeCriticalSection(&g_log_lock);
        if (GetEnvironmentVariableA("RICH4_FONT_HOOK_LOG", path, sizeof(path)) == 0) {
            lstrcpynA(path, "font_hook.log", sizeof(path));
        }
        g_log = fopen(path, "a");
        log_line("=== font_hook attached pid=%lu ===", GetCurrentProcessId());
        patch_imports(GetModuleHandleA(NULL));
    } else if (reason == DLL_PROCESS_DETACH) {
        log_line("=== font_hook detached pid=%lu ===", GetCurrentProcessId());
        if (g_log != NULL) {
            fclose(g_log);
            g_log = NULL;
        }
        DeleteCriticalSection(&g_log_lock);
    }
    return TRUE;
}
