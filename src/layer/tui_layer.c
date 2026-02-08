/*
 * TUI Layer - Panel-backed layer system implementation
 *
 * Implements the layer lifecycle (create/destroy) and manages the global
 * layer registry. Each layer owns a WINDOW + PANEL pair. The panel library
 * provides z-order compositing; this module provides the lifecycle wrapper.
 *
 * Global state uses extern linkage (declared in tui_layer.h, defined here)
 * to ensure a single instance across the INTERFACE library pattern.
 */

#include <cels-ncurses/tui_layer.h>
#include <string.h>

/* ============================================================================
 * Global State (extern declarations in tui_layer.h)
 * ============================================================================ */

TUI_Layer g_layers[TUI_LAYER_MAX];
int g_layer_count = 0;

/* ============================================================================
 * tui_layer_create -- Allocate a new panel-backed layer
 * ============================================================================
 *
 * Creates a WINDOW via newwin() and wraps it in a PANEL via new_panel().
 * The panel is placed at the top of the stack automatically.
 * set_panel_userptr() stores a back-pointer to the TUI_Layer for reverse
 * lookup when iterating the panel stack.
 *
 * Returns pointer into g_layers on success, NULL on failure.
 */
TUI_Layer* tui_layer_create(const char* name, int x, int y, int w, int h) {
    if (g_layer_count >= TUI_LAYER_MAX) return NULL;

    TUI_Layer* layer = &g_layers[g_layer_count];

    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->name[sizeof(layer->name) - 1] = '\0';

    /* newwin(lines, cols, begin_y, begin_x) -- note ncurses parameter order */
    layer->win = newwin(h, w, y, x);
    if (!layer->win) return NULL;

    /* new_panel places the panel at the top of the stack */
    layer->panel = new_panel(layer->win);
    if (!layer->panel) {
        delwin(layer->win);
        layer->win = NULL;
        return NULL;
    }

    /* Store back-pointer for reverse lookup (PANEL* -> TUI_Layer*) */
    set_panel_userptr(layer->panel, layer);

    layer->x = x;
    layer->y = y;
    layer->width = w;
    layer->height = h;
    layer->visible = true;

    g_layer_count++;
    return layer;
}

/* ============================================================================
 * tui_layer_destroy -- Free a layer's PANEL and WINDOW
 * ============================================================================
 *
 * Cleanup order: del_panel first (removes from panel stack), then delwin
 * (frees the window memory). del_panel does NOT free the window.
 *
 * Array compaction: the destroyed slot is filled by copying the last element
 * into it (swap-remove). The moved element's panel userptr is updated to
 * reflect its new address in the array.
 */
void tui_layer_destroy(TUI_Layer* layer) {
    if (!layer || !layer->panel) return;

    /* Free panel first, then window (correct order per ncurses docs) */
    del_panel(layer->panel);
    delwin(layer->win);
    layer->panel = NULL;
    layer->win = NULL;

    /* Compact array: swap-remove with last element */
    int idx = (int)(layer - g_layers);
    if (idx < g_layer_count - 1) {
        g_layers[idx] = g_layers[g_layer_count - 1];
        /* Update moved element's panel userptr to point to new location */
        set_panel_userptr(g_layers[idx].panel, &g_layers[idx]);
    }
    g_layer_count--;
}

/* ============================================================================
 * tui_layer_show -- Make a layer visible in panel compositing
 * ============================================================================ */
void tui_layer_show(TUI_Layer* layer) {
    if (!layer || !layer->panel) return;
    show_panel(layer->panel);
    layer->visible = true;
}

/* ============================================================================
 * tui_layer_hide -- Exclude a layer from panel compositing
 * ============================================================================ */
void tui_layer_hide(TUI_Layer* layer) {
    if (!layer || !layer->panel) return;
    hide_panel(layer->panel);
    layer->visible = false;
}

/* ============================================================================
 * tui_layer_raise -- Move layer to top of z-order stack
 * ============================================================================ */
void tui_layer_raise(TUI_Layer* layer) {
    if (!layer || !layer->panel) return;
    top_panel(layer->panel);
}

/* ============================================================================
 * tui_layer_lower -- Move layer to bottom of z-order stack
 * ============================================================================ */
void tui_layer_lower(TUI_Layer* layer) {
    if (!layer || !layer->panel) return;
    bottom_panel(layer->panel);
}

/* ============================================================================
 * tui_layer_move -- Reposition layer on screen
 * ============================================================================
 *
 * Uses move_panel() to keep panel position tracking in sync.
 * NEVER use mvwin() on panel-managed windows -- it bypasses the panel
 * library and causes visual corruption.
 *
 * move_panel parameter order: (panel, starty, startx)
 */
void tui_layer_move(TUI_Layer* layer, int x, int y) {
    if (!layer || !layer->panel) return;
    move_panel(layer->panel, y, x);
    layer->x = x;
    layer->y = y;
}

/* ============================================================================
 * tui_layer_resize -- Change layer dimensions
 * ============================================================================
 *
 * wresize() changes the window dimensions in place. replace_panel() MUST
 * be called afterwards to update the panel's internal size bookkeeping,
 * even though the WINDOW pointer has not changed.
 *
 * wresize parameter order: (win, lines, cols)
 */
void tui_layer_resize(TUI_Layer* layer, int w, int h) {
    if (!layer || !layer->win || !layer->panel) return;
    wresize(layer->win, h, w);
    replace_panel(layer->panel, layer->win);
    layer->width = w;
    layer->height = h;
}

/* ============================================================================
 * tui_layer_get_draw_context -- Bridge layer to Phase 2 drawing primitives
 * ============================================================================
 *
 * Returns a TUI_DrawContext with LOCAL coordinates: (0,0) is the top-left
 * of the layer's own WINDOW, not the screen. Each panel-backed window has
 * its own coordinate system. The DrawContext borrows the layer's WINDOW
 * (does not own it).
 */
TUI_DrawContext tui_layer_get_draw_context(TUI_Layer* layer) {
    return tui_draw_context_create(layer->win, 0, 0, layer->width, layer->height);
}
