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
 * NCurses Module - CEL_Module(NCurses) Definition
 *
 * All CELS declarations for the NCurses module: lifecycles, observers,
 * systems, and compositions. Implementation details (ncurses panel ops,
 * terminal init) are delegated to helper functions in other source files.
 *
 * IMPORTANT: All code that uses component _id variables MUST live in
 * this file. CEL_Component generates static per-TU _id vars, and
 * cels_register only initializes THIS TU's copies. See docs/modules.md.
 */

#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>
#include <signal.h>
#include <panel.h>
#include "tui_window.h"

CEL_State(NCurses_WindowState);
CEL_State(NCurses_InputState);

#include "tui_internal.h"

/* ============================================================================
 * Window Lifecycle
 * ============================================================================ */

CEL_Lifecycle(NCursesWindowLC);

CEL_Observe(NCursesWindowLC, on_create) {
    if (ncurses_window_get_entity() != 0)
        return;
    ncurses_window_set_entity(entity);

    const NCurses_WindowConfig* config = cel_watch(entity, NCurses_WindowConfig);
    if (!config) {
        ncurses_window_set_entity(0);
        return;
    }

    ncurses_terminal_init((NCurses_WindowConfig*)config);
}

CEL_Observe(NCursesWindowLC, on_destroy) {
    (void)entity;
    ncurses_terminal_shutdown();
    ncurses_window_set_entity(0);
}

/* ============================================================================
 * Layer Lifecycle
 * ============================================================================ */

CEL_Lifecycle(TUI_LayerLC);

CEL_Observe(TUI_LayerLC, on_create) {
    const TUI_LayerConfig* config = cel_watch(entity, TUI_LayerConfig);
    if (!config) return;

    TUI_DrawContext_Component dc = ncurses_layer_panel_create(config, entity);
    if (!dc.win) return;

    cels_entity_set_component(entity, TUI_DrawContext_Component_id,
                               &dc, sizeof(dc));
    cels_component_notify_change(TUI_DrawContext_Component_id);
}

CEL_Observe(TUI_LayerLC, on_destroy) {
    const TUI_DrawContext_Component* dc = cel_watch(entity, TUI_DrawContext_Component);
    if (!dc) return;
    ncurses_layer_panel_destroy(dc);
}

/* ============================================================================
 * Layer System -- clears, syncs visibility, rebuilds z-order stack
 * ============================================================================ */

#define TUI_LAYER_MAX 32

CEL_System(TUI_LayerSystem, .phase = PreRender) {
    static int prev_cols = 0;
    static int prev_lines = 0;

    bool resized = (COLS != prev_cols || LINES != prev_lines) &&
                   (prev_cols > 0 || prev_lines > 0);
    prev_cols = COLS;
    prev_lines = LINES;

    PANEL* panels[TUI_LAYER_MAX];
    int z_orders[TUI_LAYER_MAX];
    int count = 0;

    cel_query(TUI_LayerConfig, TUI_DrawContext_Component);
    cel_each(TUI_LayerConfig, TUI_DrawContext_Component) {
        /* Resize fullscreen entity layers when terminal dimensions changed */
        if (resized &&
            (TUI_LayerConfig->width == 0 || TUI_LayerConfig->height == 0)) {
            int new_w = TUI_LayerConfig->width == 0 ? COLS : TUI_LayerConfig->width;
            int new_h = TUI_LayerConfig->height == 0 ? LINES : TUI_LayerConfig->height;
            cel_update(TUI_DrawContext_Component) {
                ncurses_layer_panel_resize(TUI_DrawContext_Component, new_w, new_h);
            }
        }

        ncurses_layer_sync_visibility(
            TUI_LayerConfig->visible, TUI_DrawContext_Component->panel);

        /* Auto-clear visible layers (developer gets blank canvas at OnRender) */
        if (TUI_LayerConfig->visible) {
            ncurses_layer_clear_window(
                TUI_DrawContext_Component->win,
                TUI_DrawContext_Component->subcell_buf);
        }

        if (count < TUI_LAYER_MAX && TUI_DrawContext_Component->panel) {
            panels[count] = TUI_DrawContext_Component->panel;
            z_orders[count] = TUI_LayerConfig->z_order;
            count++;
        }
    }

    ncurses_layer_sort_and_stack(panels, z_orders, count);
}

/* ============================================================================
 * Frame Begin System -- blocks SIGWINCH during rendering
 * ============================================================================ */

CEL_System(TUI_FrameBeginSystem, .phase = PreRender) {
    cel_run {
        sigset_t winch_set;
        sigemptyset(&winch_set);
        sigaddset(&winch_set, SIGWINCH);
        sigprocmask(SIG_BLOCK, &winch_set, NULL);
    }
}

/* ============================================================================
 * Frame End System -- composites panels, flushes to terminal, unblocks SIGWINCH
 * ============================================================================ */

CEL_System(TUI_FrameEndSystem, .phase = PostRender) {
    cel_run {
        update_panels();
        doupdate();

        sigset_t winch_set;
        sigemptyset(&winch_set);
        sigaddset(&winch_set, SIGWINCH);
        sigprocmask(SIG_UNBLOCK, &winch_set, NULL);

        tui_hook_frame_end();
    }
}

/* ============================================================================
 * Module Init
 * ============================================================================ */

CEL_Module(NCurses, init) {
    cels_register(NCurses_WindowState, NCurses_InputState,
                  NCursesWindowLC, NCurses_WindowUpdateSystem,
                  TUI_Renderable, TUI_LayerConfig, TUI_DrawContext_Component,
                  TUI_LayerLC);
    cels_register(TUI_LayerSystem,
                  TUI_FrameBeginSystem, TUI_FrameEndSystem);
}

/* ============================================================================
 * Compositions
 * ============================================================================ */

CEL_Composition(NCursesWindow) {
    cel_has(NCurses_WindowConfig,
        .title = cel.title,
        .fps = cel.fps,
        .color_mode = cel.color_mode
    );
    cels_lifecycle_bind_entity(NCursesWindowLC_id, cels_get_current_entity());
}

CEL_Composition(TUILayer) {
    cel_has(TUI_Renderable, ._unused = 0);
    cel_has(TUI_LayerConfig,
        .z_order = cel.z_order,
        .visible = cel.visible,
        .x = cel.x,
        .y = cel.y,
        .width = cel.width,
        .height = cel.height
    );
    cels_lifecycle_bind_entity(TUI_LayerLC_id, cels_get_current_entity());
}
