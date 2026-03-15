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
 * Layer Entity Demo
 *
 * Demonstrates the entity-driven layer API:
 *   - Compositions declare structure (window + layers)
 *   - Systems handle behavior (drawing + input)
 *
 * Pattern:
 *   CEL_Compose  = WHAT exists (entities, components)
 *   CEL_System   = WHAT HAPPENS each frame (drawing, input)
 *
 * Press 'q' to exit.
 */

#include <cels/cels.h>
#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>

/* ========================================================================== *
 * Structure: compositions declare what exists                                *
 * ========================================================================== */

CEL_Compose(World) {
    NCursesWindow(.title = "Layer Demo", .fps = 30) {}

    /* Watch window state — recomposes on terminal resize */
    const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
    if (!ws) return;

    TUILayer(.z_order = 0, .visible = true) {}
    TUILayer(.z_order = 10, .visible = true,
             .x = 5, .y = 3, .width = 40, .height = 12) {}
}

/* ========================================================================== *
 * Behavior: systems run each frame                                           *
 * ========================================================================== */

/* Render all layers based on z_order */
CEL_System(Renderer, .phase = OnRender) {
    cel_query(TUI_LayerConfig, TUI_DrawContext_Component);
    cel_each(TUI_LayerConfig, TUI_DrawContext_Component) {
        TUI_DrawContext ctx = TUI_DrawContext_Component->ctx;

        if (TUI_LayerConfig->z_order == 0) {
            /* Background layer */
            TUI_Style bg_style = {
                .fg = tui_color_rgb(100, 100, 100),
                .bg = tui_color_rgb(20, 20, 40),
                .attrs = TUI_ATTR_NORMAL
            };
            tui_draw_fill_rect(&ctx,
                (TUI_CellRect){0, 0, ctx.width, ctx.height},
                '.', bg_style);
            tui_draw_text(&ctx, 2, 1, "Background Layer (z=0)", bg_style);
        } else {
            /* Overlay layer */
            TUI_Style box_style = {
                .fg = tui_color_rgb(200, 200, 255),
                .bg = tui_color_rgb(30, 30, 60),
                .attrs = TUI_ATTR_BOLD
            };
            tui_draw_fill_rect(&ctx,
                (TUI_CellRect){0, 0, ctx.width, ctx.height},
                ' ', box_style);
            tui_draw_border_rect(&ctx,
                (TUI_CellRect){0, 0, ctx.width, ctx.height},
                TUI_BORDER_SINGLE, box_style);

            TUI_Style text_style = {
                .fg = tui_color_rgb(255, 255, 100),
                .bg = tui_color_rgb(30, 30, 60),
                .attrs = TUI_ATTR_NORMAL
            };
            tui_draw_text(&ctx, 2, 1, "Overlay Layer (z=10)", text_style);
            tui_draw_text(&ctx, 2, 3, "This layer is on top!", text_style);
            tui_draw_text(&ctx, 2, 5, "Press 'q' to quit", text_style);
        }
    }
}

/* Read input each frame */
CEL_System(GameInput, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_InputState* input = cel_read(NCurses_InputState);
        if (!input) return;
        for (int i = 0; i < input->key_count; i++) {
            if (input->keys[i] == 'q' || input->keys[i] == 'Q') {
                cels_request_quit();
                return;
            }
        }
    }
}

/* ========================================================================== *
 * Plumbing: wire it up and run                                               *
 * ========================================================================== */

cels_main() {
    cels_register(NCurses);
    cels_register(Renderer, GameInput);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
