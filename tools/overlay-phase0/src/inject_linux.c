/* probe_spawn keys and keylog, Linux: one uinput keyboard, and every readable evdev device. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

static int g_fd = -1;

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void emit_ev(int fd, int type, int code, int value)
{
    struct input_event ie;
    memset(&ie, 0, sizeof ie);
    ie.type = (unsigned short)type;
    ie.code = (unsigned short)code;
    ie.value = value;
    if (write(fd, &ie, sizeof ie) != (ssize_t)sizeof ie)
        perror("uinput write");
}

static int uinput_open(void)
{
    g_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_fd < 0) {
        char fields[96];
        snprintf(fields, sizeof fields, "\"event\":\"uinput_failed\",\"error\":%d", errno);
        probe_emit(fields);
        return 1;
    }
    ioctl(g_fd, UI_SET_EVBIT, EV_KEY);
    for (unsigned i = 0; i < probe_keys_count; i++)
        ioctl(g_fd, UI_SET_KEYBIT, probe_keys_table[i].evdev);
    struct uinput_setup us;
    memset(&us, 0, sizeof us);
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x1209;
    us.id.product = 0x0001;
    snprintf(us.name, sizeof us.name, "CPTK probe keyboard");
    ioctl(g_fd, UI_DEV_SETUP, &us);
    ioctl(g_fd, UI_DEV_CREATE);
    sleep_ms(1000); /* let the desktop pick up the new device */
    return 0;
}

static void send_key(const probe_key *k, int up)
{
    emit_ev(g_fd, EV_KEY, k->evdev, up ? 0 : 1);
    emit_ev(g_fd, EV_SYN, SYN_REPORT, 0);
}

static void uinput_close(void)
{
    ioctl(g_fd, UI_DEV_DESTROY);
    close(g_fd);
    g_fd = -1;
}

static const probe_key_sender sender = { uinput_open, send_key, sleep_ms, uinput_close };

int probe_keys(int argc, char **argv)
{
    return probe_keys_main(argc, argv, &sender);
}

int probe_keylog(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn keylog <seconds>\n", stderr);
        return 2;
    }
    struct pollfd fds[64];
    char names[64][32];
    nfds_t n = 0;
    for (int d = 0; d < 64; d++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/input/event%d", d);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;
        fds[n].fd = fd;
        fds[n].events = POLLIN;
        snprintf(names[n], sizeof names[n], "%s", path);
        n++;
    }
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"keylog_devices\",\"count\":%u", (unsigned)n);
    probe_emit(fields);
    int64_t end = probe_now_ms() + (int64_t)atoi(argv[0]) * 1000;
    while (probe_now_ms() < end) {
        if (n == 0) {
            sleep_ms(50);
            continue;
        }
        if (poll(fds, n, 50) <= 0)
            continue;
        for (nfds_t f = 0; f < n; f++) {
            if (!(fds[f].revents & POLLIN))
                continue;
            struct input_event ie;
            while (read(fds[f].fd, &ie, sizeof ie) == (ssize_t)sizeof ie) {
                if (ie.type != EV_KEY)
                    continue;
                snprintf(fields, sizeof fields, "\"event\":\"raw_key\",\"device\":\"%s\",\"code\":%u,\"value\":%d",
                         names[f], ie.code, ie.value);
                probe_emit(fields);
            }
        }
    }
    for (nfds_t f = 0; f < n; f++)
        close(fds[f].fd);
    probe_emit("\"event\":\"keylog_end\"");
    return 0;
}
