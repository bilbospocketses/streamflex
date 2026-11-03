/* probe_spawn pip, Windows: the job's visible top-level windows, a close, a raise from a background process, and the
   foreground window. */
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "pip.h"
#include "spawn.h"

typedef struct {
    probe_win *out;
    int cap, n;
    unsigned long pids[256];
    int npids;
} snap_ctx;

static void utf8_of(const wchar_t *w, char *out, int cap)
{
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL))
        out[0] = '\0';
}

/* EnumWindows callback: adds a visible top-level window owned by one of the job's processes. */
static BOOL CALLBACK add_window(HWND hwnd, LPARAM lp)
{
    snap_ctx *c = (snap_ctx *)lp;
    DWORD pid = 0;
    int ours = 0;
    if (c->n == c->cap || !IsWindowVisible(hwnd))
        return TRUE;
    GetWindowThreadProcessId(hwnd, &pid);
    for (int i = 0; i < c->npids && !ours; i++)
        ours = c->pids[i] == pid;
    if (!ours)
        return TRUE;
    probe_win *w = &c->out[c->n++];
    memset(w, 0, sizeof *w);
    w->id = (uint64_t)(uintptr_t)hwnd;
    w->pid = (long)pid;
    /* Another process's window: GetWindowTextW reads the caption the system keeps and sends no message, so a hung
       browser cannot hang the watch. 160 UTF-16 units fit title's 512 bytes as UTF-8. */
    wchar_t text[160];
    text[0] = L'\0';
    GetWindowTextW(hwnd, text, 160);
    utf8_of(text, w->title, (int)sizeof w->title);
    text[0] = L'\0';
    GetClassNameW(hwnd, text, 40);
    utf8_of(text, w->cls, (int)sizeof w->cls);
    w->topmost = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    /* A minimized window's rect is its small caption bar, so it would lose the main-window ranking to any popup; its
       restored size is the one that counts. */
    RECT r;
    WINDOWPLACEMENT wp;
    wp.length = sizeof wp;
    if (IsIconic(hwnd) && GetWindowPlacement(hwnd, &wp)) {
        r = wp.rcNormalPosition;
        w->area = (long long)(r.right - r.left) * (r.bottom - r.top);
    } else if (GetWindowRect(hwnd, &r)) {
        w->area = (long long)(r.right - r.left) * (r.bottom - r.top);
    }
    return TRUE;
}

static int session(void)
{
    probe_emit("\"event\":\"pip_session\",\"session\":\"windows\",\"can_list\":true,\"can_raise\":true");
    return 1;
}

static int snapshot(probe_win *out, int cap)
{
    static snap_ctx c;
    c.out = out;
    c.cap = cap;
    c.n = 0;
    c.npids = probe_child_pids(c.pids, 256);
    if (c.npids == 0)
        return 0;
    EnumWindows(add_window, (LPARAM)&c);
    return c.n;
}

/* WM_CLOSE is what the window's own close button sends; a browser then puts the video back in its page. Posted, so a
   busy browser cannot hold up the watch. */
static int close_window(uint64_t id, char *method, size_t cap)
{
    snprintf(method, cap, "WM_CLOSE");
    return PostMessageW((HWND)(uintptr_t)id, WM_CLOSE, 0, 0) != 0;
}

/* SetForegroundWindow works for a caller that meets one of its documented conditions; for a background process the
   one within reach is "the calling process received the last input event". One empty mouse INPUT (no flags: it moves
   and presses nothing) makes this process that one. If the call still fails, the fallback joins the foreground
   thread's input state for the call, then leaves it. The method used is reported, so G17 records which one each
   browser needed. The restore and the move to the top are asynchronous (ShowWindowAsync, SWP_ASYNCWINDOWPOS), so a
   hung browser window cannot block run's loop and leave close and kill on stdin unhandled. */
static int raise_window(uint64_t id, char *method, size_t cap)
{
    HWND main = (HWND)(uintptr_t)id;
    if (IsIconic(main))
        ShowWindowAsync(main, SW_RESTORE);
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    UINT sent = SendInput(1, &in, sizeof in);
    if (SetForegroundWindow(main)) {
        snprintf(method, cap, sent == 1 ? "input" : "input_unsent");
        return 1;
    }
    HWND fg = GetForegroundWindow();
    DWORD fg_thread = fg ? GetWindowThreadProcessId(fg, NULL) : 0, me = GetCurrentThreadId();
    BOOL joined = fg_thread && fg_thread != me && AttachThreadInput(me, fg_thread, TRUE);
    SetWindowPos(main, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_ASYNCWINDOWPOS);
    BOOL ok = SetForegroundWindow(main);
    if (joined)
        AttachThreadInput(me, fg_thread, FALSE);
    snprintf(method, cap, joined ? "attach" : "attach_unjoined");
    return ok != 0;
}

static uint64_t foreground(void)
{
    return (uint64_t)(uintptr_t)GetForegroundWindow();
}

const probe_pip_platform probe_pip_platform_impl = { session, snapshot, close_window, raise_window, foreground };
