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
 * CELS NCurses - Drawing / Library API
 *
 * Single include for library authors drawing directly with primitives:
 *
 *   #include <cels_ncurses_draw.h>
 *
 * Provides:
 *   - Coordinate types and geometry utilities
 *   - Color/style system
 *   - Sub-cell rendering (half-block, quadrant, braille)
 *   - Draw context
 *   - Drawing primitives (rects, text, borders, lines)
 *   - Scissor/clipping regions
 *   - Layer management
 *   - Frame pipeline
 */

#ifndef CELS_NCURSES_DRAW_H
#define CELS_NCURSES_DRAW_H

#include <math.h>
#include <ncurses.h>
#include <panel.h>
#include <stdbool.h>
#include <stdint.h>

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

/* ============================================================================
 * Color Mode
 * ============================================================================
 *
 * Detected at startup, determines how tui_color_rgb() resolves RGB values.
 * Can be overridden via TUI_Window.color_mode or Engine_Config.color_mode.
 */

typedef enum TUI_ColorMode {
    TUI_COLOR_MODE_256,        /* Default: nearest xterm-256 mapping */
    TUI_COLOR_MODE_PALETTE,    /* can_change_color: redefine palette slots 16-255 */
    TUI_COLOR_MODE_DIRECT      /* RGB flag: packed 0xRRGGBB values */
} TUI_ColorMode;

/* ============================================================================
 * Color Type
 * ============================================================================
 *
 * Wraps a color index. The meaning of index depends on the active color mode:
 *   -1            = terminal default (all modes)
 *   0-255         = xterm-256 color index (256-color mode)
 *   16-255        = palette slot redefined to exact RGB (palette mode)
 *   0x000000-0xFFFFFF = packed RGB value (direct color mode)
 *
 * Created from RGB via tui_color_rgb() which resolves based on detected mode.
 * Use TUI_COLOR_DEFAULT for the terminal's default foreground or background.
 */

typedef struct TUI_Color {
    int index;  /* -1 = terminal default, interpretation depends on color mode */
} TUI_Color;

#define TUI_COLOR_DEFAULT ((TUI_Color){ .index = -1 })

/* ============================================================================
 * Attribute Flags
 * ============================================================================
 *
 * Bitflags for text attributes. Combine with bitwise OR in TUI_Style.attrs.
 * Converted to ncurses attr_t internally by tui_style_apply().
 */

#define TUI_ATTR_NORMAL    0x00
#define TUI_ATTR_BOLD      0x01
#define TUI_ATTR_DIM       0x02
#define TUI_ATTR_UNDERLINE 0x04
#define TUI_ATTR_REVERSE   0x08
#define TUI_ATTR_ITALIC    0x10

/* ============================================================================
 * Style Type
 * ============================================================================
 *
 * Combines foreground color, background color, and attribute flags into a
 * single value. Stack-allocated, passed by value to tui_style_apply().
 */

typedef struct TUI_Style {
    TUI_Color fg;
    TUI_Color bg;
    uint32_t attrs;
} TUI_Style;

/* ============================================================================
 * Color Functions
 * ============================================================================ */

/* Create a color from RGB values (0-255 per channel).
 * Resolves based on detected color mode:
 *   Direct:  packed 0xRRGGBB index
 *   Palette: allocates palette slot, redefines to exact RGB
 *   256:     nearest xterm-256 color index (fallback) */
extern TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Apply a style atomically to an ncurses WINDOW.
 * Uses alloc_pair for color pair resolution and wattr_set with opts
 * pointer for extended pair support. Never uses attron/attroff. */
extern void tui_style_apply(WINDOW* win, TUI_Style style);

/* Initialize color system. Call once after start_color().
 * Detects terminal color capabilities and sets the resolution mode.
 * Pass -1 for auto-detection, or a TUI_ColorMode value to force a mode. */
extern void tui_color_init(int mode_override);

/* Query the active color mode (returns TUI_ColorMode value). */
extern TUI_ColorMode tui_color_get_mode(void);

/* Human-readable name for a color mode ("256", "palette", "direct"). */
extern const char* tui_color_mode_name(TUI_ColorMode mode);

/* ============================================================================
 * Sub-Cell Mode
 * ============================================================================
 *
 * Each cell is in exactly one sub-cell mode at a time. NONE means the cell
 * has no sub-cell content (normal character). Mode mixing uses last-mode-wins:
 * drawing braille over a half-block cell replaces it entirely.
 */
typedef enum TUI_SubCellMode {
    TUI_SUBCELL_NONE = 0,   /* Cell has no sub-cell content (normal character) */
    TUI_SUBCELL_HALFBLOCK,  /* Half-block mode: top/bottom colors */
    TUI_SUBCELL_QUADRANT,   /* Quadrant mode: 4 quadrants, 2-color constraint */
    TUI_SUBCELL_BRAILLE     /* Braille mode: 8 dots on/off */
} TUI_SubCellMode;

/* ============================================================================
 * TUI_SubCell -- Per-cell sub-cell state
 * ============================================================================
 *
 * Tagged union discriminated by mode. Each cell stores mode-specific state:
 *   - halfblock: independent top/bottom colors with has_top/has_bottom flags
 *   - quadrant: 4-bit mask + fg/bg colors (2-color constraint per cell)
 *   - braille: 8-bit dot bitmask + fg color
 */
typedef struct TUI_SubCell {
    TUI_SubCellMode mode;
    union {
        struct {
            TUI_Color top;      /* Color for upper half */
            TUI_Color bottom;   /* Color for lower half */
            bool has_top;       /* Whether top half has been drawn */
            bool has_bottom;    /* Whether bottom half has been drawn */
        } halfblock;
        struct {
            uint8_t mask;       /* Bitmask: bit0=UL, bit1=UR, bit2=LL, bit3=LR */
            TUI_Color fg;       /* Foreground color (filled quadrants) */
            TUI_Color bg;       /* Background color (empty quadrants) */
        } quadrant;
        struct {
            uint8_t dots;       /* Bitmask: 8 bits for 8 dot positions */
            TUI_Color fg;       /* Dot color (foreground) */
        } braille;
    };
} TUI_SubCell;

/* ============================================================================
 * TUI_SubCellBuffer -- Per-layer shadow buffer
 * ============================================================================
 *
 * Grid of TUI_SubCell entries, one per terminal cell. Dimensions match the
 * layer's window dimensions. Allocated lazily on first sub-cell draw,
 * cleared at frame_begin alongside werase, resized with the layer.
 */
typedef struct TUI_SubCellBuffer {
    TUI_SubCell* cells;     /* width * height array */
    int width, height;      /* Matches layer dimensions (in terminal cells) */
} TUI_SubCellBuffer;

/* ============================================================================
 * Sub-Cell Buffer Lifecycle API
 * ============================================================================ */

/* Allocate a new sub-cell buffer with given dimensions.
 * All cells are zero-initialized (mode = TUI_SUBCELL_NONE).
 * Returns NULL on allocation failure. */
extern TUI_SubCellBuffer* tui_subcell_buffer_create(int width, int height);

/* Reset all cells to TUI_SUBCELL_NONE (zero-fill).
 * Called from tui_frame_begin alongside werase. NULL-safe. */
extern void tui_subcell_buffer_clear(TUI_SubCellBuffer* buf);

/* Resize buffer to new dimensions, discarding old content.
 * Called from tui_layer_resize. NULL-safe. */
extern void tui_subcell_buffer_resize(TUI_SubCellBuffer* buf, int width, int height);

/* Free buffer memory (cells array + buffer struct).
 * Called from tui_layer_destroy. NULL-safe. */
extern void tui_subcell_buffer_destroy(TUI_SubCellBuffer* buf);

/* ============================================================================
 * Draw Context
 * ============================================================================
 *
 * All draw functions take TUI_DrawContext* as their first parameter.
 * No global "current context" -- caller passes the target explicitly.
 * The clip field represents the current effective clip rect.
 */

typedef struct TUI_DrawContext {
    WINDOW* win;          /* Borrowed -- caller manages lifetime */
    int x, y;             /* Origin offset within the window */
    int width, height;    /* Drawable area dimensions */
    TUI_CellRect clip;    /* Current effective clip rect (set by scissor stack) */
    TUI_SubCellBuffer** subcell_buf; /* Pointer to layer's buffer pointer (NULL for non-layer contexts) */
} TUI_DrawContext;

/*
 * Create a draw context wrapping an ncurses WINDOW.
 * Returns a stack-allocated context with clip initialized to the full
 * drawable area. The WINDOW* is borrowed, not owned.
 */
static inline TUI_DrawContext tui_draw_context_create(WINDOW* win,
                                                      int x, int y,
                                                      int width, int height) {
    TUI_DrawContext ctx;
    ctx.win = win;
    ctx.x = x;
    ctx.y = y;
    ctx.width = width;
    ctx.height = height;
    ctx.clip = (TUI_CellRect){ .x = x, .y = y, .w = width, .h = height };
    ctx.subcell_buf = NULL;
    return ctx;
}

/* ============================================================================
 * Drawing Primitives - Types
 * ============================================================================ */

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
 * Drawing Primitives - Constants
 * ============================================================================ */

#define TUI_SIDE_TOP     0x01
#define TUI_SIDE_RIGHT   0x02
#define TUI_SIDE_BOTTOM  0x04
#define TUI_SIDE_LEFT    0x08
#define TUI_SIDE_ALL     (TUI_SIDE_TOP | TUI_SIDE_RIGHT | TUI_SIDE_BOTTOM | TUI_SIDE_LEFT)

/* ============================================================================
 * Rectangle Drawing
 * ============================================================================ */

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
 * ============================================================================ */

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
 * ============================================================================ */

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
 * ============================================================================ */

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
 * Sub-Cell Drawing -- Half-Block Mode
 * ============================================================================
 *
 * Half-block mode: 1x2 virtual pixels per terminal cell.
 * pixel_x maps 1:1 to cell_x. pixel_y / 2 = cell_y, pixel_y % 2 = sub (0=top, 1=bottom).
 * Uses U+2584 (lower half block) as canonical character: fg=bottom, bg=top.
 * Drawing one half preserves the other half's existing color.
 * Requires layer-backed DrawContext (ctx->subcell_buf != NULL).
 */

/* Plot a single virtual pixel at (px, py) in half-block coordinates. */
extern void tui_draw_halfblock_plot(TUI_DrawContext* ctx, int px, int py,
                                     TUI_Style style);

/* Fill a rectangular region of virtual pixels at half-block resolution. */
extern void tui_draw_halfblock_fill_rect(TUI_DrawContext* ctx,
                                          int px, int py, int pw, int ph,
                                          TUI_Style style);

/* ============================================================================
 * Sub-Cell Drawing -- Quadrant Mode
 * ============================================================================
 *
 * Quadrant mode: 2x2 virtual pixels per terminal cell.
 * pixel_x / 2 = cell_x, pixel_y / 2 = cell_y.
 * (pixel_x % 2, pixel_y % 2) selects one of four quadrants.
 */

/* Plot a single virtual pixel at (px, py) in quadrant coordinates. */
extern void tui_draw_quadrant_plot(TUI_DrawContext* ctx, int px, int py,
                                     TUI_Style style);

/* Fill a rectangular region of virtual pixels at quadrant resolution. */
extern void tui_draw_quadrant_fill_rect(TUI_DrawContext* ctx,
                                          int px, int py, int pw, int ph,
                                          TUI_Style style);

/* ============================================================================
 * Sub-Cell Drawing -- Braille Mode
 * ============================================================================
 *
 * Braille mode: 2x4 virtual pixels per terminal cell.
 * pixel_x / 2 = cell_x, pixel_y / 4 = cell_y.
 */

/* Plot a single virtual pixel (set a dot) at (px, py) in braille coordinates. */
extern void tui_draw_braille_plot(TUI_DrawContext* ctx, int px, int py,
                                    TUI_Style style);

/* Unplot a single virtual pixel (clear a dot) at (px, py) in braille coordinates. */
extern void tui_draw_braille_unplot(TUI_DrawContext* ctx, int px, int py);

/* Fill a rectangular region of virtual pixels at braille resolution. */
extern void tui_draw_braille_fill_rect(TUI_DrawContext* ctx,
                                         int px, int py, int pw, int ph,
                                         TUI_Style style);

/* ============================================================================
 * Sub-Cell Resolution Query
 * ============================================================================ */

/* Get virtual pixel dimensions for the given sub-cell mode.
 * HALFBLOCK: width = ctx->width,     height = ctx->height * 2
 * QUADRANT:  width = ctx->width * 2, height = ctx->height * 2
 * BRAILLE:   width = ctx->width * 2, height = ctx->height * 4
 * NONE:      width = ctx->width,     height = ctx->height (1:1) */
extern void tui_draw_subcell_resolution(TUI_DrawContext* ctx,
                                          TUI_SubCellMode mode,
                                          int* width, int* height);

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/* Return the set of 6 box-drawing characters for the given border style. */
extern TUI_BorderChars tui_border_chars_get(TUI_BorderStyle border_style);

/* ============================================================================
 * Scissor Stack API
 * ============================================================================
 *
 * tui_scissor_reset  -- Clear stack and set base clip to full drawable area.
 * tui_push_scissor   -- Push a new clip rect (intersected with current top).
 * tui_pop_scissor    -- Pop the top clip rect, restoring the previous one.
 */

#define TUI_SCISSOR_STACK_MAX 16

extern void tui_scissor_reset(TUI_DrawContext* ctx);
extern void tui_push_scissor(TUI_DrawContext* ctx, TUI_CellRect rect);
extern void tui_pop_scissor(TUI_DrawContext* ctx);

/* ============================================================================
 * Layer System
 * ============================================================================ */

#define TUI_LAYER_MAX 32

/*
 * Panel-backed layer. Each layer owns a WINDOW* and PANEL* pair.
 */
typedef struct TUI_Layer {
    char name[64];        /* Human-readable identifier */
    PANEL* panel;         /* Owned -- del_panel on destroy */
    WINDOW* win;          /* Owned -- delwin on destroy */
    int x, y;             /* Position (screen coordinates) */
    int width, height;    /* Dimensions */
    bool visible;         /* Visibility state */
    bool dirty;           /* Auto-set by tui_layer_get_draw_context; cleared by tui_frame_begin */
    TUI_SubCellBuffer* subcell_buf; /* NULL until first sub-cell draw (lazy alloc) */
} TUI_Layer;

/* Global layer state (extern -- defined in tui_layer.c) */
extern TUI_Layer g_layers[TUI_LAYER_MAX];
extern int g_layer_count;

/* Lifecycle */
extern TUI_Layer* tui_layer_create(const char* name, int x, int y, int w, int h);
extern void tui_layer_destroy(TUI_Layer* layer);

/* Visibility */
extern void tui_layer_show(TUI_Layer* layer);
extern void tui_layer_hide(TUI_Layer* layer);

/* Z-Order */
extern void tui_layer_raise(TUI_Layer* layer);
extern void tui_layer_lower(TUI_Layer* layer);

/* Position */
extern void tui_layer_move(TUI_Layer* layer, int x, int y);

/* Resize */
extern void tui_layer_resize(TUI_Layer* layer, int w, int h);
extern void tui_layer_resize_all(int new_cols, int new_lines);

/* DrawContext Bridge */
extern TUI_DrawContext tui_layer_get_draw_context(TUI_Layer* layer);

/* ============================================================================
 * Frame Pipeline
 * ============================================================================ */

typedef struct TUI_FrameState {
    uint64_t frame_count;    /* Total frames rendered */
    float delta_time;        /* Seconds since last frame */
    float fps;               /* Current FPS (1.0 / delta_time) */
    bool in_frame;           /* True between frame_begin and frame_end */
} TUI_FrameState;

extern TUI_FrameState g_frame_state;

extern void tui_frame_init(void);
extern void tui_frame_begin(void);
extern void tui_frame_end(void);
extern void tui_frame_invalidate_all(void);
extern TUI_Layer* tui_frame_get_background(void);

#ifdef CELS_HAS_ECS
extern void tui_frame_register_systems(void);
#endif

#endif /* CELS_NCURSES_DRAW_H */
