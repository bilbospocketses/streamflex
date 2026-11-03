/* probe_spawn run, Linux: a process group under a child subreaper; done once every descendant is reaped. On the way
   it reports leader_exited (the leader reaped) and group_empty (no process left in its group), both before exited. */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "spawn.h"

struct run_state {
    pid_t pid;       /* the leader, whose pid is also the group id */
    int closed;      /* a close came */
    int64_t t_close; /* probe_mono_ms of the last close */
    int killing;     /* a kill came: SIGKILL every direct child on each pass */
};

/* Milliseconds since the last close, or -1 when none came. */
static long long since_close(const struct run_state *s)
{
    return s->closed ? (long long)(probe_mono_ms() - s->t_close) : -1;
}

/* The leader's exit code as run reports it: 128 + the signal when a signal ended it, -1 when it was never reaped. */
static int exit_code(int reaped, int status)
{
    return !reaped                ? -1
           : WIFEXITED(status)   ? WEXITSTATUS(status)
           : WIFSIGNALED(status) ? 128 + WTERMSIG(status)
                                 : -1;
}

static int spawn_failed(const char *step, int error)
{
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"spawn_failed\",\"step\":\"%s\",\"error\":%d", step, error);
    probe_emit(fields);
    return 1;
}

static int g_supervising; /* probe_supervise has started a child */

/* The PPid in /proc/<pid>/status, or -1. */
static long parent_of(long pid)
{
    char path[64], line[128];
    snprintf(path, sizeof path, "/proc/%ld/status", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    long ppid = -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "PPid: %ld", &ppid) == 1)
            break;
    fclose(f);
    return ppid;
}

int probe_child_has_pid(long pid)
{
    long self = (long)getpid();
    if (!g_supervising || pid == self)
        return 0;
    for (int depth = 0; pid > 1 && depth < 64; depth++) {
        pid = parent_of(pid);
        if (pid == self)
            return 1;
    }
    return 0;
}

/* SIGKILLs every direct child: the leader, and the descendants this subreaper inherited, which may have left the
   group. Run on every pass after a kill, so a descendant orphaned by the last pass is caught by the next. */
static void kill_children(void)
{
    DIR *d = opendir("/proc");
    if (!d)
        return;
    long self = (long)getpid();
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        char *end;
        long p = strtol(e->d_name, &end, 10);
        if (*end || p <= 0)
            continue;
        if (parent_of(p) == self)
            kill((pid_t)p, SIGKILL);
    }
    closedir(d);
}

static void on_command(int cmd, void *ctx)
{
    struct run_state *s = ctx;
    if (cmd == PROBE_CMD_CLOSE) {
        s->closed = 1;
        s->t_close = probe_mono_ms();
        kill(-s->pid, SIGTERM);
        probe_emit("\"event\":\"close_sent\""); /* no windows field: a Linux close is a signal, not WM_CLOSE */
    } else {
        s->killing = 1;
        kill(-s->pid, SIGKILL);
        kill_children();
        probe_emit("\"event\":\"kill_sent\"");
    }
}

/* The forked child: its own group, stdin from /dev/null (never run's command pipe), stdout onto stderr (never among
   the JSON lines), then exec. A failure sends errno back over err_fd, which closes on a successful exec. */
static void exec_child(char **argv, int err_fd)
{
    setpgid(0, 0);
    int null_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (null_fd >= 0 && dup2(null_fd, STDIN_FILENO) >= 0 && dup2(STDERR_FILENO, STDOUT_FILENO) >= 0)
        execv(argv[0], argv);
    int e = errno;
    ssize_t w = write(err_fd, &e, sizeof e);
    (void)w;
    _exit(127);
}

int probe_run(int argc, char **argv)
{
    return probe_supervise(argc, argv, "usage: probe_spawn run <program> [args...]\n", NULL);
}

int probe_supervise(int argc, char **argv, const char *usage, const probe_watch *watch)
{
    if (argc < 1) {
        fputs(usage, stderr);
        return 2;
    }
    /* Orphaned descendants reparent to us, so exited can wait for every one of them. */
    if (prctl(PR_SET_CHILD_SUBREAPER, 1L, 0L, 0L, 0L) == -1)
        return spawn_failed("subreaper", errno);
    int exec_pipe[2];
    if (pipe(exec_pipe) == -1 || fcntl(exec_pipe[0], F_SETFD, FD_CLOEXEC) == -1
        || fcntl(exec_pipe[1], F_SETFD, FD_CLOEXEC) == -1)
        return spawn_failed("pipe", errno);
    pid_t pid = fork();
    if (pid < 0)
        return spawn_failed("fork", errno);
    if (pid == 0)
        exec_child(argv, exec_pipe[1]);
    setpgid(pid, pid);
    close(exec_pipe[1]);
    int exec_errno = 0;
    ssize_t got;
    do
        got = read(exec_pipe[0], &exec_errno, sizeof exec_errno);
    while (got < 0 && errno == EINTR);
    close(exec_pipe[0]);
    if (got == (ssize_t)sizeof exec_errno) {
        waitpid(pid, NULL, 0);
        return spawn_failed("exec", exec_errno);
    }
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%d", (int)pid);
    probe_emit(fields);
    g_supervising = 1;

    struct run_state s = { pid, 0, 0, 0 };
    probe_lines lines;
    memset(&lines, 0, sizeof lines);
    int status = 0;
    int leader_reaped = 0;
    int group_empty = 0;
    int stdin_open = 1;
    for (;;) {
        int st;
        pid_t r;
        while ((r = waitpid(-1, &st, WNOHANG)) > 0) {
            if (r == pid) {
                status = st;
                leader_reaped = 1;
                snprintf(fields, sizeof fields, "\"event\":\"leader_exited\",\"code\":%d,\"ms_since_close\":%lld",
                         exit_code(1, status), since_close(&s));
                probe_emit(fields);
            }
        }
        int no_child = r == -1 && errno == ECHILD; /* the leader and every descendant are gone */
        /* The spec's launcher signal: the leader reaped and no process left in its group. A descendant that left the
           group (setsid, setpgid) can outlive it, and exited still waits for that one. */
        if (leader_reaped && !group_empty && kill(-pid, 0) == -1 && errno == ESRCH) {
            group_empty = 1;
            snprintf(fields, sizeof fields, "\"event\":\"group_empty\",\"ms_since_close\":%lld", since_close(&s));
            probe_emit(fields);
        }
        if (no_child)
            break;
        if (watch && watch->tick)
            watch->tick(watch->ctx);
        if (s.killing)
            kill_children();
        if (!stdin_open) {
            poll(NULL, 0, 50);
            continue;
        }
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (poll(&pfd, 1, 50) <= 0)
            continue;
        char buf[256];
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            stdin_open = 0;
            continue;
        }
        probe_lines_feed(&lines, buf, (size_t)n, on_command, &s);
    }
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d,\"ms_since_close\":%lld",
             exit_code(leader_reaped, status), since_close(&s));
    probe_emit(fields);
    if (watch && watch->ended)
        watch->ended(watch->ctx);
    return 0;
}
