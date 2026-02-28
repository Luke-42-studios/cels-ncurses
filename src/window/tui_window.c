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
 * TUI Window - Observer-Based Terminal Lifecycle
 *
 * Implements the NCurses window lifecycle via flecs observers:
 *   - EcsOnSet NCurses_WindowConfig  -> initialize terminal, attach WindowState
 *   - EcsOnRemove NCurses_WindowConfig -> shut down terminal cleanly
 *
 * The observer fires when a developer adds NCurses_WindowConfig to an entity
 * (typically via the NCursesWindow() call macro). Single window entity
 * enforced -- second config logs warning and is ignored.
 *
 * ncurses_window_frame_update() is called each frame by the frame pipeline
 * to update window dimensions on resize and propagate delta_time.
 */

#define _POSIX_C_SOURCE 199309L
#include <cels-ncurses/tui_window.h>
#include <cels-ncurses/tui_ncurses.h>
#include <cels-ncurses/tui_color.h>
#include <cels/cels.h>
#include <flecs.h>
#include <ncurses.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <locale.h>
#include <time.h>
#include <stdio.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* The single window entity (0 = no window) */
static ecs_entity_t g_window_entity = 0;

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
 * Terminal Init (extracted from old tui_hook_startup)
 * ============================================================================
 *
 * Initializes the ncurses terminal from component config data.
 * Sets signal handler, atexit, locale, initscr, input mode, color system,
 * and stores FPS for timing calculations.
 */

static void ncurses_terminal_init(NCurses_WindowConfig* config) {
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

    g_running = 1;
}

/* ============================================================================
 * Terminal Shutdown
 * ============================================================================ */

static void ncurses_terminal_shutdown(void) {
    if (g_ncurses_active && !isendwin()) {
        endwin();
        g_ncurses_active = 0;
    }
}

/* ============================================================================
 * Observer: on_window_config_set (EcsOnSet NCurses_WindowConfig)
 * ============================================================================
 *
 * When a developer creates an entity with NCurses_WindowConfig, this observer
 * fires and initializes the ncurses terminal. It attaches NCurses_WindowState
 * to the same entity with initial dimensions and running state.
 *
 * Single window entity enforced: if g_window_entity is already set, log
 * warning and ignore.
 */

static void on_window_config_set(ecs_iter_t* it) {
    if (g_window_entity != 0) {
        fprintf(stderr, "[NCurses] Warning: only one window entity allowed, ignoring\n");
        return;
    }

    NCurses_WindowConfig* configs = ecs_field(it, NCurses_WindowConfig, 0);
    for (int i = 0; i < it->count; i++) {
        g_window_entity = it->entities[i];

        /* Initialize ncurses terminal */
        ncurses_terminal_init(&configs[i]);

        /* Attach NCurses_WindowState to the same entity */
        ecs_world_t* world = it->world;
        NCurses_WindowState state = {
            .width = COLS,
            .height = LINES,
            .running = true,
            .actual_fps = (float)g_target_fps,
            .delta_time = g_delta_time
        };
        ecs_set_id(world, it->entities[i], NCurses_WindowState_id,
                   sizeof(NCurses_WindowState), &state);
    }
}

/* ============================================================================
 * Observer: on_window_config_removed (EcsOnRemove NCurses_WindowConfig)
 * ============================================================================ */

static void on_window_config_removed(ecs_iter_t* it) {
    (void)it;
    ncurses_terminal_shutdown();
    g_window_entity = 0;
}

/* ============================================================================
 * Observer Registration Functions
 * ============================================================================
 *
 * These override the weak stubs in ncurses_module.c.
 */

void ncurses_register_window_observer(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    /* Ensure the component ID is populated in this TU */
    cels_ensure_component(&NCurses_WindowConfig_id, "NCurses_WindowConfig",
                          sizeof(NCurses_WindowConfig), CELS_ALIGNOF(NCurses_WindowConfig));

    ecs_observer_desc_t desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "NCurses_WindowConfigObserver";
    desc.entity = ecs_entity_init(world, &entity_desc);
    desc.query.terms[0].id = NCurses_WindowConfig_id;
    desc.events[0] = EcsOnSet;
    desc.callback = on_window_config_set;
    ecs_observer_init(world, &desc);
}

void ncurses_register_window_remove_observer(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    ecs_observer_desc_t desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "NCurses_WindowConfigRemoveObserver";
    desc.entity = ecs_entity_init(world, &entity_desc);
    desc.query.terms[0].id = NCurses_WindowConfig_id;
    desc.events[0] = EcsOnRemove;
    desc.callback = on_window_config_removed;
    ecs_observer_init(world, &desc);
}

/* ============================================================================
 * Frame Update: ncurses_window_frame_update()
 * ============================================================================
 *
 * Called each frame by the frame pipeline (tui_frame_begin_callback) to:
 *   1. Update frame timing (delta_time)
 *   2. Detect terminal resize (COLS/LINES changed) and update WindowState
 *   3. Check g_running -- if false, set WindowState.running = false and quit
 *
 * On resize, ecs_set_id triggers recomposition for anything watching
 * NCurses_WindowState via cel_watch().
 */

void ncurses_window_frame_update(void) {
    if (g_window_entity == 0) return;

    /* Timing */
    g_prev_frame_start = g_frame_start;
    clock_gettime(CLOCK_MONOTONIC, &g_frame_start);
    if (g_prev_frame_start.tv_sec != 0) {
        g_delta_time = (float)(g_frame_start.tv_sec - g_prev_frame_start.tv_sec) +
                       (float)(g_frame_start.tv_nsec - g_prev_frame_start.tv_nsec) / 1e9f;
    }

    /* Check if quit was requested (SIGINT or input 'q') */
    if (!g_running) {
        ecs_world_t* world = cels_get_world(cels_get_context());
        NCurses_WindowState state = {
            .width = g_last_cols,
            .height = g_last_lines,
            .running = false,
            .actual_fps = (g_delta_time > 0.0f) ? 1.0f / g_delta_time : 0.0f,
            .delta_time = g_delta_time
        };
        ecs_set_id(world, g_window_entity, NCurses_WindowState_id,
                   sizeof(NCurses_WindowState), &state);
        cels_request_quit();
        return;
    }

    /* Detect resize */
    int new_w = COLS;
    int new_h = LINES;
    bool resized = (new_w != g_last_cols || new_h != g_last_lines);

    if (resized) {
        g_last_cols = new_w;
        g_last_lines = new_h;
    }

    /* Update WindowState on entity (always for delta_time, trigger watch on resize) */
    if (resized) {
        ecs_world_t* world = cels_get_world(cels_get_context());
        NCurses_WindowState state = {
            .width = new_w,
            .height = new_h,
            .running = true,
            .actual_fps = (g_delta_time > 0.0f) ? 1.0f / g_delta_time : 0.0f,
            .delta_time = g_delta_time
        };
        ecs_set_id(world, g_window_entity, NCurses_WindowState_id,
                   sizeof(NCurses_WindowState), &state);
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

/* ============================================================================
 * Running Pointer Bridge
 * ============================================================================
 *
 * Kept for backward compatibility with tui_input.c which uses it to signal
 * quit on 'q' key press. Will be removed when input system fully transitions
 * to cels_request_quit().
 */

volatile int* tui_window_get_running_ptr(void) {
    return &g_running;
}
