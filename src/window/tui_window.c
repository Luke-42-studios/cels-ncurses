/*
 * TUI Window Provider - ncurses Implementation (Backend Descriptor)
 *
 * Implements the CELS_BackendDesc hook interface for ncurses TUI.
 * Core CELS owns the main loop; this module provides 6 hooks:
 *   startup, shutdown, frame_begin, frame_end, should_quit, get_delta_time
 */

#define _POSIX_C_SOURCE 199309L
#include <cels-ncurses/tui_window.h>
#include <flecs.h>
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

Engine_WindowState_t Engine_WindowState = {0};
cels_entity_t Engine_WindowStateID = 0;

void Engine_WindowState_ensure(void) {
    if (Engine_WindowStateID == 0) Engine_WindowStateID = cels_state_register("Engine_WindowState");
}

static TUI_Window g_tui_config;
static volatile int g_running = 1;
static int g_ncurses_active = 0;

/* CEL_Window singleton entity for new CEL_Query/CEL_Watch pattern */
static cels_entity_t g_cel_window_entity = 0;

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

    /* Populate Engine_WindowState */
    Engine_WindowState.state = WINDOW_STATE_READY;
    Engine_WindowState.width = g_tui_config.width > 0 ? g_tui_config.width : COLS;
    Engine_WindowState.height = g_tui_config.height > 0 ? g_tui_config.height : LINES;
    Engine_WindowState.title = g_tui_config.title;
    Engine_WindowState.version = g_tui_config.version;
    Engine_WindowState.target_fps = (float)fps;
    Engine_WindowState.delta_time = g_delta_time;
    cels_state_notify_change(Engine_WindowStateID);

    /* Create CEL_Window singleton entity for new CEL_Query/CEL_Watch pattern */
    CEL_Window_ensure();  /* Register CEL_Window component type */
    {
        CELS_Context* ctx = cels_get_context();
        ecs_world_t* world = cels_get_world(ctx);
        g_cel_window_entity = (cels_entity_t)ecs_entity(world, { .name = "CEL_Window" });
        ecs_set_id(world, (ecs_entity_t)g_cel_window_entity, (ecs_entity_t)CEL_WindowID,
                   sizeof(CEL_Window), &(CEL_Window){
                       .ready = true,
                       .width = Engine_WindowState.width,
                       .height = Engine_WindowState.height
                   });
        cels_component_notify_change(CEL_WindowID);
    }

    /* Populate standard CELS_WindowState */
    g_window_state.lifecycle = CELS_WINDOW_READY;
    g_window_state.width = Engine_WindowState.width;
    g_window_state.height = Engine_WindowState.height;
    g_window_state.title = Engine_WindowState.title;
    g_window_state.target_fps = Engine_WindowState.target_fps;
    g_window_state.delta_time = g_delta_time;
    g_window_state.backend_data = NULL;
}

/* ============================================================================
 * Backend Hook: shutdown()
 * ============================================================================ */

static void tui_hook_shutdown(void) {
    /* State transition */
    Engine_WindowState.state = WINDOW_STATE_CLOSING;
    cels_state_notify_change(Engine_WindowStateID);
    Engine_WindowState.state = WINDOW_STATE_CLOSED;

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
    Engine_WindowState.delta_time = g_delta_time;
    g_window_state.delta_time = g_delta_time;

    /* Update CEL_Window singleton each frame */
    if (g_cel_window_entity != 0) {
        int new_w = COLS;
        int new_h = LINES;
        /* Only update + notify if dimensions actually changed */
        if (new_w != Engine_WindowState.width || new_h != Engine_WindowState.height) {
            Engine_WindowState.width = new_w;
            Engine_WindowState.height = new_h;
            g_window_state.width = new_w;
            g_window_state.height = new_h;

            CEL_Window win_data = {
                .ready = true,
                .width = new_w,
                .height = new_h
            };
            ecs_world_t* world = cels_get_world(cels_get_context());
            ecs_set_id(world, (ecs_entity_t)g_cel_window_entity, (ecs_entity_t)CEL_WindowID,
                       sizeof(CEL_Window), &win_data);
            cels_component_notify_change(CEL_WindowID);
            cels_state_notify_change(Engine_WindowStateID);
        }
    }
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

Engine_WindowState_t* TUI_Window_use(TUI_Window config) {
    g_tui_config = config;
    Engine_WindowState_ensure();
    cels_backend_register(&tui_backend_desc);
    return &Engine_WindowState;
}

volatile int* tui_window_get_running_ptr(void) {
    return &g_running;
}

/* Access standard window state (for input provider resize handling) */
CELS_WindowState* tui_window_get_standard_state(void) {
    return &g_window_state;
}
