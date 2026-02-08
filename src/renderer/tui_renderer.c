/*
 * TUI Render Provider - ncurses Implementation
 *
 * Implements TUI rendering for Canvas entities using the Feature/Provides
 * pattern. Migrated from examples/tui_provider.c -- all printf+ANSI replaced
 * with ncurses mvprintw/attron/COLOR_PAIR.
 *
 * Architecture:
 *   _CEL_DefineFeature(Renderable, ...) - declares a render capability
 *   _CEL_Feature(Canvas, Renderable)    - Canvas supports Renderable
 *   _CEL_Provides(TUI, Renderable, Canvas, callback) - TUI implements it
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * components.h resolves via the consumer's include paths.
 */

#include "cels-ncurses/tui_renderer.h"
#include "cels-ncurses/tui_window.h"
#include "cels-ncurses/tui_components.h"
#include <cels/cels.h>
#include <flecs.h>
#include "components.h"

/* ============================================================================
 * Feature Definition
 * ============================================================================ */

_CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnStore, .priority = 0);

/* ============================================================================
 * Resolution Labels (for slider rendering)
 * ============================================================================ */

static const char* RESOLUTIONS[] = {
    "1920 x 1080",
    "2560 x 1440",
    "3840 x 2160"
};

/* Helper: get boolean display value for toggle settings */
static bool get_setting_bool(int setting_id, GraphicsSettings* gfx) {
    if (!gfx) return false;
    switch (setting_id) {
        case 1: return gfx->fullscreen;
        case 2: return gfx->vsync;
        default: return false;
    }
}

/* ============================================================================
 * Helper: Find GraphicsSettings entity in world
 * ============================================================================ */

static GraphicsSettings* find_graphics_settings(ecs_world_t* world) {
    if (GraphicsSettingsID == 0) return NULL;

    ecs_iter_t it = ecs_each_id(world, GraphicsSettingsID);
    while (ecs_each_next(&it)) {
        return ecs_field(&it, GraphicsSettings, 0);
    }
    return NULL;
}

/* ============================================================================
 * Render Provider Callback
 * ============================================================================
 *
 * Single coordinated render callback registered via _CEL_Provides().
 * Iterates Canvas entities and renders the full screen for each Canvas,
 * querying Selectable entities to determine rendering order.
 *
 * Uses ncurses functions exclusively -- no printf or ANSI escapes.
 * wnoutrefresh(stdscr) at end; doupdate() in window frame loop.
 */

static int g_render_frame = 0;

static void tui_prov_render_screen(CELS_Iter* it) {
    (void)it;
    g_render_frame++;

    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);
    if (!world) return;

    const CELS_Input* input = cels_input_get(ctx);
    GraphicsSettings* gfx = find_graphics_settings(world);

    /* Only redraw on state changes or periodic refresh */
    static int last_selected = -1;
    static int last_screen = -1;
    static int last_resolution = -1;
    static bool last_fullscreen = false;
    static bool last_vsync = false;

    bool force_redraw = (g_render_frame % 30 == 0);
    bool has_input = (input->axis_left[0] != 0 || input->axis_left[1] != 0 ||
                      input->button_accept || input->button_cancel);

    /* Read current screen from Canvas entities */
    int current_screen = -1;
    if (CanvasID != 0) {
        ecs_iter_t canvas_it = ecs_each_id(world, CanvasID);
        while (ecs_each_next(&canvas_it)) {
            Canvas* canvases = ecs_field(&canvas_it, Canvas, 0);
            for (int ci = 0; ci < canvas_it.count; ci++) {
                current_screen = (int)canvases[ci].screen;
            }
        }
    }

    /* Read current selection from Selectable entities */
    int current_selected = -1;
    if (SelectableID != 0) {
        ecs_iter_t sel_it = ecs_each_id(world, SelectableID);
        while (ecs_each_next(&sel_it)) {
            Selectable* sels = ecs_field(&sel_it, Selectable, 0);
            for (int si = 0; si < sel_it.count; si++) {
                if (sels[si].is_selected) {
                    current_selected = sels[si].index;
                }
            }
        }
    }

    bool changed = (current_screen != last_screen) ||
                   (current_selected != last_selected) ||
                   (gfx && gfx->resolution_index != last_resolution) ||
                   (gfx && gfx->fullscreen != last_fullscreen) ||
                   (gfx && gfx->vsync != last_vsync) ||
                   has_input || force_redraw;

    if (!changed) return;

    last_screen = current_screen;
    last_selected = current_selected;
    if (gfx) {
        last_resolution = gfx->resolution_index;
        last_fullscreen = gfx->fullscreen;
        last_vsync = gfx->vsync;
    }

    /* Clear screen and reset render row */
    erase();
    tui_render_reset_row();

    /* Render Canvas header */
    if (CanvasID != 0) {
        ecs_iter_t canvas_it = ecs_each_id(world, CanvasID);
        while (ecs_each_next(&canvas_it)) {
            Canvas* canvases = ecs_field(&canvas_it, Canvas, 0);
            for (int ci = 0; ci < canvas_it.count; ci++) {
                tui_render_canvas(canvases[ci].title);
            }
        }
    }

    /* Render Selectable entities by index order.
     * For each selectable, check what other components it has and
     * render the appropriate widget. */
    if (SelectableID != 0) {
        int max_index = 20;  /* Reasonable upper bound */

        for (int target_idx = 0; target_idx < max_index; target_idx++) {
            bool found = false;
            ecs_iter_t sel_it = ecs_each_id(world, SelectableID);
            while (ecs_each_next(&sel_it)) {
                Selectable* sels = ecs_field(&sel_it, Selectable, 0);
                for (int si = 0; si < sel_it.count; si++) {
                    if (sels[si].index != target_idx) continue;
                    found = true;

                    ecs_entity_t e = sel_it.entities[si];
                    bool selected = sels[si].is_selected;

                    /* Check if entity has Text + ClickArea (Button pattern) */
                    if (TextID != 0 && ClickAreaID != 0 &&
                        ecs_has_id(world, e, TextID) && ecs_has_id(world, e, ClickAreaID)) {
                        const Text* text = ecs_get_id(world, e, TextID);
                        if (text) {
                            tui_render_button(text->content, selected);
                        }
                    }
                    /* Check if entity has Text + Range (Slider pattern) */
                    else if (TextID != 0 && RangeID != 0 &&
                             ecs_has_id(world, e, TextID) && ecs_has_id(world, e, RangeID)) {
                        const Text* text = ecs_get_id(world, e, TextID);
                        const Range* range = ecs_get_id(world, e, RangeID);
                        if (text && range) {
                            if (range->type == RANGE_CYCLE) {
                                const char* value = RESOLUTIONS[gfx ? gfx->resolution_index : 0];
                                tui_render_slider_cycle(text->content, value, selected);
                            } else {
                                bool value = get_setting_bool(range->setting_id, gfx);
                                tui_render_slider_toggle(text->content, value, selected);
                            }
                        }
                    }
                }
            }
            if (!found) break;  /* No more items at this index or above */
        }
    }

    /* Controls hint */
    if (current_screen == SCREEN_MAIN_MENU) {
        tui_render_hint("[W/S: Navigate | Enter: Select | Q: Quit]");
    } else {
        tui_render_hint("[W/S: Navigate | A/D: Change | Enter: Save/Back | Esc: Back (no save)]");
    }

    /* Window info box */
    tui_render_info_box(TUI_WindowState.title, TUI_WindowState.version);

    /* Mark stdscr as needing refresh -- doupdate() in frame loop flushes */
    wnoutrefresh(stdscr);
}

/* ============================================================================
 * TUI Renderer Initialization -- declarative provider registration
 * ============================================================================ */

void tui_renderer_init(void) {
    /* Declare relationships -- macros handle feature + component registration */
    _CEL_Feature(Canvas, Renderable);
    _CEL_Provides(TUI, Renderable, Canvas, tui_prov_render_screen);

    /* Components consumed by the render callback */
    _CEL_ProviderConsumes(Text, ClickArea, Selectable, Range, GraphicsSettings);
}
