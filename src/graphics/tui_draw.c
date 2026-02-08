/*
 * TUI Draw - Rectangle drawing primitives and border character infrastructure
 *
 * Implements filled rectangles, outlined rectangles with box-drawing characters,
 * and the border character lookup table. All drawing functions clip against
 * TUI_DrawContext.clip before issuing ncurses calls. No function calls
 * wrefresh, wnoutrefresh, or doupdate.
 *
 * Supported border styles:
 *   SINGLE  - WACS_HLINE/WACS_VLINE with WACS_*CORNER macros
 *   DOUBLE  - WACS_D_HLINE/WACS_D_VLINE with WACS_D_*CORNER macros
 *   ROUNDED - WACS_HLINE/WACS_VLINE with custom arc corner cchar_t (U+256D-U+2570)
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * Static variables are per-consumer translation unit.
 */

#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include "cels-ncurses/tui_draw.h"
#include <stdbool.h>

/* ============================================================================
 * Rounded Corner cchar_t Initialization
 * ============================================================================
 *
 * ncurses provides WACS_ macros for single and double box-drawing corners,
 * but NOT for the rounded/arc variants (U+256D-U+2570). These must be
 * constructed manually via setcchar() on first use.
 */

static cchar_t g_round_ul, g_round_ur, g_round_ll, g_round_lr;
static bool g_round_corners_initialized = false;

static void init_rounded_corners(void) {
    if (g_round_corners_initialized) return;

    wchar_t wc[2];
    wc[1] = L'\0';

    wc[0] = 0x256D;  /* BOX DRAWINGS LIGHT ARC DOWN AND RIGHT */
    setcchar(&g_round_ul, wc, A_NORMAL, 0, NULL);

    wc[0] = 0x256E;  /* BOX DRAWINGS LIGHT ARC DOWN AND LEFT */
    setcchar(&g_round_ur, wc, A_NORMAL, 0, NULL);

    wc[0] = 0x256F;  /* BOX DRAWINGS LIGHT ARC UP AND LEFT */
    setcchar(&g_round_lr, wc, A_NORMAL, 0, NULL);

    wc[0] = 0x2570;  /* BOX DRAWINGS LIGHT ARC UP AND RIGHT */
    setcchar(&g_round_ll, wc, A_NORMAL, 0, NULL);

    g_round_corners_initialized = true;
}

/* ============================================================================
 * Border Character Lookup
 * ============================================================================
 *
 * Returns the set of 6 box-drawing characters for a given border style.
 * WACS_ macros are already const cchar_t* pointers. Rounded corner globals
 * need the & operator since they are cchar_t values.
 */

TUI_BorderChars tui_border_chars_get(TUI_BorderStyle border_style) {
    TUI_BorderChars chars;

    switch (border_style) {
        case TUI_BORDER_DOUBLE:
            chars.hline = WACS_D_HLINE;
            chars.vline = WACS_D_VLINE;
            chars.ul    = WACS_D_ULCORNER;
            chars.ur    = WACS_D_URCORNER;
            chars.ll    = WACS_D_LLCORNER;
            chars.lr    = WACS_D_LRCORNER;
            break;
        case TUI_BORDER_ROUNDED:
            init_rounded_corners();
            chars.hline = WACS_HLINE;
            chars.vline = WACS_VLINE;
            chars.ul    = &g_round_ul;
            chars.ur    = &g_round_ur;
            chars.ll    = &g_round_ll;
            chars.lr    = &g_round_lr;
            break;
        case TUI_BORDER_SINGLE: /* fall through */
        default:
            chars.hline = WACS_HLINE;
            chars.vline = WACS_VLINE;
            chars.ul    = WACS_ULCORNER;
            chars.ur    = WACS_URCORNER;
            chars.ll    = WACS_LLCORNER;
            chars.lr    = WACS_LRCORNER;
            break;
    }

    return chars;
}

/* ============================================================================
 * Filled Rectangle (DRAW-01)
 * ============================================================================
 *
 * Fills every cell within the intersection of rect and ctx->clip with the
 * given chtype character. Style is applied once before the fill loop.
 */

void tui_draw_fill_rect(TUI_DrawContext* ctx, TUI_CellRect rect,
                         chtype fill_ch, TUI_Style style) {
    TUI_CellRect visible = tui_cell_rect_intersect(rect, ctx->clip);
    if (visible.w <= 0 || visible.h <= 0) return;

    tui_style_apply(ctx->win, style);
    for (int row = visible.y; row < visible.y + visible.h; row++) {
        for (int col = visible.x; col < visible.x + visible.w; col++) {
            mvwaddch(ctx->win, row, col, fill_ch);
        }
    }
}

/* ============================================================================
 * Outlined Rectangle (DRAW-02, DRAW-06, DRAW-07)
 * ============================================================================
 *
 * Draws a border around the rectangle using box-drawing characters for the
 * given border style. Each individual cell (corners, edges) is clipped
 * against ctx->clip via tui_cell_rect_contains() before drawing.
 * Requires rect.w >= 2 and rect.h >= 2 to form a valid border.
 */

void tui_draw_border_rect(TUI_DrawContext* ctx, TUI_CellRect rect,
                           TUI_BorderStyle border_style, TUI_Style style) {
    if (rect.w < 2 || rect.h < 2) return;

    TUI_BorderChars chars = tui_border_chars_get(border_style);
    tui_style_apply(ctx->win, style);

    int x1 = rect.x;
    int y1 = rect.y;
    int x2 = rect.x + rect.w - 1;
    int y2 = rect.y + rect.h - 1;

    /* Corners */
    if (tui_cell_rect_contains(ctx->clip, x1, y1)) {
        mvwadd_wch(ctx->win, y1, x1, chars.ul);
    }
    if (tui_cell_rect_contains(ctx->clip, x2, y1)) {
        mvwadd_wch(ctx->win, y1, x2, chars.ur);
    }
    if (tui_cell_rect_contains(ctx->clip, x1, y2)) {
        mvwadd_wch(ctx->win, y2, x1, chars.ll);
    }
    if (tui_cell_rect_contains(ctx->clip, x2, y2)) {
        mvwadd_wch(ctx->win, y2, x2, chars.lr);
    }

    /* Top edge */
    for (int col = x1 + 1; col < x2; col++) {
        if (tui_cell_rect_contains(ctx->clip, col, y1)) {
            mvwadd_wch(ctx->win, y1, col, chars.hline);
        }
    }

    /* Bottom edge */
    for (int col = x1 + 1; col < x2; col++) {
        if (tui_cell_rect_contains(ctx->clip, col, y2)) {
            mvwadd_wch(ctx->win, y2, col, chars.hline);
        }
    }

    /* Left edge */
    for (int row = y1 + 1; row < y2; row++) {
        if (tui_cell_rect_contains(ctx->clip, x1, row)) {
            mvwadd_wch(ctx->win, row, x1, chars.vline);
        }
    }

    /* Right edge */
    for (int row = y1 + 1; row < y2; row++) {
        if (tui_cell_rect_contains(ctx->clip, x2, row)) {
            mvwadd_wch(ctx->win, row, x2, chars.vline);
        }
    }
}
