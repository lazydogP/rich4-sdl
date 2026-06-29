#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_last_error(const char *what)
{
    DWORD err = GetLastError();
    char msg[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   err,
                   0,
                   msg,
                   sizeof(msg),
                   NULL);
    fprintf(stderr, "error: %s failed [%lu]: %s\n", what, err, msg);
}

static void quote_arg(char *out, size_t out_size, const char *arg)
{
    int need_quotes = strchr(arg, ' ') != NULL || strchr(arg, '\t') != NULL || strchr(arg, '"') != NULL;
    size_t used = strlen(out);
    const char *p;
    if (used + 2 >= out_size) {
        return;
    }
    if (used != 0) {
        out[used++] = ' ';
        out[used] = '\0';
    }
    if (need_quotes && used + 1 < out_size) {
        out[used++] = '"';
        out[used] = '\0';
    }
    for (p = arg; *p != '\0' && used + 2 < out_size; p++) {
        if (*p == '"') {
            out[used++] = '\\';
        }
        out[used++] = *p;
        out[used] = '\0';
    }
    if (need_quotes && used + 1 < out_size) {
        out[used++] = '"';
        out[used] = '\0';
    }
}

static int dirname_of(const char *path, char *out, size_t out_size)
{
    const char *slash1 = strrchr(path, '\\');
    const char *slash2 = strrchr(path, '/');
    const char *slash = slash1 > slash2 ? slash1 : slash2;
    size_t len;
    if (slash == NULL) {
        GetCurrentDirectoryA((DWORD)out_size, out);
        return 1;
    }
    len = (size_t)(slash - path);
    if (len + 1 > out_size) {
        return 0;
    }
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH];
    char self_dir[MAX_PATH];
    char dll_path[MAX_PATH];
    char log_path[MAX_PATH];
    char work_dir[MAX_PATH];
    char cmdline[8192] = "";
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    LPVOID remote_mem;
    HANDLE thread;
    DWORD wait_code;
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: font_hook_launcher.exe <path-to-rich4.exe> [args...]\n");
        return 2;
    }

    GetModuleFileNameA(NULL, self, sizeof(self));
    if (!dirname_of(self, self_dir, sizeof(self_dir))) {
        fprintf(stderr, "error: launcher path is too long\n");
        return 1;
    }
    snprintf(dll_path, sizeof(dll_path), "%s\\font_hook.dll", self_dir);
    snprintf(log_path, sizeof(log_path), "%s\\font_hook.log", self_dir);
    SetEnvironmentVariableA("RICH4_FONT_HOOK_LOG", log_path);

    for (i = 1; i < argc; i++) {
        quote_arg(cmdline, sizeof(cmdline), argv[i]);
    }
    if (!dirname_of(argv[1], work_dir, sizeof(work_dir))) {
        fprintf(stderr, "error: target path is too long\n");
        return 1;
    }

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessA(argv[1],
                        cmdline,
                        NULL,
                        NULL,
                        FALSE,
                        CREATE_SUSPENDED,
                        NULL,
                        work_dir,
                        &si,
                        &pi)) {
        print_last_error("CreateProcessA");
        return 1;
    }

    remote_mem = VirtualAllocEx(pi.hProcess, NULL, strlen(dll_path) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remote_mem == NULL) {
        print_last_error("VirtualAllocEx");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    if (!WriteProcessMemory(pi.hProcess, remote_mem, dll_path, strlen(dll_path) + 1, NULL)) {
        print_last_error("WriteProcessMemory");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    thread = CreateRemoteThread(pi.hProcess,
                                NULL,
                                0,
                                (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"),
                                remote_mem,
                                0,
                                NULL);
    if (thread == NULL) {
        print_last_error("CreateRemoteThread");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    wait_code = WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    VirtualFreeEx(pi.hProcess, remote_mem, 0, MEM_RELEASE);
    if (wait_code != WAIT_OBJECT_0) {
        fprintf(stderr, "error: DLL injection timed out\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    printf("font hook injected. log: %s\n", log_path);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
