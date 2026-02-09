/*
 * TUI Window Provider - ncurses Implementation (Backend Descriptor)
 *
 * Implements the CELS_BackendDesc hook interface for ncurses TUI.
 * Core CELS owns the main loop; this module provides 6 hooks:
 *   startup, shutdown, frame_begin, frame_end, should_quit, get_delta_time
 */

#define _POSIX_C_SOURCE 199309L
#include <cels-ncurses/tui_window.h>
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

TUI_WindowState_t TUI_WindowState = {0};
cels_entity_t TUI_WindowStateID = 0;

void TUI_WindowState_ensure(void) {
    if (TUI_WindowStateID == 0) TUI_WindowStateID = cels_state_register("TUI_WindowState");
}

static TUI_Window g_tui_config;
static volatile int g_running = 1;
static int g_ncurses_active = 0;

/* Standard window state for CELS_BackendDesc */
static CELS_WindowState g_window_state = {0};

/* Timing state */
static struct timespec g_frame_start = {0};
static struct timespec g_prev_frame_start = {0};
static float g_delta_time = 1.0f / 60.0f;

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
 * Backend Hook: startup()
 * ============================================================================ */

static void tui_hook_startup(void) {
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
    }

    int fps = g_tui_config.fps > 0 ? g_tui_config.fps : 60;
    g_delta_time = 1.0f / (float)fps;

    /* Populate legacy TUI_WindowState */
    TUI_WindowState.state = WINDOW_STATE_READY;
    TUI_WindowState.width = g_tui_config.width > 0 ? g_tui_config.width : COLS;
    TUI_WindowState.height = g_tui_config.height > 0 ? g_tui_config.height : LINES;
    TUI_WindowState.title = g_tui_config.title;
    TUI_WindowState.version = g_tui_config.version;
    TUI_WindowState.target_fps = (float)fps;
    TUI_WindowState.delta_time = g_delta_time;
    cels_state_notify_change(TUI_WindowStateID);

    /* Populate standard CELS_WindowState */
    g_window_state.lifecycle = CELS_WINDOW_READY;
    g_window_state.width = TUI_WindowState.width;
    g_window_state.height = TUI_WindowState.height;
    g_window_state.title = TUI_WindowState.title;
    g_window_state.target_fps = TUI_WindowState.target_fps;
    g_window_state.delta_time = g_delta_time;
    g_window_state.backend_data = NULL;
}

/* ============================================================================
 * Backend Hook: shutdown()
 * ============================================================================ */

static void tui_hook_shutdown(void) {
    /* Legacy state transition */
    TUI_WindowState.state = WINDOW_STATE_CLOSING;
    cels_state_notify_change(TUI_WindowStateID);
    TUI_WindowState.state = WINDOW_STATE_CLOSED;

    /* Standard state transition */
    g_window_state.lifecycle = CELS_WINDOW_CLOSING;
    if (g_ncurses_active && !isendwin()) {
        endwin();
        g_ncurses_active = 0;
    }
    g_window_state.lifecycle = CELS_WINDOW_CLOSED;
}

/* ============================================================================
 * Backend Hook: frame_begin()
 * ============================================================================ */

static void tui_hook_frame_begin(void) {
    /* Timing */
    g_prev_frame_start = g_frame_start;
    clock_gettime(CLOCK_MONOTONIC, &g_frame_start);
    if (g_prev_frame_start.tv_sec != 0) {
        g_delta_time = (float)(g_frame_start.tv_sec - g_prev_frame_start.tv_sec) +
                       (float)(g_frame_start.tv_nsec - g_prev_frame_start.tv_nsec) / 1e9f;
    }

    /* Update timing in both state structs */
    TUI_WindowState.delta_time = g_delta_time;
    g_window_state.delta_time = g_delta_time;
}

/* ============================================================================
 * Backend Hook: frame_end()
 * ============================================================================ */

static void tui_hook_frame_end(void) {
    /* FPS throttle */
    int fps = g_tui_config.fps > 0 ? g_tui_config.fps : 60;
    float target_delta = 1.0f / (float)fps;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    float elapsed = (float)(now.tv_sec - g_frame_start.tv_sec) +
                    (float)(now.tv_nsec - g_frame_start.tv_nsec) / 1e9f;

    if (elapsed < target_delta) {
        float remaining = target_delta - elapsed;
        usleep((unsigned int)(remaining * 1000000));
    }
}

/* ============================================================================
 * Backend Hook: should_quit()
 * ============================================================================ */

static bool tui_hook_should_quit(void) {
    return !g_running;
}

/* ============================================================================
 * Backend Hook: get_delta_time()
 * ============================================================================ */

static float tui_hook_get_delta_time(void) {
    return g_delta_time;
}

/* ============================================================================
 * Backend Descriptor (static, lives for application lifetime)
 * ============================================================================ */

static CELS_BackendDesc tui_backend_desc = {
    .name = "TUI",
    .hooks = {
        .startup        = tui_hook_startup,
        .shutdown       = tui_hook_shutdown,
        .frame_begin    = tui_hook_frame_begin,
        .frame_end      = tui_hook_frame_end,
        .should_quit    = tui_hook_should_quit,
        .get_delta_time = tui_hook_get_delta_time,
    },
    .window_state = &g_window_state,
};

/* ============================================================================
 * TUI_Window_use -- Provider Registration
 * ============================================================================ */

TUI_WindowState_t* TUI_Window_use(TUI_Window config) {
    g_tui_config = config;
    TUI_WindowState_ensure();
    cels_backend_register(&tui_backend_desc);
    return &TUI_WindowState;
}

volatile int* tui_window_get_running_ptr(void) {
    return &g_running;
}

/* Access standard window state (for input provider resize handling) */
CELS_WindowState* tui_window_get_standard_state(void) {
    return &g_window_state;
}
