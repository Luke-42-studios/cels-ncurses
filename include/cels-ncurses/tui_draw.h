/*
 * TUI Draw - Drawing primitives for the TUI graphics API
 *
 * Provides all drawing function declarations and supporting types for the
 * Phase 2 drawing primitive system. Includes filled and outlined rectangles,
 * positioned and bounded text, per-side borders, horizontal/vertical lines,
 * and scissor (clip) region management.
 *
 * All drawing functions take TUI_DrawContext* as their first parameter and
 * clip against ctx->clip before issuing ncurses calls. No drawing function
 * calls wrefresh, wnoutrefresh, or doupdate -- screen updates are handled
 * by the frame pipeline in Phase 4.
 *
 * Usage:
 *   #include <cels-ncurses/tui_draw.h>
 *
 *   TUI_DrawContext ctx = tui_draw_context_create(win, 0, 0, 80, 24);
 *   TUI_Style style = { .fg = tui_color_rgb(255, 255, 255), .bg = TUI_COLOR_DEFAULT };
 *   TUI_CellRect rect = { .x = 5, .y = 2, .w = 20, .h = 10 };
 *
 *   tui_draw_fill_rect(&ctx, rect, ' ', style);
 *   tui_draw_border_rect(&ctx, rect, TUI_BORDER_ROUNDED, style);
 *   tui_draw_text(&ctx, 7, 4, "Hello, world!", style);
 */

#ifndef CELS_NCURSES_TUI_DRAW_H
#define CELS_NCURSES_TUI_DRAW_H

#include "cels-ncurses/tui_types.h"
#include "cels-ncurses/tui_color.h"
#include "cels-ncurses/tui_draw_context.h"
#include <ncurses.h>

/* ============================================================================
 * Types
 * ============================================================================
 *
 * Border style enum and border character struct used by rectangle, border,
 * and line drawing functions.
 */

/*
 * Border style selector for box-drawing characters.
 * SINGLE uses WACS_HLINE/WACS_VLINE/WACS_*CORNER macros.
 * DOUBLE uses WACS_D_HLINE/WACS_D_VLINE/WACS_D_*CORNER macros.
 * ROUNDED uses WACS_HLINE/WACS_VLINE with custom arc corner cchar_t values.
 */
typedef enum TUI_BorderStyle {
    TUI_BORDER_SINGLE,      /* WACS_HLINE / WACS_VLINE / WACS_*CORNER */
    TUI_BORDER_DOUBLE,      /* WACS_D_HLINE / WACS_D_VLINE / WACS_D_*CORNER */
    TUI_BORDER_ROUNDED,     /* WACS_HLINE / WACS_VLINE / custom arc corners */
    TUI_BORDER_HEAVY,       /* Heavy-weight lines (falls back to SINGLE if unsupported) */
    TUI_BORDER_NONE         /* No border drawn (early return in draw functions) */
} TUI_BorderStyle;

/*
 * Set of 6 box-drawing characters for a complete border: horizontal line,
 * vertical line, and four corners. Pointers reference either WACS_ macros
 * (which are already const cchar_t*) or static cchar_t globals for rounded
 * corners.
 */
typedef struct TUI_BorderChars {
    const cchar_t* hline;   /* Horizontal line character */
    const cchar_t* vline;   /* Vertical line character */
    const cchar_t* ul;      /* Upper-left corner */
    const cchar_t* ur;      /* Upper-right corner */
    const cchar_t* ll;      /* Lower-left corner */
    const cchar_t* lr;      /* Lower-right corner */
} TUI_BorderChars;

/* ============================================================================
 * Constants
 * ============================================================================
 *
 * Bitmask defines for per-side border control. Combine with bitwise OR
 * to select which sides of a rectangle to draw.
 */

#define TUI_SIDE_TOP     0x01
#define TUI_SIDE_RIGHT   0x02
#define TUI_SIDE_BOTTOM  0x04
#define TUI_SIDE_LEFT    0x08
#define TUI_SIDE_ALL     (TUI_SIDE_TOP | TUI_SIDE_RIGHT | TUI_SIDE_BOTTOM | TUI_SIDE_LEFT)

/* ============================================================================
 * Rectangle Drawing
 * ============================================================================
 *
 * Filled and outlined rectangle primitives. Both clip against ctx->clip.
 */

/* Draw a filled rectangle using the given chtype character and style.
 * Every cell within the intersection of rect and ctx->clip is filled. */
extern void tui_draw_fill_rect(TUI_DrawContext* ctx, TUI_CellRect rect,
                                chtype fill_ch, TUI_Style style);

/* Draw an outlined rectangle using box-drawing characters for the given
 * border style. Requires rect.w >= 2 and rect.h >= 2. Each border cell
 * is individually clipped against ctx->clip. */
extern void tui_draw_border_rect(TUI_DrawContext* ctx, TUI_CellRect rect,
                                  TUI_BorderStyle border_style,
                                  TUI_Style style);

/* ============================================================================
 * Text Drawing
 * ============================================================================
 *
 * Positioned text with UTF-8 support and bounded text with column limit.
 */

/* Draw UTF-8 text at position (x, y) with the given style.
 * Text is clipped horizontally and vertically against ctx->clip.
 * Wide characters (CJK) are handled correctly via wcwidth(). */
extern void tui_draw_text(TUI_DrawContext* ctx, int x, int y,
                           const char* text, TUI_Style style);

/* Draw UTF-8 text bounded to max_cols display columns.
 * Equivalent to drawing within a temporary clip of (x, y, max_cols, 1)
 * intersected with ctx->clip. */
extern void tui_draw_text_bounded(TUI_DrawContext* ctx, int x, int y,
                                   const char* text, int max_cols,
                                   TUI_Style style);

/* ============================================================================
 * Border Drawing
 * ============================================================================
 *
 * Per-side border control with corner logic: corners are drawn only when
 * both adjacent sides are enabled.
 */

/* Draw borders on selected sides of a rectangle.
 * The sides parameter is a bitmask of TUI_SIDE_* values. Corner characters
 * are placed only when both adjacent sides are enabled. When only one
 * adjacent side is present, its line character extends into the corner
 * position. */
extern void tui_draw_border(TUI_DrawContext* ctx, TUI_CellRect rect,
                             uint8_t sides, TUI_BorderStyle border_style,
                             TUI_Style style);

/* ============================================================================
 * Line Drawing
 * ============================================================================
 *
 * Horizontal and vertical lines using box-drawing characters.
 */

/* Draw a horizontal line at (x, y) extending length cells to the right.
 * Uses the horizontal line character for the given border style.
 * Clipped against ctx->clip. */
extern void tui_draw_hline(TUI_DrawContext* ctx, int x, int y, int length,
                            TUI_BorderStyle border_style, TUI_Style style);

/* Draw a vertical line at (x, y) extending length cells downward.
 * Uses the vertical line character for the given border style.
 * Clipped against ctx->clip. */
extern void tui_draw_vline(TUI_DrawContext* ctx, int x, int y, int length,
                            TUI_BorderStyle border_style, TUI_Style style);

/* ============================================================================
 * Internal Helpers
 * ============================================================================
 *
 * Border character lookup, used by border and line drawing code.
 * Declared extern since it lives in tui_draw.c.
 */

/* Return the set of 6 box-drawing characters for the given border style.
 * For TUI_BORDER_ROUNDED, initializes static cchar_t globals on first call. */
extern TUI_BorderChars tui_border_chars_get(TUI_BorderStyle border_style);

#endif /* CELS_NCURSES_TUI_DRAW_H */
