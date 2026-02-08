/*
 * TUI Types - Coordinate types and geometry utilities for the TUI graphics API
 *
 * Foundational type vocabulary for coordinate mapping between Clay's float
 * layout engine and ncurses integer cell grid. TUI_Rect stores float
 * coordinates matching Clay_BoundingBox; TUI_CellRect stores integer cell
 * coordinates for ncurses drawing. Conversion and geometry utilities handle
 * the float-to-cell mapping and rectangle intersection/containment tests.
 *
 * All functions are static inline -- no link-time dependency beyond math.h.
 *
 * Usage:
 *   #include <cels-ncurses/tui_types.h>
 *
 *   TUI_Rect layout = { .x = 1.5f, .y = 2.7f, .w = 10.3f, .h = 5.1f };
 *   TUI_CellRect cells = tui_rect_to_cells(layout);
 *   -- cells = { .x = 1, .y = 2, .w = 11, .h = 6 }
 */

#ifndef CELS_NCURSES_TUI_TYPES_H
#define CELS_NCURSES_TUI_TYPES_H

#include <math.h>

/* ============================================================================
 * TUI_Rect -- Float coordinates (Clay-compatible)
 * ============================================================================
 *
 * Stores position and dimensions as floats, matching Clay_BoundingBox layout.
 * x/y = position (floorf snaps to top-left cell during conversion).
 * w/h = dimensions (ceilf ensures content is not clipped during conversion).
 * Negative x/y are valid (off-screen elements handled by clipping).
 */
typedef struct TUI_Rect {
    float x, y, w, h;
} TUI_Rect;

/* ============================================================================
 * TUI_CellRect -- Integer cell coordinates (ncurses-native)
 * ============================================================================
 *
 * Stores position and dimensions as integers for direct use with ncurses
 * drawing functions. Obtained from TUI_Rect via tui_rect_to_cells(), or
 * constructed directly for ncurses-native operations.
 * Negative x/y are valid (off-screen, handled by clipping).
 */
typedef struct TUI_CellRect {
    int x, y, w, h;
} TUI_CellRect;

/* ============================================================================
 * Conversion Utilities
 * ============================================================================ */

/*
 * Convert float rect to integer cell rect.
 * floorf for position: snap to top-left cell.
 * ceilf for dimensions: ensure content not clipped.
 */
static inline TUI_CellRect tui_rect_to_cells(TUI_Rect r) {
    return (TUI_CellRect){
        .x = (int)floorf(r.x),
        .y = (int)floorf(r.y),
        .w = (int)ceilf(r.w),
        .h = (int)ceilf(r.h)
    };
}

/* ============================================================================
 * Geometry Utilities
 * ============================================================================ */

/*
 * Compute intersection of two cell rects (for scissor/clip).
 * Returns a zero-area rect (w=0, h=0) if no overlap exists.
 */
static inline TUI_CellRect tui_cell_rect_intersect(TUI_CellRect a,
                                                    TUI_CellRect b) {
    int x1 = a.x > b.x ? a.x : b.x;
    int y1 = a.y > b.y ? a.y : b.y;
    int x2 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y2 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    int w = x2 - x1;
    int h = y2 - y1;
    if (w < 0) { w = 0; }
    if (h < 0) { h = 0; }
    return (TUI_CellRect){ .x = x1, .y = y1, .w = w, .h = h };
}

/*
 * Test if a point (px, py) is inside a cell rect.
 * Returns 1 if inside, 0 otherwise.
 */
static inline int tui_cell_rect_contains(TUI_CellRect r, int px, int py) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

#endif /* CELS_NCURSES_TUI_TYPES_H */
