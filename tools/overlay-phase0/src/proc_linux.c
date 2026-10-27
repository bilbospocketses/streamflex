/* probe_spawn run, Linux: a process group, waitpid on the leader, then the group must be empty. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "spawn.h"

int probe_run(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn run <program> [args...]\n", stderr);
        return 2;
    }
    char fields[160];
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(fields, sizeof fields, "\"event\":\"spawn_failed\",\"error\":%d", errno);
        probe_emit(fields);
        return 1;
    }
    if (pid == 0) {
        setpgid(0, 0);
        execv(argv[0], argv);
        _exit(127);
    }
    setpgid(pid, pid);
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%d", (int)pid);
    probe_emit(fields);

    int64_t t_close = 0;
    int status = 0;
    int leader_done = 0;
    int stdin_open = 1;
    for (;;) {
        if (!leader_done && waitpid(pid, &status, WNOHANG) == pid)
            leader_done = 1;
        if (leader_done && kill(-pid, 0) == -1 && errno == ESRCH)
            break;
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (!stdin_open) {
            poll(NULL, 0, 50);
            continue;
        }
        if (poll(&pfd, 1, 50) <= 0)
            continue;
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf - 1);
        if (n <= 0) {
            stdin_open = 0;
            continue;
        }
        buf[n] = '\0';
        if (strstr(buf, "close")) {
            t_close = probe_now_ms();
            kill(-pid, SIGTERM);
            probe_emit("\"event\":\"close_sent\"");
        } else if (strstr(buf, "kill")) {
            kill(-pid, SIGKILL);
            probe_emit("\"event\":\"kill_sent\"");
        }
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
    long long since = t_close ? (long long)(probe_now_ms() - t_close) : -1;
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d,\"ms_since_close\":%lld", code, since);
    probe_emit(fields);
    return 0;
}
