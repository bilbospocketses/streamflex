/* probe_spawn keys and keylog, Windows: SendInput, and raw input from a message-only window. */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

static int g_send_failed;

static void send_key(const probe_key *k, int up)
{
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = k->vk;
    in.ki.wScan = (WORD)MapVirtualKeyW(k->vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = (k->extended ? KEYEVENTF_EXTENDEDKEY : 0) | (up ? KEYEVENTF_KEYUP : 0);
    if (SendInput(1, &in, sizeof in) != 1) {
        char fields[160];
        snprintf(fields, sizeof fields, "\"event\":\"send_failed\",\"name\":\"%s\",\"up\":%s,\"error\":%lu", k->name,
                 up ? "true" : "false", GetLastError());
        probe_emit(fields);
        g_send_failed = 1;
    }
}

static void sleep_ms(long ms)
{
    Sleep((DWORD)ms);
}

static const probe_key_sender sender = { NULL, send_key, sleep_ms, NULL };

int probe_keys(int argc, char **argv)
{
    int rc = probe_keys_main(argc, argv, &sender);
    return rc ? rc : g_send_failed;
}

static LRESULT CALLBACK keylog_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg != WM_INPUT)
        return DefWindowProcW(hwnd, msg, wp, lp);
    UINT size = 0;
    GetRawInputData((HRAWINPUT)lp, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
    RAWINPUT *ri = malloc(size);
    char fields[512];
    if (ri && GetRawInputData((HRAWINPUT)lp, RID_INPUT, ri, &size, sizeof(RAWINPUTHEADER)) == size) {
        if (ri->header.dwType == RIM_TYPEKEYBOARD) {
            snprintf(fields, sizeof fields, "\"event\":\"raw_key\",\"vk\":%u,\"scan\":%u,\"flags\":%u,\"message\":%u",
                     ri->data.keyboard.VKey, ri->data.keyboard.MakeCode, ri->data.keyboard.Flags,
                     ri->data.keyboard.Message);
            probe_emit(fields);
        } else if (ri->header.dwType == RIM_TYPEHID) {
            char hex[200];
            DWORD bytes = ri->data.hid.dwSizeHid * ri->data.hid.dwCount;
            DWORD shown = bytes < 64 ? bytes : 64;
            for (DWORD b = 0; b < shown; b++)
                snprintf(hex + b * 2, 3, "%02x", ri->data.hid.bRawData[b]);
            hex[shown * 2] = '\0';
            snprintf(fields, sizeof fields, "\"event\":\"raw_hid\",\"bytes\":\"%s\"", hex);
            probe_emit(fields);
        }
    }
    free(ri);
    return DefWindowProcW(hwnd, msg, wp, lp); /* lets the system clean up the WM_INPUT */
}

int probe_keylog(int argc, char **argv)
{
    long seconds;
    if (argc < 1 || probe_parse_count(argv[0], PROBE_KEYLOG_MAX_S, &seconds) < 0) {
        fputs("usage: probe_spawn keylog <seconds>\n", stderr);
        return 2;
    }
    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = keylog_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"cptk_probe_keylog";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
    RAWINPUTDEVICE rid[2];
    rid[0].usUsagePage = 0x01; /* generic desktop */
    rid[0].usUsage = 0x06;     /* keyboard */
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hwnd;
    rid[1].usUsagePage = 0x0C; /* consumer */
    rid[1].usUsage = 0x01;     /* consumer control */
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = hwnd;
    char fields[128];
    if (!RegisterRawInputDevices(rid, 2, sizeof rid[0])) {
        snprintf(fields, sizeof fields, "\"event\":\"keylog_failed\",\"step\":\"RegisterRawInputDevices\",\"error\":%lu",
                 GetLastError());
        probe_emit(fields);
        return 1;
    }
    if (!SetTimer(hwnd, 1, (UINT)seconds * 1000u, NULL)) {
        snprintf(fields, sizeof fields, "\"event\":\"keylog_failed\",\"step\":\"SetTimer\",\"error\":%lu",
                 GetLastError());
        probe_emit(fields);
        return 1;
    }
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0)
        DispatchMessageW(&m);
    probe_emit("\"event\":\"keylog_end\"");
    return 0;
}
