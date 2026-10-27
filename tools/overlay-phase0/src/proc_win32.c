/* probe_spawn run, Windows: a Job Object with a completion port around a suspended start. */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "spawn.h"

static HANDLE g_job;
static volatile LONG g_cmd; /* 0 none, 1 close, 2 kill */

static DWORD WINAPI stdin_thread(LPVOID unused)
{
    char line[64];
    (void)unused;
    while (fgets(line, sizeof line, stdin)) {
        if (strncmp(line, "close", 5) == 0)
            InterlockedExchange(&g_cmd, 1);
        else if (strncmp(line, "kill", 4) == 0)
            InterlockedExchange(&g_cmd, 2);
    }
    return 0;
}

static BOOL CALLBACK close_if_in_job(HWND hwnd, LPARAM unused)
{
    DWORD pid = 0;
    (void)unused;
    if (!IsWindowVisible(hwnd))
        return TRUE;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h) {
        BOOL in = FALSE;
        if (IsProcessInJob(h, g_job, &in) && in)
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        CloseHandle(h);
    }
    return TRUE;
}

/* Appends one argument to the command line, quoted by the rules CommandLineToArgvW reverses. */
static void append_arg(wchar_t *out, size_t cap, const char *utf8)
{
    wchar_t w[4096];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 4096))
        w[0] = L'\0';
    size_t len = wcslen(out);
    if (len && len + 1 < cap)
        out[len++] = L' ';
    if (len + 1 < cap)
        out[len++] = L'"';
    size_t slashes = 0;
    for (const wchar_t *p = w; *p && len + 3 < cap; p++) {
        if (*p == L'\\') {
            slashes++;
            out[len++] = L'\\';
            continue;
        }
        if (*p == L'"') {
            for (size_t i = 0; i < slashes + 1 && len + 2 < cap; i++)
                out[len++] = L'\\';
        }
        slashes = 0;
        out[len++] = *p;
    }
    for (size_t i = 0; i < slashes && len + 2 < cap; i++)
        out[len++] = L'\\';
    if (len + 1 < cap)
        out[len++] = L'"';
    out[len] = L'\0';
}

int probe_run(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn run <program> [args...]\n", stderr);
        return 2;
    }
    static wchar_t cmdline[32768];
    cmdline[0] = L'\0';
    for (int i = 0; i < argc; i++)
        append_arg(cmdline, sizeof cmdline / sizeof cmdline[0], argv[i]);

    g_job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim;
    memset(&lim, 0, sizeof lim);
    lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &lim, sizeof lim);
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT acp;
    acp.CompletionKey = (PVOID)1;
    acp.CompletionPort = port;
    SetInformationJobObject(g_job, JobObjectAssociateCompletionPortInformation, &acp, sizeof acp);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    char fields[160];
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        snprintf(fields, sizeof fields, "\"event\":\"spawn_failed\",\"error\":%lu", GetLastError());
        probe_emit(fields);
        return 1;
    }
    AssignProcessToJobObject(g_job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%lu", pi.dwProcessId);
    probe_emit(fields);
    CreateThread(NULL, 0, stdin_thread, NULL, 0, NULL);

    int64_t t_close = 0;
    for (;;) {
        DWORD code;
        ULONG_PTR key;
        LPOVERLAPPED ov;
        if (GetQueuedCompletionStatus(port, &code, &key, &ov, 50) && code == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)
            break;
        LONG cmd = InterlockedExchange(&g_cmd, 0);
        if (cmd == 1) {
            t_close = probe_now_ms();
            EnumWindows(close_if_in_job, 0);
            probe_emit("\"event\":\"close_sent\"");
        } else if (cmd == 2) {
            TerminateJobObject(g_job, 137);
            probe_emit("\"event\":\"kill_sent\"");
        }
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    long long since = t_close ? (long long)(probe_now_ms() - t_close) : -1;
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%lu,\"ms_since_close\":%lld", exit_code, since);
    probe_emit(fields);
    return 0;
}
