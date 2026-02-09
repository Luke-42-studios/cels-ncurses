/*
 * TUI Frame Pipeline - Begin/end frame lifecycle with dirty tracking
 *
 * Provides the retained-mode frame lifecycle that orchestrates layer
 * compositing with a single doupdate() per frame. Each frame follows:
 *
 *   tui_frame_begin()  -- clear dirty layers, reset style, update timing
 *   ... draw calls ...  -- via DrawContext from layers (auto-marks dirty)
 *   tui_frame_end()    -- composite via update_panels() + doupdate()
 *
 * Dirty tracking: layers keep content between frames. Only layers marked
 * dirty (via tui_layer_get_draw_context) are erased at frame_begin.
 * tui_frame_invalidate_all() forces every layer dirty for full redraws
 * (after resize, mode changes).
 *
 * Frame timing: measures real elapsed time via clock_gettime(CLOCK_MONOTONIC).
 * Provides delta_time, fps, and frame_count via g_frame_state.
 *
 * ECS integration: frame_begin runs at EcsPreStore (before renderer),
 * frame_end runs at EcsPostFrame (after renderer). Register via
 * tui_frame_register_systems().
 *
 * Usage:
 *   #include <cels-ncurses/tui_frame.h>
 *
 *   tui_frame_init();
 *   tui_frame_register_systems();
 *   // ... in ECS loop:
 *   // PreStore: tui_frame_begin() runs automatically
 *   // OnStore:  renderer draws into layers
 *   // PostFrame: tui_frame_end() runs automatically
 */

#ifndef CELS_NCURSES_TUI_FRAME_H
#define CELS_NCURSES_TUI_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "cels-ncurses/tui_layer.h"

/* ============================================================================
 * TUI_FrameState -- Per-frame timing and lifecycle state
 * ============================================================================
 *
 * Global frame state with extern linkage (INTERFACE library pattern).
 * Declared here, defined in tui_frame.c.
 */
typedef struct TUI_FrameState {
    uint64_t frame_count;    /* Total frames rendered */
    float delta_time;        /* Seconds since last frame */
    float fps;               /* Current FPS (1.0 / delta_time) */
    bool in_frame;           /* True between frame_begin and frame_end */
} TUI_FrameState;

extern TUI_FrameState g_frame_state;

/* ============================================================================
 * Frame Pipeline API
 * ============================================================================ */

/*
 * Initialize the frame pipeline. Background layer creation is deferred to
 * the first tui_frame_begin() call when ncurses is guaranteed active.
 * Safe to call before initscr().
 */
extern void tui_frame_init(void);

/*
 * Begin a new frame. Must be called once per frame before any draw calls.
 * - Updates frame timing (delta_time, fps, frame_count)
 * - Clears dirty+visible layers via werase (NOT wclear -- avoids flicker)
 * - Resets style to A_NORMAL on dirty layers
 * - Asserts on nested calls (debug builds)
 */
extern void tui_frame_begin(void);

/*
 * End the current frame. Must be called once per frame after all draw calls.
 * - Composites all visible layers via update_panels() + doupdate()
 * - Asserts if called without a matching tui_frame_begin() (debug builds)
 */
extern void tui_frame_end(void);

/*
 * Mark all layers as dirty, forcing a full redraw on the next frame_begin.
 * Use after terminal resize, mode changes, or any event that invalidates
 * all retained layer content.
 */
extern void tui_frame_invalidate_all(void);

#ifdef CELS_HAS_ECS
/*
 * Register frame pipeline ECS systems.
 * - TUI_FrameBeginSystem at EcsPreStore (runs before renderer at OnStore)
 * - TUI_FrameEndSystem at EcsPostFrame (runs after renderer at OnStore)
 */
extern void tui_frame_register_systems(void);
#endif

/*
 * Get the default background layer (created by tui_frame_init).
 * Returns NULL if tui_frame_init has not been called.
 */
extern TUI_Layer* tui_frame_get_background(void);

#endif /* CELS_NCURSES_TUI_FRAME_H */
