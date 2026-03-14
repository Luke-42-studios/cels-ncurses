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
 * TUI Spawn - PTY-based terminal window spawning
 *
 * Opens a new terminal emulator window and connects it to this process
 * via a PTY pair. The original process stays alive (debugger attached,
 * stderr/stdout go to IDE console). ncurses renders to the PTY via
 * newterm() instead of initscr().
 *
 * How it works:
 *   1. openpty() creates a master/slave PTY pair
 *   2. fork() a child that execs a terminal emulator running our own
 *      binary as a PTY relay (detected via CELS_NCURSES_PTY_RELAY env var)
 *   3. The relay process bridges the terminal's stdin/stdout to our slave PTY
 *      using select() and handles SIGWINCH for resize propagation
 *   4. Parent uses the master fd with newterm() for ncurses rendering
 *   5. ncurses_console_log() writes to stderr (goes to IDE console)
 *
 * Controlled via CELS_NCURSES_TERMINAL env var:
 *   (unset)  = auto-detect from available terminals
 *   "kitty"  = force kitty
 *   "none"   = skip spawning, use current terminal (initscr path)
 *
 * Supported terminals:
 *   Linux  - kitty, alacritty, xterm, gnome-terminal, konsole
 *   macOS  - kitty, alacritty
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
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__)
#include <util.h>
#endif
#endif

/* ============================================================================
 * Console log -- always writes to stderr (IDE console)
 * ============================================================================ */

void ncurses_console_log(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len <= 0)
        return;
    if ((size_t)len >= sizeof(buf))
        len = (int)(sizeof(buf) - 1);

    fwrite(buf, 1, (size_t)len, stderr);
    fflush(stderr);
}

/* ============================================================================
 * Platform: get path to current executable
 * ============================================================================ */

#if defined(__linux__) || defined(__APPLE__)

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
 * PTY Relay -- runs inside the terminal emulator
 * ============================================================================
 *
 * When our binary is re-exec'd with CELS_NCURSES_PTY_RELAY set, this
 * function runs instead of the normal application. It bridges the
 * terminal's stdin/stdout to our slave PTY using select().
 *
 * SIGWINCH: when the terminal emulator resizes, this relay reads the
 * new size from its own terminal (stdin) and propagates it to the
 * slave PTY via ioctl(TIOCSWINSZ). This causes a SIGWINCH on the
 * master side, which ncurses uses to update COLS/LINES.
 */

static volatile sig_atomic_t g_relay_winch = 0;
static int g_relay_slave_fd = -1;

static void relay_sigwinch_handler(int sig) {
    (void)sig;
    g_relay_winch = 1;
}

static void relay_propagate_winch(int slave_fd) {
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
        ioctl(slave_fd, TIOCSWINSZ, &ws);
    }
}

static int run_pty_relay(const char* slave_path) {
    /* Open our slave PTY */
    int slave_fd = open(slave_path, O_RDWR | O_NOCTTY);
    if (slave_fd < 0) {
        fprintf(stderr, "[relay] Cannot open %s\n", slave_path);
        return 1;
    }
    g_relay_slave_fd = slave_fd;

    /* Set stdin (terminal's PTY) to raw mode */
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    /* Propagate initial window size */
    relay_propagate_winch(slave_fd);

    /* Install SIGWINCH handler for resize propagation */
    struct sigaction sa = { .sa_handler = relay_sigwinch_handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, NULL);

    /* select() loop: bridge stdin ↔ slave_fd */
    char buf[4096];
    int maxfd = (STDIN_FILENO > slave_fd ? STDIN_FILENO : slave_fd) + 1;

    for (;;) {
        /* Handle pending SIGWINCH */
        if (g_relay_winch) {
            g_relay_winch = 0;
            relay_propagate_winch(slave_fd);
        }

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(slave_fd, &rfds);

        /* Use a timeout so we can check SIGWINCH flag periodically */
        struct timeval tv = { .tv_sec = 0, .tv_usec = 50000 }; /* 50ms */

        int ret = select(maxfd, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        /* Terminal input → slave PTY (user keystrokes → ncurses) */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break; /* terminal closed */
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(slave_fd, buf + written, (size_t)(n - written));
                if (w <= 0) goto done;
                written += w;
            }
        }

        /* Slave PTY → terminal output (ncurses rendering → display) */
        if (FD_ISSET(slave_fd, &rfds)) {
            ssize_t n = read(slave_fd, buf, sizeof(buf));
            if (n <= 0) break; /* master closed */
            ssize_t written = 0;
            while (written < n) {
                ssize_t w = write(STDOUT_FILENO, buf + written, (size_t)(n - written));
                if (w <= 0) goto done;
                written += w;
            }
        }
    }
done:
    close(slave_fd);
    return 0;
}

/*
 * Constructor: check if we're a relay process.
 *
 * When a terminal emulator re-execs our binary with CELS_NCURSES_PTY_RELAY
 * set, this constructor intercepts before main() and runs the relay loop.
 * The normal application never starts.
 */
__attribute__((constructor))
static void ncurses_check_pty_relay(void) {
    const char* slave_path = getenv("CELS_NCURSES_PTY_RELAY");
    if (slave_path && slave_path[0]) {
        int rc = run_pty_relay(slave_path);
        _exit(rc);
    }
}

/* ============================================================================
 * Spawned terminal child PID (for cleanup on shutdown)
 * ============================================================================ */

static pid_t g_terminal_pid = -1;

void ncurses_kill_terminal(void) {
    if (g_terminal_pid > 0) {
        kill(g_terminal_pid, SIGTERM);
        waitpid(g_terminal_pid, NULL, 0);
        g_terminal_pid = -1;
    }
}

/* ============================================================================
 * Terminal candidate list per platform
 * ============================================================================ */

typedef struct {
    const char* name;
    const char* path;
    const char* title_flag;  /* NULL if unsupported */
} TermCandidate;

#if defined(__linux__)
static const TermCandidate g_terminals[] = {
    { "kitty",          "/usr/bin/kitty",            "--title" },
    { "alacritty",      "/usr/bin/alacritty",        "--title" },
    { "xterm",          "/usr/bin/xterm",            "-title"  },
    { "gnome-terminal", "/usr/bin/gnome-terminal",   "--title" },
    { "konsole",        "/usr/bin/konsole",          NULL      },
    { NULL, NULL, NULL }
};
#elif defined(__APPLE__)
static const TermCandidate g_terminals[] = {
    { "kitty",     "/usr/local/bin/kitty",              "--title" },
    { "kitty",     "/opt/homebrew/bin/kitty",           "--title" },
    { "alacritty", "/usr/local/bin/alacritty",          "--title" },
    { "alacritty", "/opt/homebrew/bin/alacritty",       "--title" },
    { NULL, NULL, NULL }
};
#else
static const TermCandidate g_terminals[] = {
    { NULL, NULL, NULL }
};
#endif

/* ============================================================================
 * PTY-based terminal spawning
 * ============================================================================
 *
 * Forks a child that execs a terminal emulator running our own binary
 * as a PTY relay. The terminal runs: our_binary (with env var set).
 * The constructor detects the env var and runs the relay loop.
 *
 * Returns child pid on success, -1 on failure.
 */

static pid_t try_spawn_pty_terminal(const TermCandidate* term, const char* exe,
                                    const char* title, const char* slave_path) {
    if (access(term->path, X_OK) != 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        /* Child: set relay env var and exec terminal running our binary */
        setenv("CELS_NCURSES_PTY_RELAY", slave_path, 1);

        if (term->title_flag && title) {
            execl(term->path, term->path,
                  term->title_flag, title,
                  "-e", exe,
                  (char*)NULL);
        } else {
            execl(term->path, term->path,
                  "-e", exe,
                  (char*)NULL);
        }
        _exit(127);
    }

    /* Parent: briefly check if child survived exec */
    usleep(200000); /* 200ms -- give terminal time to start */
    int status;
    pid_t w = waitpid(pid, &status, WNOHANG);
    if (w == 0) {
        /* Child still running = success */
        g_terminal_pid = pid;
        return pid;
    }

    return -1;
}

#endif /* __linux__ || __APPLE__ */

/* ============================================================================
 * Public API: ncurses_spawn_terminal_pty
 * ============================================================================
 *
 * Creates a PTY pair and spawns a terminal emulator connected to it.
 * Returns the master fd for use with newterm(). Returns -1 if spawning
 * is disabled or fails (caller should fall back to initscr).
 */

int ncurses_spawn_terminal_pty(const char* window_title) {
#if defined(__linux__) || defined(__APPLE__)
    const char* term_pref = getenv("CELS_NCURSES_TERMINAL");
    if (term_pref && strcmp(term_pref, "none") == 0)
        return -1;

    char exe[4096];
    if (!get_self_exe(exe, sizeof(exe))) {
        fprintf(stderr, "[NCurses] Cannot determine executable path\n");
        return -1;
    }

    const char* title = (window_title && window_title[0])
                      ? window_title : "cels-ncurses";

    /* Create PTY pair */
    int master_fd, slave_fd;
    if (openpty(&master_fd, &slave_fd, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "[NCurses] openpty() failed\n");
        return -1;
    }

    /* Get slave path for the relay to open */
    char* slave_path = ttyname(slave_fd);
    if (!slave_path) {
        fprintf(stderr, "[NCurses] ttyname() failed\n");
        close(master_fd);
        close(slave_fd);
        return -1;
    }
    char slave_path_buf[256];
    strncpy(slave_path_buf, slave_path, sizeof(slave_path_buf) - 1);
    slave_path_buf[sizeof(slave_path_buf) - 1] = '\0';

    /* Close slave in parent -- the relay child opens it via path */
    close(slave_fd);

    /* Try to spawn a terminal emulator */
    if (term_pref && term_pref[0]) {
        /* User specified a terminal */
        for (int i = 0; g_terminals[i].name; i++) {
            if (strcmp(g_terminals[i].name, term_pref) == 0) {
                pid_t pid = try_spawn_pty_terminal(&g_terminals[i], exe,
                                                   title, slave_path_buf);
                if (pid > 0) {
                    fprintf(stderr, "[NCurses] Spawned %s window (pid %d): %s\n",
                            g_terminals[i].name, pid, title);
                    return master_fd;
                }
                fprintf(stderr, "[NCurses] Error: could not launch %s at %s\n",
                        term_pref, g_terminals[i].path);
                break;
            }
        }
    } else {
        /* Auto-detect: try each terminal in order */
        for (int i = 0; g_terminals[i].name; i++) {
            pid_t pid = try_spawn_pty_terminal(&g_terminals[i], exe,
                                               title, slave_path_buf);
            if (pid > 0) {
                fprintf(stderr, "[NCurses] Spawned %s window (pid %d): %s\n",
                        g_terminals[i].name, pid, title);
                return master_fd;
            }
        }
    }

    /* No terminal found -- clean up master fd */
    close(master_fd);
    fprintf(stderr, "[NCurses] No terminal emulator found, using current terminal\n");
#else
    (void)window_title;
#endif
    return -1;
}
