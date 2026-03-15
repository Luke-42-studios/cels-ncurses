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
 * Surface Panel Operations - Pure ncurses panel management
 *
 * Helper functions for creating, destroying, clearing, and stacking
 * ncurses PANEL/WINDOW resources for surface entities. Called by
 * lifecycle observers and systems in ncurses_module.c.
 *
 * No CELS component _id usage here -- all data is passed in by the
 * caller. Safe for any translation unit.
 */

#ifdef CELS_HAS_ECS

#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>
#include <ncurses.h>
#include <panel.h>
#include <stdint.h>

TUI_DrawContext_Component ncurses_surface_panel_create(
    const TUI_SurfaceConfig* config, cels_entity_t entity)
{
    TUI_DrawContext_Component dc = {0};

    int w = config->width > 0 ? config->width : COLS;
    int h = config->height > 0 ? config->height : LINES;

    WINDOW* win = newwin(h, w, config->y, config->x);
    if (!win) return dc;

    PANEL* panel = new_panel(win);
    if (!panel) {
        delwin(win);
        return dc;
    }

    set_panel_userptr(panel, (void*)(uintptr_t)entity);

    if (!config->visible) {
        hide_panel(panel);
    }

    dc.ctx = tui_draw_context_create(win, 0, 0, w, h);
    dc.dirty = false;
    dc.panel = panel;
    dc.win = win;
    dc.subcell_buf = NULL;

    return dc;
}

void ncurses_surface_panel_destroy(const TUI_DrawContext_Component* dc) {
    if (dc->subcell_buf) {
        tui_subcell_buffer_destroy(dc->subcell_buf);
    }
    if (dc->panel) del_panel(dc->panel);
    if (dc->win) delwin(dc->win);
}

void ncurses_surface_panel_resize(TUI_DrawContext_Component* dc, int new_w, int new_h) {
    if (!dc || !dc->win || !dc->panel) return;

    wresize(dc->win, new_h, new_w);
    replace_panel(dc->panel, dc->win);
    werase(dc->win);

    dc->ctx = tui_draw_context_create(dc->win, 0, 0, new_w, new_h);
    dc->dirty = true;

    if (dc->subcell_buf) {
        tui_subcell_buffer_resize(dc->subcell_buf, new_w, new_h);
    }
}

void ncurses_surface_sync_visibility(bool visible, PANEL* panel) {
    if (!panel) return;
    if (visible && panel_hidden(panel)) {
        show_panel(panel);
    } else if (!visible && !panel_hidden(panel)) {
        hide_panel(panel);
    }
}

void ncurses_surface_clear_window(WINDOW* win, TUI_SubCellBuffer* subcell_buf) {
    if (!win) return;
    werase(win);
    wattr_set(win, A_NORMAL, 0, NULL);
    if (subcell_buf) {
        tui_subcell_buffer_clear(subcell_buf);
    }
}

void ncurses_surface_sort_and_stack(PANEL** panels, int* z_orders, int count) {
    if (count <= 1) {
        if (count == 1 && panels[0]) top_panel(panels[0]);
        return;
    }

    /* Insertion sort by z_order ascending (max 32 entries) */
    for (int i = 1; i < count; i++) {
        PANEL* kp = panels[i];
        int kz = z_orders[i];
        int j = i - 1;
        while (j >= 0 && z_orders[j] > kz) {
            panels[j + 1] = panels[j];
            z_orders[j + 1] = z_orders[j];
            j--;
        }
        panels[j + 1] = kp;
        z_orders[j + 1] = kz;
    }

    /* Rebuild panel stack: top_panel in ascending z_order */
    for (int i = 0; i < count; i++) {
        if (panels[i]) top_panel(panels[i]);
    }
}

#endif /* CELS_HAS_ECS */
