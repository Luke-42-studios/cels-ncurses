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
 * The NCurses_WindowUpdateSystem mutates it via cel_mutate, and the
 * ncurses_window() accessor exposes it to consumer TUs.
 */

#define _POSIX_C_SOURCE 199309L
#include <cels-ncurses/tui_window.h>
#include <cels-ncurses/tui_ncurses.h>
#include <cels-ncurses/tui_internal.h>
#include <cels-ncurses/tui_color.h>
#include <cels/cels.h>
#include <ncurses.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <time.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* The single window entity (0 = no window) */
static cels_entity_t g_window_entity = 0;

static volatile int g_running = 1;
static int g_ncurses_active = 0;

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
 * Signal Handler
 * ============================================================================ */

static void tui_sigint_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

static void cleanup_endwin(void) {
    if (g_ncurses_active && !isendwin()) {
        endwin();
        g_ncurses_active = 0;
    }
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
 * State Singleton Accessor
 * ============================================================================
 *
 * Returns this TU's canonical NCurses_WindowState (the CEL_State static).
 * Consumer TUs call this instead of reading their own local static.
 */

const NCurses_WindowState_t* ncurses_window(void) {
    return &NCurses_WindowState;
}

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

    /* Auto-spawn a new terminal window if not already inside one.
     * Similar to how SDL creates an OS window -- the user just runs the
     * binary and a window appears. If spawning succeeds, the parent waits
     * and exits; the child re-runs in the new terminal with ncurses. */
    extern bool ncurses_try_spawn_terminal(const char* window_title);
    ncurses_try_spawn_terminal(config->title);

    signal(SIGINT, tui_sigint_handler);
    atexit(cleanup_endwin);

    setlocale(LC_ALL, "");

    initscr();
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

        /* Detect resize */
        int new_w = COLS;
        int new_h = LINES;
        bool resized = (new_w != g_last_cols || new_h != g_last_lines);
        if (resized) {
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

        /* Resize detected -- update state to trigger reactivity */
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

