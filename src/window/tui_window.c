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
 * TUI Window - Terminal Lifecycle
 *
 * Provides terminal initialization, shutdown, and per-frame update.
 * Lifecycle observers in ncurses_module.c call ncurses_terminal_init()
 * and ncurses_terminal_shutdown() via extern declarations in tui_internal.h.
 *
 * Owns the canonical NCurses_WindowState (this TU's CEL_State static).
 * The NCurses_WindowUpdateSystem mutates it via cel_mutate.
 * Consumer TUs read via cel_read(NCurses_WindowState) (cross-TU pointer registry).
 */

#define _POSIX_C_SOURCE 199309L
#include "../tui_window.h"
#include <cels_ncurses.h>
#include "../tui_internal.h"
#include <cels_ncurses_draw.h>
#include <cels/cels.h>
#include <ncurses.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <time.h>
#include <stdio.h>
#include <sys/ioctl.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* CEL_State(NCurses_WindowState) — owned by this TU */
static struct NCurses_WindowState NCurses_WindowState = { .running = true };

/* The single window entity (0 = no window) */
static cels_entity_t g_window_entity = 0;

static volatile int g_running = 1;
static int g_ncurses_active = 0;

/* newterm() screen (NULL when using initscr fallback) */
static SCREEN* g_screen = NULL;

/* PTY master fd for polling resize (-1 when using initscr) */
static int g_pty_master_fd = -1;

/* Stored FPS from config for frame_end throttle */
static int g_target_fps = 60;

/* Timing state */
static struct timespec g_frame_start = {0};
static struct timespec g_prev_frame_start = {0};
static float g_delta_time = 1.0f / 60.0f;

/* Cached dimensions for resize detection */
static int g_last_cols = 0;
static int g_last_lines = 0;

/* ============================================================================
 * Signal Handlers
 * ============================================================================ */

static void tui_sigint_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* SIGWINCH: terminal was resized. Flag it for the update system to handle.
 * When using PTY spawn, the relay propagates resize via ioctl(TIOCSWINSZ)
 * which triggers SIGWINCH on the master side. */
static volatile sig_atomic_t g_sigwinch_pending = 0;

static void tui_sigwinch_handler(int sig) {
    (void)sig;
    g_sigwinch_pending = 1;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

static void cleanup_endwin(void) {
    if (g_ncurses_active && !isendwin()) {
        endwin();
        g_ncurses_active = 0;
    }
    if (g_screen) {
        delscreen(g_screen);
        g_screen = NULL;
    }
    /* Kill the terminal emulator child process */
    extern void ncurses_kill_terminal(void);
    ncurses_kill_terminal();
}

/* ============================================================================
 * Window Lifecycle Accessors
 * ============================================================================
 *
 * Called by CEL_Observe(NCursesWindowLC, on_create/on_destroy) in
 * ncurses_module.c. These accessors keep g_window_entity static to this
 * file while allowing the lifecycle observers to read and write it.
 */

void ncurses_window_set_entity(cels_entity_t entity) { g_window_entity = entity; }
cels_entity_t ncurses_window_get_entity(void) { return g_window_entity; }
bool ncurses_window_is_active(void) { return g_ncurses_active != 0; }

/* ============================================================================
 * Terminal Init (extracted from old tui_hook_startup)
 * ============================================================================
 *
 * Initializes the ncurses terminal from component config data.
 * Sets signal handler, atexit, locale, initscr, input mode, color system,
 * and stores FPS for timing calculations.
 */

void ncurses_terminal_init(NCurses_WindowConfig* config) {
    /* Ensure this TU's state ID is set (idempotent) */
    NCurses_WindowState_register();
    NCurses_WindowState.running = true;
    cels_state_bind(NCurses_WindowState);

    signal(SIGINT, tui_sigint_handler);
    signal(SIGWINCH, tui_sigwinch_handler);
    atexit(cleanup_endwin);

    setlocale(LC_ALL, "");

    /* Try to spawn a terminal window connected via PTY.
     * On success we get a master fd and use newterm() so the original
     * process stays alive (debugger attached, stderr → IDE console).
     * On failure (-1) we fall back to initscr() on the current terminal. */
    extern int ncurses_spawn_terminal_pty(const char* window_title);
    int pty_master = ncurses_spawn_terminal_pty(config->title);

    if (pty_master >= 0) {
        /* PTY path: ncurses renders to the terminal emulator via PTY.
         * newterm() needs SEPARATE FILE* for output and input — using
         * the same FILE* causes stdio buffer corruption between reads
         * and writes. dup() the fd so each FILE* has its own buffer. */
        g_pty_master_fd = pty_master;
        int pty_in_fd = dup(pty_master);
        FILE* pty_out = fdopen(pty_master, "w");
        FILE* pty_in  = fdopen(pty_in_fd, "r");
        if (!pty_out || !pty_in) {
            fprintf(stderr, "[NCurses] fdopen(pty) failed, falling back to initscr\n");
            if (pty_out) fclose(pty_out); else close(pty_master);
            if (pty_in) fclose(pty_in); else close(pty_in_fd);
            initscr();
        } else {
            /* Disable stdio buffering — ncurses escape sequences must
             * reach the terminal immediately */
            setvbuf(pty_out, NULL, _IONBF, 0);
            setvbuf(pty_in, NULL, _IONBF, 0);

            g_screen = newterm("xterm-256color", pty_out, pty_in);
            if (!g_screen) {
                fprintf(stderr, "[NCurses] newterm() failed, falling back to initscr\n");
                fclose(pty_out);
                fclose(pty_in);
                initscr();
            } else {
                set_term(g_screen);
            }
        }
    } else {
        /* No PTY: use current terminal directly */
        initscr();
    }

    g_ncurses_active = 1;
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);
    ESCDELAY = 25;

    if (has_colors()) {
        start_color();
        use_default_colors();
        assume_default_colors(-1, -1);

        /* Initialize color system with mode detection/override.
         * color_mode: 0=auto, 1=256, 2=palette, 3=direct */
        int override = config->color_mode > 0
                     ? config->color_mode - 1   /* Convert 1-3 to TUI_ColorMode 0-2 */
                     : -1;                       /* 0 means auto-detect */
        tui_color_init(override);
    }

    g_target_fps = config->fps > 0 ? config->fps : 60;
    g_delta_time = 1.0f / (float)g_target_fps;

    /* Cache initial dimensions for resize detection */
    g_last_cols = COLS;
    g_last_lines = LINES;

    /* Input: register key sequences and enable mouse (requires active ncurses) */
    ncurses_input_configure_terminal();

    g_running = 1;
}

/* ============================================================================
 * Terminal Shutdown
 * ============================================================================ */

void ncurses_terminal_shutdown(void) {
    if (g_ncurses_active && !isendwin()) {
        endwin();
        g_ncurses_active = 0;
    }
}

/* ============================================================================
 * ECS System -- per-frame window state update via cel_mutate
 * ============================================================================
 *
 * Each frame: updates timing, detects resize, checks quit signal.
 * cel_mutate auto-notifies the reactivity system after the block.
 *
 * Runs at PreRender so WindowState is current before rendering systems.
 */

CEL_System(NCurses_WindowUpdateSystem, .phase = PreRender) {
    cel_run {
        /* Timing */
        g_prev_frame_start = g_frame_start;
        clock_gettime(CLOCK_MONOTONIC, &g_frame_start);
        if (g_prev_frame_start.tv_sec != 0) {
            g_delta_time = (float)(g_frame_start.tv_sec - g_prev_frame_start.tv_sec) +
                           (float)(g_frame_start.tv_nsec - g_prev_frame_start.tv_nsec) / 1e9f;
        }

        /* Handle resize: two paths depending on terminal mode.
         *
         * PTY mode: the relay sets TIOCSWINSZ on the slave, but no SIGWINCH
         * reaches us (no controlling terminal). Poll the PTY size directly
         * and call resizeterm() to update ncurses COLS/LINES.
         *
         * Direct mode (initscr): SIGWINCH + KEY_RESIZE handled by getch().
         */
        if (g_pty_master_fd >= 0) {
            struct winsize ws;
            if (ioctl(g_pty_master_fd, TIOCGWINSZ, &ws) == 0
                && ws.ws_row > 0 && ws.ws_col > 0) {
                if (ws.ws_col != COLS || ws.ws_row != LINES) {
                    resizeterm(ws.ws_row, ws.ws_col);
                    clearok(curscr, TRUE);
                }
            }
        } else if (g_sigwinch_pending) {
            g_sigwinch_pending = 0;
            endwin();
            refresh();
        }

        /* Detect resize */
        int new_w = COLS;
        int new_h = LINES;
        static bool first_frame = true;
        bool resized = first_frame || (new_w != g_last_cols || new_h != g_last_lines);
        if (resized) {
            first_frame = false;
            g_last_cols = new_w;
            g_last_lines = new_h;
        }

        /* Quit requested (SIGINT) */
        if (!g_running) {
            cel_mutate(NCurses_WindowState) {
                NCurses_WindowState.width = g_last_cols;
                NCurses_WindowState.height = g_last_lines;
                NCurses_WindowState.running = false;
                NCurses_WindowState.actual_fps = (g_delta_time > 0.0f) ? 1.0f / g_delta_time : 0.0f;
                NCurses_WindowState.delta_time = g_delta_time;
            }
            cels_request_quit();
            return;
        }

        /* First frame or resize -- update state to trigger reactivity */
        if (resized) {
            cel_mutate(NCurses_WindowState) {
                NCurses_WindowState.width = new_w;
                NCurses_WindowState.height = new_h;
                NCurses_WindowState.running = true;
                NCurses_WindowState.actual_fps = (g_delta_time > 0.0f) ? 1.0f / g_delta_time : 0.0f;
                NCurses_WindowState.delta_time = g_delta_time;
            }
        }
    }
}

/* ============================================================================
 * Frame End: tui_hook_frame_end()
 * ============================================================================
 *
 * FPS throttle -- sleeps to maintain target frame rate.
 * Called by the frame pipeline's frame_end system.
 */

void tui_hook_frame_end(void) {
    float target_delta = 1.0f / (float)g_target_fps;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    float elapsed = (float)(now.tv_sec - g_frame_start.tv_sec) +
                    (float)(now.tv_nsec - g_frame_start.tv_nsec) / 1e9f;

    if (elapsed < target_delta) {
        float remaining = target_delta - elapsed;
        usleep((unsigned int)(remaining * 1000000));
    }
}

