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

#include "cels-ncurses/tui_draw.h"
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>

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
        case TUI_BORDER_HEAVY:
#ifdef WACS_T_HLINE
            chars.hline = WACS_T_HLINE;
            chars.vline = WACS_T_VLINE;
            chars.ul    = WACS_T_ULCORNER;
            chars.ur    = WACS_T_URCORNER;
            chars.ll    = WACS_T_LLCORNER;
            chars.lr    = WACS_T_LRCORNER;
#else
            /* Fall back to SINGLE if heavy line chars unavailable */
            chars.hline = WACS_HLINE;
            chars.vline = WACS_VLINE;
            chars.ul    = WACS_ULCORNER;
            chars.ur    = WACS_URCORNER;
            chars.ll    = WACS_LLCORNER;
            chars.lr    = WACS_LRCORNER;
#endif
            break;
        case TUI_BORDER_NONE:
            /* Return SINGLE as fallback -- callers should early-return for NONE */
            chars.hline = WACS_HLINE;
            chars.vline = WACS_VLINE;
            chars.ul    = WACS_ULCORNER;
            chars.ur    = WACS_URCORNER;
            chars.ll    = WACS_LLCORNER;
            chars.lr    = WACS_LRCORNER;
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
    if (border_style == TUI_BORDER_NONE) return;

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

/* ============================================================================
 * Line Drawing (DRAW-11)
 * ============================================================================
 *
 * Horizontal and vertical lines using box-drawing characters from
 * tui_border_chars_get(). Lines are clipped against ctx->clip before drawing.
 * Uses ncurses built-in mvwhline_set/mvwvline_set for efficiency.
 */

void tui_draw_hline(TUI_DrawContext* ctx, int x, int y, int length,
                     TUI_BorderStyle border_style, TUI_Style style) {
    if (length <= 0) return;

    /* Clip vertically: if row is outside clip region, nothing to draw */
    if (y < ctx->clip.y || y >= ctx->clip.y + ctx->clip.h) return;

    /* Clip horizontally */
    int left = (x > ctx->clip.x) ? x : ctx->clip.x;
    int right_end = x + length;
    int clip_right = ctx->clip.x + ctx->clip.w;
    int right = (right_end < clip_right) ? right_end : clip_right;
    int visible_len = right - left;
    if (visible_len <= 0) return;

    TUI_BorderChars chars = tui_border_chars_get(border_style);
    tui_style_apply(ctx->win, style);
    mvwhline_set(ctx->win, y, left, chars.hline, visible_len);
}

void tui_draw_vline(TUI_DrawContext* ctx, int x, int y, int length,
                     TUI_BorderStyle border_style, TUI_Style style) {
    if (length <= 0) return;

    /* Clip horizontally: if column is outside clip region, nothing to draw */
    if (x < ctx->clip.x || x >= ctx->clip.x + ctx->clip.w) return;

    /* Clip vertically */
    int top = (y > ctx->clip.y) ? y : ctx->clip.y;
    int bottom_end = y + length;
    int clip_bottom = ctx->clip.y + ctx->clip.h;
    int bottom = (bottom_end < clip_bottom) ? bottom_end : clip_bottom;
    int visible_len = bottom - top;
    if (visible_len <= 0) return;

    TUI_BorderChars chars = tui_border_chars_get(border_style);
    tui_style_apply(ctx->win, style);
    mvwvline_set(ctx->win, top, x, chars.vline, visible_len);
}

/* ============================================================================
 * Border Drawing (DRAW-05, DRAW-06, DRAW-07)
 * ============================================================================
 *
 * Per-side border drawing with corner logic. Corners are placed only when
 * both adjacent sides are enabled; when only one adjacent side is present,
 * that side's line character extends into the corner cell. Supports single,
 * double, and rounded border styles via tui_border_chars_get().
 *
 * Uses per-cell loops with tui_cell_rect_contains (not mvwhline_set/
 * mvwvline_set) because line segments exclude corner cells and each cell
 * needs an individual clip check.
 */

void tui_draw_border(TUI_DrawContext* ctx, TUI_CellRect rect,
                      uint8_t sides, TUI_BorderStyle border_style,
                      TUI_Style style) {
    if (rect.w < 2 || rect.h < 2) return;
    if (sides == 0) return;
    if (border_style == TUI_BORDER_NONE) return;

    TUI_BorderChars chars = tui_border_chars_get(border_style);
    tui_style_apply(ctx->win, style);

    /* Corner positions */
    int ul_x = rect.x;
    int ul_y = rect.y;
    int ur_x = rect.x + rect.w - 1;
    int ur_y = rect.y;
    int ll_x = rect.x;
    int ll_y = rect.y + rect.h - 1;
    int lr_x = rect.x + rect.w - 1;
    int lr_y = rect.y + rect.h - 1;

    /* Upper-left corner */
    if ((sides & TUI_SIDE_TOP) && (sides & TUI_SIDE_LEFT)) {
        if (tui_cell_rect_contains(ctx->clip, ul_x, ul_y)) {
            mvwadd_wch(ctx->win, ul_y, ul_x, chars.ul);
        }
    } else if (sides & TUI_SIDE_TOP) {
        if (tui_cell_rect_contains(ctx->clip, ul_x, ul_y)) {
            mvwadd_wch(ctx->win, ul_y, ul_x, chars.hline);
        }
    } else if (sides & TUI_SIDE_LEFT) {
        if (tui_cell_rect_contains(ctx->clip, ul_x, ul_y)) {
            mvwadd_wch(ctx->win, ul_y, ul_x, chars.vline);
        }
    }

    /* Upper-right corner */
    if ((sides & TUI_SIDE_TOP) && (sides & TUI_SIDE_RIGHT)) {
        if (tui_cell_rect_contains(ctx->clip, ur_x, ur_y)) {
            mvwadd_wch(ctx->win, ur_y, ur_x, chars.ur);
        }
    } else if (sides & TUI_SIDE_TOP) {
        if (tui_cell_rect_contains(ctx->clip, ur_x, ur_y)) {
            mvwadd_wch(ctx->win, ur_y, ur_x, chars.hline);
        }
    } else if (sides & TUI_SIDE_RIGHT) {
        if (tui_cell_rect_contains(ctx->clip, ur_x, ur_y)) {
            mvwadd_wch(ctx->win, ur_y, ur_x, chars.vline);
        }
    }

    /* Lower-left corner */
    if ((sides & TUI_SIDE_BOTTOM) && (sides & TUI_SIDE_LEFT)) {
        if (tui_cell_rect_contains(ctx->clip, ll_x, ll_y)) {
            mvwadd_wch(ctx->win, ll_y, ll_x, chars.ll);
        }
    } else if (sides & TUI_SIDE_BOTTOM) {
        if (tui_cell_rect_contains(ctx->clip, ll_x, ll_y)) {
            mvwadd_wch(ctx->win, ll_y, ll_x, chars.hline);
        }
    } else if (sides & TUI_SIDE_LEFT) {
        if (tui_cell_rect_contains(ctx->clip, ll_x, ll_y)) {
            mvwadd_wch(ctx->win, ll_y, ll_x, chars.vline);
        }
    }

    /* Lower-right corner */
    if ((sides & TUI_SIDE_BOTTOM) && (sides & TUI_SIDE_RIGHT)) {
        if (tui_cell_rect_contains(ctx->clip, lr_x, lr_y)) {
            mvwadd_wch(ctx->win, lr_y, lr_x, chars.lr);
        }
    } else if (sides & TUI_SIDE_BOTTOM) {
        if (tui_cell_rect_contains(ctx->clip, lr_x, lr_y)) {
            mvwadd_wch(ctx->win, lr_y, lr_x, chars.hline);
        }
    } else if (sides & TUI_SIDE_RIGHT) {
        if (tui_cell_rect_contains(ctx->clip, lr_x, lr_y)) {
            mvwadd_wch(ctx->win, lr_y, lr_x, chars.vline);
        }
    }

    /* Top side (line segment between corners) */
    if (sides & TUI_SIDE_TOP) {
        for (int col = rect.x + 1; col <= rect.x + rect.w - 2; col++) {
            if (tui_cell_rect_contains(ctx->clip, col, rect.y)) {
                mvwadd_wch(ctx->win, rect.y, col, chars.hline);
            }
        }
    }

    /* Bottom side (line segment between corners) */
    if (sides & TUI_SIDE_BOTTOM) {
        int bottom_y = rect.y + rect.h - 1;
        for (int col = rect.x + 1; col <= rect.x + rect.w - 2; col++) {
            if (tui_cell_rect_contains(ctx->clip, col, bottom_y)) {
                mvwadd_wch(ctx->win, bottom_y, col, chars.hline);
            }
        }
    }

    /* Left side (line segment between corners) */
    if (sides & TUI_SIDE_LEFT) {
        for (int row = rect.y + 1; row <= rect.y + rect.h - 2; row++) {
            if (tui_cell_rect_contains(ctx->clip, rect.x, row)) {
                mvwadd_wch(ctx->win, row, rect.x, chars.vline);
            }
        }
    }

    /* Right side (line segment between corners) */
    if (sides & TUI_SIDE_RIGHT) {
        int right_x = rect.x + rect.w - 1;
        for (int row = rect.y + 1; row <= rect.y + rect.h - 2; row++) {
            if (tui_cell_rect_contains(ctx->clip, right_x, row)) {
                mvwadd_wch(ctx->win, row, right_x, chars.vline);
            }
        }
    }
}

/* ============================================================================
 * Text Drawing (DRAW-03, DRAW-04)
 * ============================================================================
 *
 * Positioned text with UTF-8 support via mbstowcs() and column-accurate
 * clipping via wcwidth(). Wide characters (CJK, 2-column) that straddle
 * clip boundaries are skipped entirely to prevent display corruption.
 *
 * Uses a stack-allocated wchar_t buffer (256 elements) for typical strings,
 * with malloc fallback for longer text. Rendering uses mvwaddnwstr() which
 * takes an element count (not column count).
 */

void tui_draw_text(TUI_DrawContext* ctx, int x, int y,
                    const char* text, TUI_Style style) {
    if (text == NULL) return;

    /* Clip vertically: row must be within clip region */
    if (y < ctx->clip.y || y >= ctx->clip.y + ctx->clip.h) return;

    /* UTF-8 to wchar_t conversion */
    wchar_t wbuf_stack[256];
    wchar_t* wbuf = wbuf_stack;
    int wlen;

    size_t needed = mbstowcs(NULL, text, 0);
    if (needed == (size_t)-1) return;  /* Invalid UTF-8 */

    if (needed >= 256) {
        wbuf = malloc((needed + 1) * sizeof(wchar_t));
        if (wbuf == NULL) return;
    }

    wlen = (int)mbstowcs(wbuf, text, needed + 1);
    if (wlen <= 0) {
        if (wbuf != wbuf_stack) free(wbuf);
        return;
    }

    /* Column-accurate horizontal clipping */
    int clip_left = ctx->clip.x;
    int clip_right = ctx->clip.x + ctx->clip.w;

    int col = x;           /* Current column position */
    int start_idx = -1;    /* First wchar_t index in visible range */
    int end_idx = wlen;    /* One past last wchar_t index in visible range */
    int draw_x = x;        /* Column where rendering actually starts */

    for (int i = 0; i < wlen; i++) {
        int cw = wcwidth(wbuf[i]);
        if (cw < 0) cw = 0;  /* Non-printable: treat as zero-width */

        if (start_idx == -1) {
            /* Haven't found the start of visible region yet */
            if (col + cw > clip_left) {
                /* This character starts or extends into visible region */
                if (col < clip_left) {
                    /* Wide char straddles left clip boundary -- skip it */
                    col += cw;
                    continue;
                }
                start_idx = i;
                draw_x = col;
            }
        }

        if (col >= clip_right) {
            end_idx = i;
            break;
        }

        /* Check if this character extends past the right clip boundary */
        if (col + cw > clip_right && start_idx != -1) {
            /* Wide char straddles right clip boundary -- exclude it */
            end_idx = i;
            break;
        }

        col += cw;
    }

    /* If no visible characters found, nothing to draw */
    if (start_idx == -1) {
        if (wbuf != wbuf_stack) free(wbuf);
        return;
    }

    /* Render the visible slice */
    tui_style_apply(ctx->win, style);
    mvwaddnwstr(ctx->win, y, draw_x, &wbuf[start_idx], end_idx - start_idx);

    if (wbuf != wbuf_stack) free(wbuf);
}

void tui_draw_text_bounded(TUI_DrawContext* ctx, int x, int y,
                            const char* text, int max_cols,
                            TUI_Style style) {
    if (max_cols <= 0) return;

    /* Create a bounding rect for the text area */
    TUI_CellRect text_bounds = { .x = x, .y = y, .w = max_cols, .h = 1 };

    /* Temporarily narrow the clip to the intersection of current clip and text bounds */
    TUI_CellRect saved_clip = ctx->clip;
    ctx->clip = tui_cell_rect_intersect(ctx->clip, text_bounds);

    /* Delegate to tui_draw_text which will clip against the narrowed region */
    tui_draw_text(ctx, x, y, text, style);

    /* Restore original clip */
    ctx->clip = saved_clip;
}
