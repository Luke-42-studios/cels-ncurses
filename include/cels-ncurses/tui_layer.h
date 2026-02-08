/*
 * TUI Layer - Panel-backed layer system for z-ordered window compositing
 *
 * Provides the TUI_Layer struct and lifecycle API for managing panel-backed
 * layers. Each layer wraps an ncurses WINDOW* and PANEL* pair, giving
 * automatic z-order compositing via the ncurses panel library. Layers are
 * stored in a global fixed-capacity array with extern linkage (required by
 * the INTERFACE library pattern).
 *
 * The panel library handles overlap resolution, visibility, and refresh
 * ordering. All screen updates go through update_panels() + doupdate() --
 * never wrefresh() or wnoutrefresh() on panel-managed windows.
 *
 * Usage:
 *   #include <cels-ncurses/tui_layer.h>
 *
 *   TUI_Layer* bg = tui_layer_create("background", 0, 0, COLS, LINES);
 *   TUI_Layer* popup = tui_layer_create("popup", 10, 5, 40, 10);
 *   // Draw into layers via their WINDOW* (layer->win)
 *   update_panels();
 *   doupdate();
 *   tui_layer_destroy(popup);
 */

#ifndef CELS_NCURSES_TUI_LAYER_H
#define CELS_NCURSES_TUI_LAYER_H

#include <ncurses.h>
#include <panel.h>
#include <stdbool.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TUI_LAYER_MAX 32

/* ============================================================================
 * TUI_Layer -- Panel-backed layer
 * ============================================================================
 *
 * Each layer owns a WINDOW* and PANEL* pair. The panel provides automatic
 * z-order compositing via the ncurses panel library. Position and dimensions
 * are cached for convenience (canonical source is the panel/window).
 *
 * Lifecycle: tui_layer_create() allocates both window and panel.
 * tui_layer_destroy() frees panel first, then window (correct order).
 */
typedef struct TUI_Layer {
    char name[64];        /* Human-readable identifier */
    PANEL* panel;         /* Owned -- del_panel on destroy */
    WINDOW* win;          /* Owned -- delwin on destroy */
    int x, y;             /* Position (screen coordinates) */
    int width, height;    /* Dimensions */
    bool visible;         /* Visibility state */
} TUI_Layer;

/* ============================================================================
 * Global Layer State (extern -- defined in tui_layer.c)
 * ============================================================================
 *
 * Fixed-capacity array of layers. Uses extern declarations here with
 * definitions in tui_layer.c to avoid per-TU duplication in the INTERFACE
 * library pattern.
 */
extern TUI_Layer g_layers[TUI_LAYER_MAX];
extern int g_layer_count;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/*
 * Create a named layer at the given screen position and dimensions.
 * Allocates a WINDOW via newwin() and a PANEL via new_panel().
 * Returns pointer into g_layers on success, NULL on failure
 * (capacity full, newwin failed, or new_panel failed).
 */
extern TUI_Layer* tui_layer_create(const char* name, int x, int y, int w, int h);

/*
 * Destroy a layer, freeing its PANEL then WINDOW (correct order).
 * Compacts the g_layers array by swapping the destroyed slot with the
 * last element. Updates the moved element's panel userptr.
 * Safe to call with NULL or already-destroyed layer.
 */
extern void tui_layer_destroy(TUI_Layer* layer);

/* ============================================================================
 * Visibility
 * ============================================================================ */

/*
 * Show a hidden layer. Calls show_panel() to include the layer in
 * panel compositing. Sets layer->visible = true.
 */
extern void tui_layer_show(TUI_Layer* layer);

/*
 * Hide a visible layer. Calls hide_panel() to exclude the layer from
 * panel compositing. Sets layer->visible = false.
 */
extern void tui_layer_hide(TUI_Layer* layer);

/* ============================================================================
 * Z-Order
 * ============================================================================ */

/*
 * Raise a layer to the top of the z-order stack.
 * Calls top_panel() -- the layer will be drawn last (on top of all others).
 */
extern void tui_layer_raise(TUI_Layer* layer);

/*
 * Lower a layer to the bottom of the z-order stack.
 * Calls bottom_panel() -- the layer will be drawn first (behind all others).
 */
extern void tui_layer_lower(TUI_Layer* layer);

/* ============================================================================
 * Position
 * ============================================================================ */

/*
 * Move a layer to a new screen position.
 * Uses move_panel() (NOT mvwin) to keep panel position tracking in sync.
 * Updates layer->x and layer->y.
 */
extern void tui_layer_move(TUI_Layer* layer, int x, int y);

#endif /* CELS_NCURSES_TUI_LAYER_H */
