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
 * TUI Frame Pipeline - Implementation
 *
 * Implements the retained-mode frame lifecycle with dirty tracking.
 * frame_begin clears only dirty+visible layers (werase + style reset),
 * frame_end composites all visible layers via update_panels() + doupdate().
 *
 * Frame timing uses clock_gettime(CLOCK_MONOTONIC) for accurate delta_time
 * and FPS measurement.
 *
 * A default fullscreen background layer (z=0) is created at init time,
 * providing an immediate drawing surface that replaces stdscr usage.
 *
 * ECS integration: two standalone systems defined via CEL_System macro:
 * - TUI_FrameBeginSystem at PreRender (before renderer)
 * - TUI_FrameEndSystem at PostRender (after renderer)
 */

#include <cels_ncurses_draw.h>
#include "../tui_window.h"
#include <ncurses.h>
#include <panel.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#ifdef CELS_HAS_ECS
#include <cels/cels.h>
#endif

/* ============================================================================
 * Global State (extern declaration in tui_frame.h)
 * ============================================================================ */

TUI_FrameState g_frame_state = {0};

/* ============================================================================
 * Static State
 * ============================================================================ */

static TUI_Layer* g_background_layer = NULL;
static struct timespec g_frame_start = {0};
static struct timespec g_prev_frame_start = {0};

/* ============================================================================
 * Helpers
 * ============================================================================ */

/*
 * Compute the difference between two timespec values in seconds.
 * Returns a float suitable for delta_time (typically 0.016 for 60fps).
 */
static float timespec_diff_sec(struct timespec end, struct timespec start) {
    return (float)(end.tv_sec - start.tv_sec) +
           (float)(end.tv_nsec - start.tv_nsec) / 1e9f;
}

/* ============================================================================
 * tui_frame_init -- Initialize frame state, defer background layer creation
 * ============================================================================
 *
 * Zero-initializes g_frame_state and marks the background layer for deferred
 * creation. The actual layer is created on the first tui_frame_begin() call,
 * when ncurses is guaranteed to be initialized (COLS/LINES valid).
 */
static bool g_frame_initialized = false;

void tui_frame_init(void) {
    g_frame_state = (TUI_FrameState){0};
    g_background_layer = NULL;
    g_frame_initialized = true;
}

/*
 * Create background layer on first frame_begin (deferred from tui_frame_init).
 * Called once, after ncurses initscr() has set COLS and LINES.
 */
static void tui_frame_ensure_background(void) {
    if (g_background_layer) return;
    if (COLS <= 0 || LINES <= 0) return;

    g_background_layer = tui_layer_create("background", 0, 0, COLS, LINES);
    if (g_background_layer) {
        tui_layer_lower(g_background_layer);
    }
}

/* ============================================================================
 * tui_frame_begin -- Start a new frame
 * ============================================================================
 *
 * 1. Assert no nested calls (debug builds)
 * 2. Update frame timing via clock_gettime(CLOCK_MONOTONIC)
 * 3. Increment frame_count
 * 4. Clear dirty+visible layers: werase (NOT wclear) + wattr_set to default
 * 5. Set in_frame = true
 *
 * Only dirty layers are cleared (retained-mode). Non-dirty layers keep
 * their content from the previous frame.
 */
void tui_frame_begin(void) {
    assert(!g_frame_state.in_frame && "Nested tui_frame_begin() calls");

    /* Deferred background layer creation (needs ncurses to be active) */
    tui_frame_ensure_background();

    /* Frame timing */
    g_prev_frame_start = g_frame_start;
    clock_gettime(CLOCK_MONOTONIC, &g_frame_start);
    if (g_prev_frame_start.tv_sec != 0) {
        g_frame_state.delta_time = timespec_diff_sec(g_frame_start, g_prev_frame_start);
        if (g_frame_state.delta_time > 0.0f) {
            g_frame_state.fps = 1.0f / g_frame_state.delta_time;
        }
    }
    g_frame_state.frame_count++;

    /* Clear dirty+visible layers only (retained-mode) */
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].dirty && g_layers[i].visible) {
            werase(g_layers[i].win);
            wattr_set(g_layers[i].win, A_NORMAL, 0, NULL);
            if (g_layers[i].subcell_buf) {
                tui_subcell_buffer_clear(g_layers[i].subcell_buf);
            }
            g_layers[i].dirty = false;
        }
    }

    /* Block SIGWINCH during rendering to prevent ncurses corruption.
     * SIGWINCH arriving mid-doupdate/update_panels corrupts internal state.
     * Unblocked in tui_frame_end() so getch() can receive KEY_RESIZE. */
    sigset_t winch_set;
    sigemptyset(&winch_set);
    sigaddset(&winch_set, SIGWINCH);
    sigprocmask(SIG_BLOCK, &winch_set, NULL);

    g_frame_state.in_frame = true;
}

/* ============================================================================
 * tui_frame_end -- Finish the current frame
 * ============================================================================
 *
 * Composites all visible layers via the ncurses panel library.
 * update_panels() calls wnoutrefresh() internally for each visible panel.
 * doupdate() flushes all changes to the terminal in a single write.
 *
 * Must be called exactly once per frame, after all draw calls.
 */
void tui_frame_end(void) {
    assert(g_frame_state.in_frame && "tui_frame_end() without tui_frame_begin()");

    g_frame_state.in_frame = false;

    update_panels();
    doupdate();

    /* Unblock SIGWINCH so the next getch() can receive KEY_RESIZE */
    sigset_t winch_set;
    sigemptyset(&winch_set);
    sigaddset(&winch_set, SIGWINCH);
    sigprocmask(SIG_UNBLOCK, &winch_set, NULL);
}

/* ============================================================================
 * tui_frame_invalidate_all -- Force full redraw
 * ============================================================================
 *
 * Marks every layer as dirty, so the next tui_frame_begin() will clear
 * and redraw all layers. Use after terminal resize, mode changes, or any
 * event that invalidates retained layer content.
 */
void tui_frame_invalidate_all(void) {
    for (int i = 0; i < g_layer_count; i++) {
        g_layers[i].dirty = true;
    }
}

/* ============================================================================
 * tui_frame_get_background -- Access the default background layer
 * ============================================================================ */
TUI_Layer* tui_frame_get_background(void) {
    return g_background_layer;
}

#ifdef CELS_HAS_ECS
/* ============================================================================
 * ECS System Definitions -- replace raw ecs_system_init boilerplate
 * ============================================================================
 *
 * CEL_System generates Name_register() with external linkage.
 * TUI_FrameBeginSystem at PreRender (maps to EcsPreStore -- before renderer)
 * TUI_FrameEndSystem at PostRender (maps to EcsPostFrame -- after renderer)
 */

CEL_System(TUI_FrameBeginSystem, .phase = PreRender) {
    cel_run {
        tui_frame_begin();
    }
}

CEL_System(TUI_FrameEndSystem, .phase = PostRender) {
    cel_run {
        tui_frame_end();
        tui_hook_frame_end();
    }
}

/* ============================================================================
 * ncurses_register_frame_systems -- Module entry point
 * ============================================================================
 *
 * Called by CEL_Module(NCurses) init body. Initializes the frame pipeline
 * and registers the ECS frame systems. Overrides the weak stub in
 * ncurses_module.c.
 */
void ncurses_register_frame_systems(void) {
    tui_frame_init();
    TUI_FrameBeginSystem_register();
    TUI_FrameEndSystem_register();
}
#endif /* CELS_HAS_ECS */
