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
 *   - NCursesWindow() to create the terminal window entity
 *   - TUILayer() to create layer entities at different z-orders
 *   - cel_watch(TUI_DrawContext_Component) to get a drawable surface
 *   - tui_draw_* functions for text, fills, and borders
 *   - CEL_System with cel_read(NCurses_InputState) for input handling
 *
 * Proves all three LAYR requirements:
 *   LAYR-01: Creates entities with TUI_LayerConfig (via TUILayer composition)
 *   LAYR-02: NCurses attaches TUI_DrawContext_Component (accessed via cel_watch)
 *   LAYR-03: Developer draws using tui_draw_text, tui_draw_fill_rect, tui_draw_border_rect
 *
 * Press 'q' to exit.
 */

#include <cels/cels.h>
#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>

CEL_Compose(World) {
    NCursesWindow(.title = "Layer Demo", .fps = 30) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        /* Background layer - fullscreen, z=0 */
        TUILayer(.z_order = 0, .visible = true) {
            const struct TUI_DrawContext_Component* dc = cel_watch(TUI_DrawContext_Component);
            if (!dc) return;
            TUI_DrawContext ctx = dc->ctx;

            TUI_Style bg_style = {
                .fg = tui_color_rgb(100, 100, 100),
                .bg = tui_color_rgb(20, 20, 40),
                .attrs = TUI_ATTR_NORMAL
            };
            tui_draw_fill_rect(&ctx,
                (TUI_CellRect){0, 0, ctx.width, ctx.height},
                '.', bg_style);
            tui_draw_text(&ctx, 2, 1, "Background Layer (z=0)", bg_style);
        }

        /* Overlay layer - fixed position/size, z=10 */
        TUILayer(.z_order = 10, .visible = true,
                 .x = 5, .y = 3, .width = 40, .height = 12) {
            const struct TUI_DrawContext_Component* dc = cel_watch(TUI_DrawContext_Component);
            if (!dc) return;
            TUI_DrawContext ctx = dc->ctx;

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

/* System: reads raw input each frame */
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

cels_main() {
    cels_register(NCurses);
    cels_register(GameInput);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
