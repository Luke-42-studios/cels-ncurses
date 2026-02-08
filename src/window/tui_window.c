/*
 * TUI Window Provider - ncurses Implementation
 *
 * Implements the TUI_Window provider that owns the terminal frame loop.
 * Uses ncurses for terminal management (replaces raw termios approach).
 *
 * Responsibilities:
 *   - ncurses initialization (initscr, cbreak, keypad, nodelay)
 *   - Color system setup (start_color, use_default_colors, assume_default_colors)
 *   - Frame loop with configurable FPS timing
 *   - WindowLifecycle state machine (fast-tracks NONE -> READY for TUI)
 *   - Clean shutdown: CLOSING -> CLOSED on quit, endwin() cleanup
 *
 * NOTE: This provider does NOT read input. Input is handled by TUI_Input
 * as an ECS system running during Engine_Progress().
 */

#include <cels-ncurses/tui_window.h>
#include <ncurses.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* Provider observable state (extern'd in tui_window.h) */
TUI_WindowState_t TUI_WindowState = {0};
cels_entity_t TUI_WindowStateID = 0;

void TUI_WindowState_ensure(void) {
    if (TUI_WindowStateID == 0) TUI_WindowStateID = cels_state_register("TUI_WindowState");
}

static TUI_Window g_tui_config;
static volatile int g_running = 1;
static int g_ncurses_active = 0;

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
 * Provider Startup (registered via cels_register_startup)
 * ============================================================================
 *
 * Initializes ncurses terminal mode with full key support and colors.
 * Called by cels_run_providers() before the frame loop starts.
 */
static void tui_window_startup(void) {
    signal(SIGINT, tui_sigint_handler);
    atexit(cleanup_endwin);

    /* Enable Unicode box-drawing characters (MUST be before initscr) */
    setlocale(LC_ALL, "");

    /* ncurses initialization */
    initscr();
    g_ncurses_active = 1;
    cbreak();                    /* char-at-a-time mode (replaces raw termios) */
    noecho();                    /* don't echo typed characters */
    keypad(stdscr, TRUE);        /* enable function key decoding */
    curs_set(0);                 /* hide cursor */
    nodelay(stdscr, TRUE);       /* non-blocking getch() */

    /* Fast escape key response (default ~1000ms is too slow) */
    ESCDELAY = 25;

    /* Color setup */
    if (has_colors()) {
        start_color();
        use_default_colors();
        assume_default_colors(-1, -1);  /* terminal theme compatibility */
    }

    /* INVARIANT: No code in this module calls mvprintw(), printw(),
     * addstr(), addch(), or any ncurses function that implicitly targets
     * stdscr for drawing. All drawing goes through TUI_DrawContext via
     * tui_draw_* functions. stdscr is retained only for input:
     * keypad(stdscr), nodelay(stdscr), wgetch(stdscr).
     * Violation check: grep -rn 'mvprintw\|printw\|addstr\|addch' src/ */
}

/* ============================================================================
 * Provider Frame Loop (registered via cels_register_frame_loop)
 * ============================================================================
 *
 * Owns the main loop: calls Engine_Progress, handles timing.
 * Input is read by TUI_Input ECS system during Engine_Progress.
 * Manages WindowLifecycle state transitions:
 *   - Fast-tracks to READY on start (TUI skips CREATED, SURFACE_READY)
 *   - Transitions CLOSING -> CLOSED on quit
 *
 * Called by cels_run_providers() -- blocks until window closes.
 */
static void tui_window_frame_loop(void) {
    int fps = g_tui_config.fps > 0 ? g_tui_config.fps : 60;
    float delta = 1.0f / (float)fps;

    /* Fast-track to READY (TUI skips CREATED, SURFACE_READY) */
    TUI_WindowState.state = WINDOW_STATE_READY;
    TUI_WindowState.width = g_tui_config.width > 0 ? g_tui_config.width : COLS;
    TUI_WindowState.height = g_tui_config.height > 0 ? g_tui_config.height : LINES;
    TUI_WindowState.title = g_tui_config.title;
    TUI_WindowState.version = g_tui_config.version;
    TUI_WindowState.target_fps = (float)fps;
    TUI_WindowState.delta_time = delta;
    cels_state_notify_change(TUI_WindowStateID);

    while (g_running) {
        Engine_Progress(delta);
        usleep((unsigned int)(delta * 1000000));
    }

    /* Transition: CLOSING -> CLOSED */
    TUI_WindowState.state = WINDOW_STATE_CLOSING;
    cels_state_notify_change(TUI_WindowStateID);
    TUI_WindowState.state = WINDOW_STATE_CLOSED;
}

/* ============================================================================
 * TUI_Window_use -- Provider Registration
 * ============================================================================
 *
 * Called by Use(TUI_Window, .title = "X", .fps = 60) inside Build() block.
 * Stores config and registers startup + frame_loop hooks with CELS runtime.
 */
TUI_WindowState_t* TUI_Window_use(TUI_Window config) {
    g_tui_config = config;
    TUI_WindowState_ensure();
    cels_register_startup(tui_window_startup);
    cels_register_frame_loop(tui_window_frame_loop);
    return &TUI_WindowState;
}

/* ============================================================================
 * Quit Signal Access
 * ============================================================================
 *
 * Returns pointer to g_running flag so TUI_Input can signal quit on Q key.
 */

volatile int* tui_window_get_running_ptr(void) {
    return &g_running;
}
