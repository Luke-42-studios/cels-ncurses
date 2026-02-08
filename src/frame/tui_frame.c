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
 * ECS integration: two standalone systems registered via ecs_system_init():
 * - TUI_FrameBeginSystem at EcsPreStore (before renderer)
 * - TUI_FrameEndSystem at EcsPostFrame (after renderer)
 */

#include <cels-ncurses/tui_frame.h>
#include <cels-ncurses/tui_layer.h>
#include <ncurses.h>
#include <panel.h>
#include <time.h>
#include <assert.h>
#include <cels/cels.h>
#include <flecs.h>

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
 * tui_frame_init -- Create background layer, initialize state
 * ============================================================================
 *
 * Creates a fullscreen background layer at z=0 (bottom of z-order stack).
 * Must be called after ncurses initialization so COLS and LINES are valid.
 * Zero-initializes g_frame_state.
 */
void tui_frame_init(void) {
    g_background_layer = tui_layer_create("background", 0, 0, COLS, LINES);
    if (g_background_layer) {
        tui_layer_lower(g_background_layer);
    }

    g_frame_state = (TUI_FrameState){0};
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
            g_layers[i].dirty = false;
        }
    }

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

/* ============================================================================
 * ECS System Callbacks
 * ============================================================================
 *
 * Standalone system callbacks (no query terms). Registered at specific
 * ECS phases to bracket the renderer.
 */

static void tui_frame_begin_callback(ecs_iter_t* it) {
    (void)it;
    tui_frame_begin();
}

static void tui_frame_end_callback(ecs_iter_t* it) {
    (void)it;
    tui_frame_end();
}

/* ============================================================================
 * tui_frame_register_systems -- Register ECS systems at PreStore/PostFrame
 * ============================================================================
 *
 * Follows the exact pattern from tui_input.c (lines 149-161).
 * - TUI_FrameBeginSystem at EcsPreStore (runs before renderer at OnStore)
 * - TUI_FrameEndSystem at EcsPostFrame (runs after renderer at OnStore)
 */
void tui_frame_register_systems(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    /* frame_begin at PreStore (before renderer) */
    {
        ecs_system_desc_t sys_desc = {0};
        ecs_entity_desc_t entity_desc = {0};
        entity_desc.name = "TUI_FrameBeginSystem";
        ecs_id_t phase_ids[3] = {
            ecs_pair(EcsDependsOn, EcsPreStore),
            EcsPreStore,
            0
        };
        entity_desc.add = phase_ids;
        sys_desc.entity = ecs_entity_init(world, &entity_desc);
        sys_desc.callback = tui_frame_begin_callback;
        ecs_system_init(world, &sys_desc);
    }

    /* frame_end at PostFrame (after renderer) */
    {
        ecs_system_desc_t sys_desc = {0};
        ecs_entity_desc_t entity_desc = {0};
        entity_desc.name = "TUI_FrameEndSystem";
        ecs_id_t phase_ids[3] = {
            ecs_pair(EcsDependsOn, EcsPostFrame),
            EcsPostFrame,
            0
        };
        entity_desc.add = phase_ids;
        sys_desc.entity = ecs_entity_init(world, &entity_desc);
        sys_desc.callback = tui_frame_end_callback;
        ecs_system_init(world, &sys_desc);
    }
}
