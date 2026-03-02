/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * TUI Spawn - Cross-platform terminal window spawning
 *
 * Automatically opens a new terminal window for ncurses rendering,
 * similar to how SDL creates an OS window. The parent process stays
 * alive for debug output (visible in IDEs like CLion).
 *
 * Console logging: ncurses_console_log() sends messages from the child
 * (ncurses process inside kitty) back to the parent (CLion console)
 * via a named FIFO. The parent reads the FIFO with poll() while
 * waiting for the child to exit.
 *
 * Supported platforms:
 *   Linux  - kitty, alacritty, xterm, gnome-terminal, konsole
 *   macOS  - kitty, Terminal.app via open(1)
 *
 * Controlled via CELS_NCURSES_TERMINAL env var:
 *   (unset)  = auto-detect from available terminals
 *   "kitty"  = force kitty
 *   "none"   = skip spawning, use current terminal
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

/* ============================================================================
 * Console log -- child writes to FIFO, parent relays to stderr
 * ============================================================================ */

/* Path to shared console FIFO (set via env var after spawn) */
static const char* g_console_path = NULL;
static int         g_console_fd   = -1;

void ncurses_console_log(const char* fmt, ...) {
    if (!g_console_path)
        return;

    /* Open the FIFO once on first call */
    if (g_console_fd < 0) {
        g_console_fd = open(g_console_path, O_WRONLY | O_NONBLOCK);
        if (g_console_fd < 0)
            return;
    }

    char buf[4096]; /* fits within PIPE_BUF — atomic write */
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        if ((size_t)len >= sizeof(buf))
            len = (int)(sizeof(buf) - 1);
        write(g_console_fd, buf, (size_t)len);
    }
}

/* ============================================================================
 * Platform: get path to current executable
 * ============================================================================ */

static bool get_self_exe(char* buf, size_t bufsz) {
#if defined(__linux__)
    ssize_t len = readlink("/proc/self/exe", buf, bufsz - 1);
    if (len > 0) { buf[len] = '\0'; return true; }
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    uint32_t size = (uint32_t)bufsz;
    if (_NSGetExecutablePath(buf, &size) == 0) return true;
#endif
    return false;
}

/* ============================================================================
 * Parent: relay console FIFO to stderr while waiting for child to exit
 * ============================================================================ */

#if defined(__linux__) || defined(__APPLE__)

static void parent_wait_and_relay(pid_t child_pid, const char* log_path) {
    int fd = open(log_path, O_RDONLY | O_NONBLOCK);
    char buf[4096];

    struct pollfd pfd = { .fd = fd, .events = POLLIN };

    for (;;) {
        /* Wait up to 50ms for data on the FIFO */
        if (fd >= 0 && poll(&pfd, 1, 50) > 0 && (pfd.revents & POLLIN)) {
            ssize_t n;
            while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                fputs(buf, stderr);
                fflush(stderr);
            }
        }

        /* Check if child exited */
        int status;
        pid_t w = waitpid(child_pid, &status, WNOHANG);
        if (w > 0) {
            /* Final drain */
            if (fd >= 0) {
                ssize_t n;
                while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
                    buf[n] = '\0';
                    fputs(buf, stderr);
                }
                close(fd);
            }
            fflush(stderr);
            fprintf(stderr, "[NCurses] Window closed\n");
            fflush(stderr);
            unlink(log_path);
            exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
        }

        /* If poll wasn't called (fd < 0), sleep to avoid busy-spin */
        if (fd < 0)
            usleep(50000);
    }
}

/* ============================================================================
 * Platform: try launching a terminal emulator
 * ============================================================================ */

/* Try a single terminal emulator. Returns child pid on success, -1 on failure. */
static pid_t try_terminal(const char* term_path, const char* exe,
                          const char* title, const char* flag_title,
                          const char* flag_exec) {
    /* Check if the terminal binary exists */
    if (access(term_path, X_OK) != 0) return -1;

    pid_t pid = fork();
    if (pid == 0) {
        /* Child: exec into terminal running our executable */
        setenv("CELS_NCURSES_SPAWNED", "1", 1);
        if (flag_title && title) {
            execl(term_path, term_path, flag_title, title,
                  flag_exec, exe, (char*)NULL);
        } else {
            execl(term_path, term_path, flag_exec, exe, (char*)NULL);
        }
        /* exec failed */
        _exit(127);
    } else if (pid > 0) {
        /* Parent: briefly check if child survived exec */
        usleep(100000); /* 100ms */
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == 0) {
            /* Child still running = exec succeeded */
            return pid;
        }
        /* Child exited immediately = exec failed or terminal not found */
    }
    return -1;
}

#endif /* __linux__ || __APPLE__ */

/* ============================================================================
 * Terminal candidate list per platform
 * ============================================================================ */

typedef struct {
    const char* name;
    const char* path;
    const char* title_flag;  /* NULL if unsupported */
    const char* exec_flag;
} TermCandidate;

#if defined(__linux__)
static const TermCandidate g_terminals[] = {
    { "kitty",          "/usr/bin/kitty",            "--title", "-e" },
    { "alacritty",      "/usr/bin/alacritty",        "--title", "-e" },
    { "xterm",          "/usr/bin/xterm",            "-title",  "-e" },
    { "gnome-terminal", "/usr/bin/gnome-terminal",   "--title", "--" },
    { "konsole",        "/usr/bin/konsole",          NULL,      "-e" },
    { NULL, NULL, NULL, NULL }
};
#elif defined(__APPLE__)
static const TermCandidate g_terminals[] = {
    { "kitty",     "/usr/local/bin/kitty",              "--title", "-e" },
    { "kitty",     "/opt/homebrew/bin/kitty",           "--title", "-e" },
    { "alacritty", "/usr/local/bin/alacritty",          "--title", "-e" },
    { "alacritty", "/opt/homebrew/bin/alacritty",       "--title", "-e" },
    { NULL, NULL, NULL, NULL }
};
#else
static const TermCandidate g_terminals[] = {
    { NULL, NULL, NULL, NULL }
};
#endif

/* ============================================================================
 * Public API: ncurses_try_spawn_terminal
 * ============================================================================ */

bool ncurses_try_spawn_terminal(const char* window_title) {
    /* Already inside a spawned terminal -- init console log path */
    if (getenv("CELS_NCURSES_SPAWNED")) {
        g_console_path = getenv("CELS_NCURSES_CONSOLE_PATH");
        ncurses_console_log("[NCurses] Console relay active\n");
        return false;
    }

    /* Check if user disabled spawning */
    const char* term_pref = getenv("CELS_NCURSES_TERMINAL");
    if (term_pref && strcmp(term_pref, "none") == 0)
        return false;

    char exe[4096];
    if (!get_self_exe(exe, sizeof(exe)))
        return false;

    const char* title = (window_title && window_title[0])
                      ? window_title : "cels-ncurses";

    /* Create console FIFO and set path for child to inherit */
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "/tmp/cels-ncurses-console-%d.fifo", getpid());
    unlink(log_path); /* remove stale FIFO from a previous run */
    mkfifo(log_path, 0600);
    setenv("CELS_NCURSES_CONSOLE_PATH", log_path, 1);

#if defined(__linux__) || defined(__APPLE__)
    /* If user specified a terminal, try only that one */
    if (term_pref && term_pref[0]) {
        for (int i = 0; g_terminals[i].name; i++) {
            if (strcmp(g_terminals[i].name, term_pref) == 0) {
                pid_t pid = try_terminal(g_terminals[i].path, exe, title,
                                         g_terminals[i].title_flag,
                                         g_terminals[i].exec_flag);
                if (pid > 0) {
                    fprintf(stderr, "[NCurses] Spawned %s window (pid %d): %s\n",
                            g_terminals[i].name, pid, title);
                    fflush(stderr);
                    parent_wait_and_relay(pid, log_path);
                    /* never returns */
                }
                fprintf(stderr, "[NCurses] Error: could not launch %s at %s\n",
                        term_pref, g_terminals[i].path);
                return false;
            }
        }
        fprintf(stderr, "[NCurses] Error: unknown terminal '%s'\n", term_pref);
        return false;
    }

    /* Auto-detect: try each terminal in order */
    for (int i = 0; g_terminals[i].name; i++) {
        pid_t pid = try_terminal(g_terminals[i].path, exe, title,
                                 g_terminals[i].title_flag,
                                 g_terminals[i].exec_flag);
        if (pid > 0) {
            fprintf(stderr, "[NCurses] Spawned %s window (pid %d): %s\n",
                    g_terminals[i].name, pid, title);
            fflush(stderr);
            parent_wait_and_relay(pid, log_path);
            /* never returns */
        }
    }
#endif

    unlink(log_path);
    fprintf(stderr, "[NCurses] No terminal emulator found, using current terminal\n");
    return false;
}
