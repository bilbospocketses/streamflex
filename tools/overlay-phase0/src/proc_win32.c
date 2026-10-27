/* probe_spawn run, Windows: a Job Object with a completion port around a suspended start. */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "spawn.h"

static HANDLE g_job;
static volatile LONG g_close, g_kill; /* stdin commands not yet handled */

static void note_command(int cmd, void *unused)
{
    (void)unused;
    InterlockedExchange(cmd == PROBE_CMD_CLOSE ? &g_close : &g_kill, 1);
}

/* Reads stdin raw (no CRT stream lock), so a read still pending at exit holds nothing up. */
static DWORD WINAPI stdin_thread(LPVOID unused)
{
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    probe_lines lines;
    char buf[256];
    DWORD n;
    (void)unused;
    memset(&lines, 0, sizeof lines);
    while (ReadFile(in, buf, sizeof buf, &n, NULL) && n > 0)
        probe_lines_feed(&lines, buf, n, note_command, NULL);
    return 0;
}

/* EnumWindows callback: posts WM_CLOSE to a visible top-level window of a job process; count is an int *. */
static BOOL CALLBACK close_if_in_job(HWND hwnd, LPARAM count)
{
    DWORD pid = 0;
    if (!IsWindowVisible(hwnd))
        return TRUE;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h) {
        BOOL in = FALSE;
        if (IsProcessInJob(h, g_job, &in) && in && PostMessageW(hwnd, WM_CLOSE, 0, 0))
            ++*(int *)count;
        CloseHandle(h);
    }
    return TRUE;
}

/* The job message ACTIVE_PROCESS_ZERO is not guaranteed to arrive, so the loop also asks the job itself. */
static int job_is_empty(void)
{
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION acct;
    return QueryInformationJobObject(g_job, JobObjectBasicAccountingInformation, &acct, sizeof acct, NULL)
        && acct.ActiveProcesses == 0;
}

static int put(wchar_t *out, size_t cap, size_t *len, wchar_t c)
{
    if (*len + 1 >= cap)
        return 0;
    out[(*len)++] = c;
    out[*len] = L'\0';
    return 1;
}

/* Appends one argument to the command line, quoted by the rules CommandLineToArgvW reverses. Returns 0, leaving
   the line cut short, when the argument or the line does not fit. */
static int append_arg(wchar_t *out, size_t cap, const char *utf8)
{
    wchar_t w[4096];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 4096))
        return 0;
    size_t len = wcslen(out);
    if (len && !put(out, cap, &len, L' '))
        return 0;
    if (!put(out, cap, &len, L'"'))
        return 0;
    size_t slashes = 0;
    for (const wchar_t *p = w; *p; p++) {
        if (*p == L'\\') {
            slashes++;
            if (!put(out, cap, &len, L'\\'))
                return 0;
            continue;
        }
        if (*p == L'"') {
            for (size_t i = 0; i < slashes + 1; i++)
                if (!put(out, cap, &len, L'\\'))
                    return 0;
        }
        slashes = 0;
        if (!put(out, cap, &len, *p))
            return 0;
    }
    for (size_t i = 0; i < slashes; i++)
        if (!put(out, cap, &len, L'\\'))
            return 0;
    return put(out, cap, &len, L'"');
}

/* Reports a setup step that failed and ends the suspended child, if there is one. Returns run's exit code. */
static int spawn_failed(const char *step, DWORD error, HANDLE child)
{
    char fields[160];
    if (child)
        TerminateProcess(child, 1);
    snprintf(fields, sizeof fields, "\"event\":\"spawn_failed\",\"step\":\"%s\",\"error\":%lu", step, error);
    probe_emit(fields);
    return 1;
}

/* The child's standard handles, all inheritable: stdin from NUL, so the child never shares run's command pipe,
   and stdout and stderr onto probe_spawn's stderr (NUL when it has none), so nothing lands among the JSON lines. */
static int child_handles(HANDLE *nul, HANDLE *err)
{
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    *nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                       0, NULL);
    if (*nul == INVALID_HANDLE_VALUE)
        return 0;
    *err = NULL;
    HANDLE mine = GetStdHandle(STD_ERROR_HANDLE);
    if (mine && mine != INVALID_HANDLE_VALUE
        && !DuplicateHandle(GetCurrentProcess(), mine, GetCurrentProcess(), err, 0, TRUE, DUPLICATE_SAME_ACCESS))
        *err = NULL;
    return 1;
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
        if (!append_arg(cmdline, sizeof cmdline / sizeof cmdline[0], argv[i]))
            return spawn_failed("command_line", ERROR_INSUFFICIENT_BUFFER, NULL);

    g_job = CreateJobObjectW(NULL, NULL);
    if (!g_job)
        return spawn_failed("CreateJobObject", GetLastError(), NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim;
    memset(&lim, 0, sizeof lim);
    lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &lim, sizeof lim))
        return spawn_failed("SetInformationJobObject_limits", GetLastError(), NULL);
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (!port)
        return spawn_failed("CreateIoCompletionPort", GetLastError(), NULL);
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT acp;
    acp.CompletionKey = (PVOID)1;
    acp.CompletionPort = port;
    if (!SetInformationJobObject(g_job, JobObjectAssociateCompletionPortInformation, &acp, sizeof acp))
        return spawn_failed("SetInformationJobObject_port", GetLastError(), NULL);

    /* Inherit exactly the child's standard handles: bInheritHandles TRUE, narrowed by a handle list. */
    HANDLE nul, err;
    if (!child_handles(&nul, &err))
        return spawn_failed("open_NUL", GetLastError(), NULL);
    HANDLE inherit[2] = { nul, err };
    SIZE_T attr_size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attr_size); /* fails by design: it only sizes the list */
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = malloc(attr_size);
    if (!attrs)
        return spawn_failed("InitializeProcThreadAttributeList", ERROR_NOT_ENOUGH_MEMORY, NULL);
    if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size))
        return spawn_failed("InitializeProcThreadAttributeList", GetLastError(), NULL);
    if (!UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit,
                                   (err ? 2 : 1) * sizeof(HANDLE), NULL, NULL))
        return spawn_failed("UpdateProcThreadAttribute", GetLastError(), NULL);

    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul;
    si.StartupInfo.hStdOutput = err ? err : nul;
    si.StartupInfo.hStdError = err ? err : nul;
    si.lpAttributeList = attrs;
    BOOL made = CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                               NULL, NULL, &si.StartupInfo, &pi);
    DWORD made_error = GetLastError();
    DeleteProcThreadAttributeList(attrs);
    free(attrs);
    CloseHandle(nul);
    if (err)
        CloseHandle(err);
    if (!made)
        return spawn_failed("CreateProcess", made_error, NULL);

    /* Never resume a child that is not in the job. */
    if (!AssignProcessToJobObject(g_job, pi.hProcess))
        return spawn_failed("AssignProcessToJobObject", GetLastError(), pi.hProcess);
    HANDLE reader = CreateThread(NULL, 0, stdin_thread, NULL, 0, NULL);
    if (!reader)
        return spawn_failed("CreateThread", GetLastError(), pi.hProcess);
    CloseHandle(reader);
    if (ResumeThread(pi.hThread) == (DWORD)-1)
        return spawn_failed("ResumeThread", GetLastError(), pi.hProcess);
    CloseHandle(pi.hThread);
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%lu", pi.dwProcessId);
    probe_emit(fields);

    int closed = 0;
    ULONGLONG t_close = 0; /* GetTickCount64 of the last close: the interval runs on a monotonic clock */
    for (;;) {
        DWORD code;
        ULONG_PTR key;
        LPOVERLAPPED ov;
        if (GetQueuedCompletionStatus(port, &code, &key, &ov, 50) && code == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)
            break;
        if (job_is_empty())
            break;
        if (InterlockedExchange(&g_close, 0)) {
            int windows = 0;
            closed = 1;
            t_close = GetTickCount64();
            EnumWindows(close_if_in_job, (LPARAM)&windows);
            snprintf(fields, sizeof fields, "\"event\":\"close_sent\",\"windows\":%d", windows);
            probe_emit(fields);
        }
        if (InterlockedExchange(&g_kill, 0)) {
            TerminateJobObject(g_job, 137);
            probe_emit("\"event\":\"kill_sent\"");
        }
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    long long since = closed ? (long long)(GetTickCount64() - t_close) : -1;
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%lu,\"ms_since_close\":%lld", exit_code, since);
    probe_emit(fields);
    return 0;
}
