# Phase 2: Drawing Primitives - Research

**Researched:** 2026-02-07
**Domain:** ncurses drawing functions, Unicode box-drawing characters, scissor/clipping, wide-character text rendering
**Confidence:** HIGH

## Summary

This phase implements the full set of drawing primitives needed by a Clay renderer: filled rectangles, outlined rectangles/borders with box-drawing characters (single, double, rounded), positioned text with style, bounded/clipped text, horizontal/vertical lines, and a scissor (clip) stack. All drawing targets the `WINDOW*` inside `TUI_DrawContext` via ncurses write-only calls (`mvwaddch`, `mvwadd_wch`, `mvwaddnwstr`, `mvwhline_set`, `mvwvline_set`). No refresh calls occur inside drawing functions (FRAM-04 constraint).

The critical complexities are: (1) ncurses `mv*` functions return ERR for out-of-bounds coordinates, so all drawing functions must clip against `TUI_DrawContext.clip` BEFORE calling ncurses -- ncurses provides no automatic clipping; (2) wide-character text clipping is non-trivial because `mvwaddnwstr` counts wchar_t elements not display columns, and CJK characters occupy 2 columns; (3) rounded corners (U+256D-U+2570) have no WACS_ macros in ncurses, requiring manual `cchar_t` construction via `setcchar()`; (4) the scissor stack is a separate module-level array that updates `TUI_DrawContext.clip` on push/pop via `tui_cell_rect_intersect()`.

The Phase 1 foundation provides everything needed: `TUI_DrawContext` (with embedded `clip` field), `TUI_CellRect` (with `tui_cell_rect_intersect`/`tui_cell_rect_contains`), `TUI_Style` (with `tui_style_apply`), and `TUI_Color` (with `tui_color_rgb`). Drawing functions compose these types directly.

**Primary recommendation:** Implement all drawing as cell-by-cell loops with per-cell clip checks using `tui_cell_rect_contains()`, except for text which uses a column-counting wchar_t slice approach. Use the WACS_ macros for single and double box-drawing, and `setcchar()` for rounded corners. Build the scissor stack as a fixed-size static array with max depth 16.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncursesw | 6.5 | All drawing: mvwaddch, mvwadd_wch, mvwhline_set, mvwvline_set, mvwaddnwstr | Already linked, provides wide-char box-drawing macros (WACS_*) |
| C99 stdlib | -- | wchar.h (wchar_t, wcwidth, mbstowcs, mbrtowc), stdlib.h (mbtowc) | UTF-8 text measurement and conversion |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| wchar.h | C99/POSIX | wcwidth() for column width, mbstowcs() for UTF-8 to wchar_t | Text measurement (DRAW-03, DRAW-04) |
| string.h | C99 | memset, memcpy | Buffer initialization |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Cell-by-cell clip loop | ncurses subwindow clipping | Subwindows allocate heap memory and add lifecycle management; cell-by-cell is simpler and allocation-free |
| mvwadd_wch for rounded corners | Direct mvwaddstr with UTF-8 bytes | mvwadd_wch respects ncurses attribute system correctly; raw UTF-8 strings bypass cchar_t rendering attributes |
| WACS_ macros for single/double | Manual setcchar for all styles | WACS_ macros are portable and handle terminal capability detection automatically |

**Installation:**
No additional packages needed. ncursesw 6.5 is already installed and linked.

## Architecture Patterns

### Recommended Project Structure (new files for Phase 2)
```
include/cels-ncurses/
    tui_draw.h           # All drawing function declarations
src/
    graphics/
        tui_draw.c       # Rectangle, text, border, line implementations
        tui_scissor.c    # Scissor stack management (push/pop/reset)
```

### Pattern 1: Clip-Then-Draw
**What:** Every drawing function clips its target rectangle against `ctx->clip` BEFORE issuing any ncurses calls. ncurses `mv*` functions return ERR for out-of-bounds coordinates but do not clip partial content -- they just fail. Our code must handle partial visibility.
**When to use:** Every draw function without exception.
**Example:**
```c
/* Source: ncurses curs_addch(3X) -- "mv functions fail if position is outside window boundaries" */

void tui_draw_fill_rect(TUI_DrawContext* ctx, TUI_CellRect rect, chtype ch, TUI_Style style) {
    /* Clip the draw rect against the context's current clip region */
    TUI_CellRect visible = tui_cell_rect_intersect(rect, ctx->clip);
    if (visible.w <= 0 || visible.h <= 0) return;  /* Fully clipped */

    tui_style_apply(ctx->win, style);
    for (int row = visible.y; row < visible.y + visible.h; row++) {
        for (int col = visible.x; col < visible.x + visible.w; col++) {
            mvwaddch(ctx->win, row, col, ch);
        }
    }
}
```

### Pattern 2: Border Style Table
**What:** A struct holding pointers to the 6 cchar_t values (hline, vline, 4 corners) for each border style (single, double, rounded). Drawing code indexes into the table rather than branching per-style.
**When to use:** Border and outlined rectangle rendering (DRAW-02, DRAW-05, DRAW-06, DRAW-07).
**Example:**
```c
/* Source: ncurses curs_add_wch(3X) -- WACS_HLINE, WACS_D_HLINE, etc. */

typedef struct TUI_BorderChars {
    const cchar_t* hline;     /* Horizontal line */
    const cchar_t* vline;     /* Vertical line */
    const cchar_t* ul;        /* Upper-left corner */
    const cchar_t* ur;        /* Upper-right corner */
    const cchar_t* ll;        /* Lower-left corner */
    const cchar_t* lr;        /* Lower-right corner */
} TUI_BorderChars;

/* Single: WACS_HLINE, WACS_VLINE, WACS_ULCORNER, etc. */
/* Double: WACS_D_HLINE, WACS_D_VLINE, WACS_D_ULCORNER, etc. */
/* Rounded: custom cchar_t for U+256D, U+256E, U+256F, U+2570 + WACS_HLINE/WACS_VLINE */
```

### Pattern 3: Column-Counting Text Slice
**What:** For bounded/clipped text, convert UTF-8 to wchar_t array, then walk the array counting display columns via wcwidth(). Compute a (start_index, count) slice of the wchar_t array that fits within the visible column range. Pass that slice to mvwaddnwstr().
**When to use:** Text rendering with clipping (DRAW-03, DRAW-04).
**Example:**
```c
/* Source: wcwidth(3) man page, ncurses curs_addwstr(3X) */

/* Given: UTF-8 text, clip region, text position */
/* Step 1: mbstowcs to convert UTF-8 -> wchar_t[] */
/* Step 2: Walk wchar_t[], accumulating column widths via wcwidth() */
/* Step 3: Find first wchar_t whose cumulative column >= clip_left offset */
/* Step 4: Find last wchar_t whose cumulative column < clip_left + clip_width */
/* Step 5: mvwaddnwstr(win, y, adjusted_x, &wbuf[start], count) */
```

### Pattern 4: Scissor Stack as Static Array
**What:** A fixed-size static array of TUI_CellRect with an integer stack pointer. Push intersects new rect with current top and increments pointer. Pop decrements pointer. The effective clip is always `stack[sp]`. TUI_DrawContext.clip is updated after each push/pop.
**When to use:** DRAW-08, DRAW-09, DRAW-10 -- scissor push/pop operations.
**Example:**
```c
#define TUI_SCISSOR_STACK_MAX 16

static TUI_CellRect scissor_stack[TUI_SCISSOR_STACK_MAX];
static int scissor_sp = 0;  /* Points to current top */

void tui_push_scissor(TUI_DrawContext* ctx, TUI_CellRect rect) {
    if (scissor_sp >= TUI_SCISSOR_STACK_MAX - 1) return;  /* Stack full */
    TUI_CellRect clipped = tui_cell_rect_intersect(rect, scissor_stack[scissor_sp]);
    scissor_sp++;
    scissor_stack[scissor_sp] = clipped;
    ctx->clip = clipped;
}

void tui_pop_scissor(TUI_DrawContext* ctx) {
    if (scissor_sp > 0) scissor_sp--;
    ctx->clip = scissor_stack[scissor_sp];
}
```

### Pattern 5: No Refresh Inside Draw Functions (FRAM-04)
**What:** Drawing primitives ONLY write to the WINDOW* buffer. They never call wrefresh(), wnoutrefresh(), or doupdate(). The frame pipeline in Phase 4 handles all screen updates.
**When to use:** Every draw function -- this is a hard constraint.
**Example:**
```c
/* CORRECT: write to buffer only */
mvwaddch(ctx->win, row, col, ch);

/* WRONG: never do this in a draw function */
wrefresh(ctx->win);     /* NO */
wnoutrefresh(ctx->win); /* NO */
doupdate();              /* NO */
```

### Anti-Patterns to Avoid
- **Using ncurses wborder/box for bordered rectangles:** These draw to the entire window edge, not to an arbitrary rect within a window. Use manual corner + hline + vline drawing instead.
- **mvinch to sample background color:** The upstream PR does this to make text transparent over backgrounds. Our layer system (Phase 3) eliminates this need -- each layer is a separate WINDOW with its own background.
- **Calling mvwaddch for out-of-bounds coordinates:** ncurses returns ERR but may have side effects on cursor position. Always clip before calling.
- **Using `n` parameter of mvwaddnwstr as column count:** The `n` parameter counts wchar_t ELEMENTS, not display columns. A CJK character is 1 element but 2 columns. Column counting must be done separately with wcwidth().
- **Heap allocation per draw call:** No malloc inside draw functions. Use stack-allocated buffers with reasonable maximums (e.g., 256 wchar_t for text).
- **Using ACS_ macros instead of WACS_ macros:** ACS_ macros use the legacy non-wide character type (chtype). WACS_ macros use cchar_t and are the correct choice for wide-character ncurses. Since we build with ncursesw, use WACS_ exclusively.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Horizontal line drawing | Per-cell mvwaddch loop for hlines | `mvwhline_set(win, y, x, wch, n)` | Built-in, handles edge cases, respects attributes |
| Vertical line drawing | Per-cell mvwaddch loop for vlines | `mvwvline_set(win, y, x, wch, n)` | Built-in, handles edge cases |
| Single-line box corners | setcchar for U+250C/U+2510/etc. | `WACS_ULCORNER` / `WACS_URCORNER` / etc. | Portable, handles terminal capability detection |
| Double-line box chars | setcchar for U+2550/U+2551/etc. | `WACS_D_HLINE` / `WACS_D_VLINE` / `WACS_D_ULCORNER` / etc. | Available in ncurses 6.5, handles fallback |
| Column width of wide char | Lookup table or heuristic | `wcwidth(wc)` from wchar.h | POSIX standard, locale-aware, handles CJK |
| UTF-8 to wchar_t conversion | Byte-by-byte parsing | `mbstowcs()` or `mbrtowc()` loop | Standard C, handles all UTF-8 edge cases |
| Rectangle intersection | Custom min/max math | `tui_cell_rect_intersect()` from Phase 1 | Already implemented and tested in tui_types.h |
| Point-in-rect test | Manual bounds check | `tui_cell_rect_contains()` from Phase 1 | Already implemented in tui_types.h |
| Style application | attron/attroff per draw call | `tui_style_apply(win, style)` from Phase 1 | Atomic via wattr_set, no state leaks |

**Key insight:** The Phase 1 foundation already provides the geometry utilities (intersection, containment), color pair management (alloc_pair via tui_style_apply), and style application that drawing functions compose. Drawing code should be thin wrappers: clip, apply style, issue ncurses calls.

## Common Pitfalls

### Pitfall 1: ncurses mv* Functions Fail Silently on Out-of-Bounds
**What goes wrong:** `mvwaddch(win, -1, 5, 'x')` returns ERR and does nothing. `mvwaddch(win, 0, 80, 'x')` on an 80-col window returns ERR. There is no partial drawing -- if the coordinate is outside the window, the entire call fails.
**Why it happens:** ncurses treats the window as a strict grid. mv* functions move the cursor first; if the move fails, the draw is skipped entirely.
**How to avoid:** All drawing functions must intersect their target rectangle with `ctx->clip` before issuing any ncurses calls. Only iterate over cells that are within both the draw rect AND the clip rect.
**Warning signs:** Characters not appearing at edges of the screen; rectangles missing their last row or column.

### Pitfall 2: mvwaddnwstr n Parameter Counts Elements, Not Columns
**What goes wrong:** Passing `n = 10` to `mvwaddnwstr` renders 10 wchar_t characters, which could be 10-20 display columns (CJK chars are 2 columns each). Text overflows its bounding box.
**Why it happens:** The man page says "writes at most n wide characters" -- this means wchar_t count, not column count.
**How to avoid:** Walk the wchar_t array with `wcwidth()` to count columns. Compute the wchar_t index range that fits within the target column width. Pass that count to mvwaddnwstr.
**Warning signs:** CJK or emoji text overflows its bounding box by roughly 2x.

### Pitfall 3: Wide Characters Split at Clip Boundary
**What goes wrong:** A 2-column CJK character positioned so that column 0 is visible but column 1 is clipped. Drawing it writes both columns (ncurses writes the full character), potentially overwriting content in the clipped region.
**Why it happens:** A wide character occupies 2 terminal cells. You cannot draw half of it.
**How to avoid:** When a wide character straddles the clip boundary (left or right edge), skip it entirely and replace with a space if needed. Track column offsets carefully during the wchar_t walk.
**Warning signs:** Characters appearing outside clip regions; garbled display at clip boundaries with CJK text.

### Pitfall 4: Rounded Corners Have No WACS_ Macros
**What goes wrong:** Code tries to use `WACS_ULCORNER` for rounded corners and gets sharp corners (U+250C instead of U+256D).
**Why it happens:** ncurses defines WACS_ macros for single-line (sharp), double-line, and thick-line box-drawing characters, but NOT for the rounded/arc variants (U+256D-U+2570).
**How to avoid:** Create custom `cchar_t` values using `setcchar()` for the 4 rounded corner characters:
- U+256D (top-left arc)
- U+256E (top-right arc)
- U+256F (bottom-right arc)
- U+2570 (bottom-left arc)
Use `mvwadd_wch()` to draw them. These combine with WACS_HLINE/WACS_VLINE for the straight segments.
**Warning signs:** All borders appear with sharp corners regardless of corner_radius setting.

### Pitfall 5: Scissor Stack Overflow
**What goes wrong:** Deeply nested scroll containers push more scissors than the stack can hold. Silent stack overflow corrupts memory or clips incorrectly.
**Why it happens:** Fixed-size stack with no overflow protection.
**How to avoid:** Check `scissor_sp < TUI_SCISSOR_STACK_MAX - 1` before push. Return early or assert on overflow. 16 levels is sufficient for practical UI hierarchies (the upstream PR uses 16). Log a warning if stack is full.
**Warning signs:** Rendering artifacts in deeply nested layouts.

### Pitfall 6: Forgetting to Reset Scissor Stack Between Frames
**What goes wrong:** If a frame's render pass is interrupted (early return, error), the scissor stack retains stale entries from the previous partial frame. Next frame starts with incorrect clipping.
**Why it happens:** Scissor push/pop must be perfectly balanced. Errors break the balance.
**How to avoid:** Provide a `tui_scissor_reset(ctx)` function that resets `scissor_sp = 0` and restores `ctx->clip` to the full drawable area. Call it at the start of each frame in the frame pipeline (Phase 4).
**Warning signs:** Clipping that persists across frames; drawing confined to a small region after an error.

### Pitfall 7: Per-Side Border Drawing Without Corner Handling
**What goes wrong:** Drawing only the top and left borders of a rectangle leaves the top-left corner with incorrect character (hline or vline, but not a corner piece).
**Why it happens:** Corners are the intersection of two border sides. If both adjacent sides are drawn, a corner character is needed. If only one side is drawn, the corner should be omitted or use a line character.
**How to avoid:** Corner placement logic: draw a corner character only if BOTH adjacent sides are enabled. If only one adjacent side is enabled, extend that side's line character into the corner position. If neither adjacent side is enabled, leave the corner empty.
**Warning signs:** Stray corner pieces appearing when only one border side is drawn; missing line segments at border edges.

### Pitfall 8: Static Globals in INTERFACE Library
**What goes wrong:** The scissor stack uses static variables (`scissor_stack[]`, `scissor_sp`). Since this is an INTERFACE CMake library, each translation unit gets its own copy.
**Why it happens:** Source files compile in the consumer's context. `static` means file-local scope.
**How to avoid:** Place the scissor stack in a `.c` file (not a header). Expose only `extern` function declarations in the header. Same pattern as `tui_color.c` from Phase 1.
**Warning signs:** Scissor stack appears empty in one source file but has entries in another.

## Code Examples

Verified patterns from official sources:

### Filled Rectangle Drawing
```c
/* Source: ncurses curs_addch(3X), verified pattern */

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
```

### Outlined Rectangle with Box-Drawing Characters
```c
/* Source: ncurses curs_add_wch(3X), curs_border_set(3X) */

/* Draw border using WACS_ macros (single-line example) */
/* Top edge: hline from (x+1, y) to (x+w-2, y) */
/* Bottom edge: hline from (x+1, y+h-1) to (x+w-2, y+h-1) */
/* Left edge: vline from (x, y+1) to (x, y+h-2) */
/* Right edge: vline from (x+w-1, y+1) to (x+w-1, y+h-2) */
/* Corners: ul=(x,y), ur=(x+w-1,y), ll=(x,y+h-1), lr=(x+w-1,y+h-1) */

/* Each component is individually clipped against ctx->clip */
/* Corners drawn with mvwadd_wch(ctx->win, y, x, corner_cchar) */
/* Lines drawn with per-cell loop (not mvwhline_set, because we need clip control) */
```

### Creating Rounded Corner cchar_t
```c
/* Source: ncurses curs_getcchar(3X) -- setcchar() */
/* Unicode: U+256D, U+256E, U+256F, U+2570 are "light arc" box-drawing */

static cchar_t round_ul, round_ur, round_ll, round_lr;
static bool round_corners_initialized = false;

static void init_rounded_corners(void) {
    if (round_corners_initialized) return;
    wchar_t wc[2] = { 0, L'\0' };

    wc[0] = 0x256D;  /* BOX DRAWINGS LIGHT ARC DOWN AND RIGHT */
    setcchar(&round_ul, wc, A_NORMAL, 0, NULL);

    wc[0] = 0x256E;  /* BOX DRAWINGS LIGHT ARC DOWN AND LEFT */
    setcchar(&round_ur, wc, A_NORMAL, 0, NULL);

    wc[0] = 0x256F;  /* BOX DRAWINGS LIGHT ARC UP AND LEFT */
    setcchar(&round_lr, wc, A_NORMAL, 0, NULL);

    wc[0] = 0x2570;  /* BOX DRAWINGS LIGHT ARC UP AND RIGHT */
    setcchar(&round_ll, wc, A_NORMAL, 0, NULL);

    round_corners_initialized = true;
}
```

### Text Rendering with Column-Accurate Clipping
```c
/* Source: wcwidth(3), ncurses curs_addwstr(3X) */

void tui_draw_text(TUI_DrawContext* ctx, int x, int y,
                    const char* text, TUI_Style style) {
    if (y < ctx->clip.y || y >= ctx->clip.y + ctx->clip.h) return;  /* Row not visible */

    /* Convert UTF-8 to wchar_t (stack buffer for typical strings) */
    wchar_t wbuf[256];
    int wlen = (int)mbstowcs(wbuf, text, 255);
    if (wlen <= 0) return;
    wbuf[wlen] = L'\0';

    /* Compute total column width */
    int total_cols = 0;
    for (int i = 0; i < wlen; i++) {
        int cw = wcwidth(wbuf[i]);
        if (cw > 0) total_cols += cw;
    }

    /* Clip horizontally: find visible column range */
    int clip_left = ctx->clip.x;
    int clip_right = ctx->clip.x + ctx->clip.w;
    int text_left = x;
    int text_right = x + total_cols;

    int vis_left = (text_left > clip_left) ? text_left : clip_left;
    int vis_right = (text_right < clip_right) ? text_right : clip_right;
    if (vis_left >= vis_right) return;  /* Fully clipped */

    /* Walk wchar_t array to find the slice that maps to visible columns */
    int col = x;
    int start_idx = -1, end_idx = wlen;
    int draw_x = vis_left;

    for (int i = 0; i < wlen; i++) {
        int cw = wcwidth(wbuf[i]);
        if (cw < 0) cw = 0;

        /* Check if this character starts within visible range */
        if (col + cw > vis_left && col < vis_right) {
            if (start_idx == -1) {
                start_idx = i;
                draw_x = col;  /* Actual column where rendering starts */
            }
        }
        if (col >= vis_right) {
            end_idx = i;
            break;
        }
        col += cw;
    }

    if (start_idx == -1) return;

    tui_style_apply(ctx->win, style);
    mvwaddnwstr(ctx->win, y, draw_x, &wbuf[start_idx], end_idx - start_idx);
}
```

### Bounded Text (DRAW-04: max column width)
```c
/* Source: Clay text bounding requirement */

void tui_draw_text_bounded(TUI_DrawContext* ctx, int x, int y,
                            const char* text, int max_cols, TUI_Style style) {
    /* Measure and truncate to max_cols before passing to tui_draw_text */
    /* Or: create a temporary clip that is the intersection of ctx->clip
       and (x, y, max_cols, 1), then call tui_draw_text within that clip */
    TUI_CellRect text_bounds = { .x = x, .y = y, .w = max_cols, .h = 1 };
    TUI_CellRect saved_clip = ctx->clip;
    ctx->clip = tui_cell_rect_intersect(ctx->clip, text_bounds);
    tui_draw_text(ctx, x, y, text, style);
    ctx->clip = saved_clip;
}
```

### Scissor Stack Operations
```c
/* Source: Architecture decision -- static array with intersection */

#define TUI_SCISSOR_STACK_MAX 16

/* In tui_scissor.c */
static TUI_CellRect scissor_stack[TUI_SCISSOR_STACK_MAX];
static int scissor_sp = 0;

void tui_scissor_reset(TUI_DrawContext* ctx) {
    scissor_stack[0] = (TUI_CellRect){ ctx->x, ctx->y, ctx->width, ctx->height };
    scissor_sp = 0;
    ctx->clip = scissor_stack[0];
}

void tui_push_scissor(TUI_DrawContext* ctx, TUI_CellRect rect) {
    if (scissor_sp >= TUI_SCISSOR_STACK_MAX - 1) return;
    TUI_CellRect clipped = tui_cell_rect_intersect(rect, scissor_stack[scissor_sp]);
    scissor_sp++;
    scissor_stack[scissor_sp] = clipped;
    ctx->clip = clipped;
}

void tui_pop_scissor(TUI_DrawContext* ctx) {
    if (scissor_sp > 0) scissor_sp--;
    ctx->clip = scissor_stack[scissor_sp];
}
```

### Drawing a Horizontal Line with Clipping
```c
/* Source: ncurses curs_border_set(3X) -- mvwhline_set */

void tui_draw_hline(TUI_DrawContext* ctx, int x, int y, int length,
                     const cchar_t* ch, TUI_Style style) {
    /* Clip the line against ctx->clip */
    if (y < ctx->clip.y || y >= ctx->clip.y + ctx->clip.h) return;

    int left = (x > ctx->clip.x) ? x : ctx->clip.x;
    int right = (x + length < ctx->clip.x + ctx->clip.w)
                ? x + length : ctx->clip.x + ctx->clip.w;
    int visible_len = right - left;
    if (visible_len <= 0) return;

    tui_style_apply(ctx->win, style);
    mvwhline_set(ctx->win, y, left, ch, visible_len);
}
```

## Box-Drawing Character Reference

Complete character map for the three supported border styles:

### Single Line (WACS_ macros)
| Part | WACS Macro | Unicode | Glyph |
|------|-----------|---------|-------|
| Horizontal | WACS_HLINE | U+2500 | --- |
| Vertical | WACS_VLINE | U+2502 | \| |
| Upper-left | WACS_ULCORNER | U+250C | corner |
| Upper-right | WACS_URCORNER | U+2510 | corner |
| Lower-left | WACS_LLCORNER | U+2514 | corner |
| Lower-right | WACS_LRCORNER | U+2518 | corner |

### Double Line (WACS_D_ macros)
| Part | WACS Macro | Unicode | Glyph |
|------|-----------|---------|-------|
| Horizontal | WACS_D_HLINE | U+2550 | double h |
| Vertical | WACS_D_VLINE | U+2551 | double v |
| Upper-left | WACS_D_ULCORNER | U+2554 | double corner |
| Upper-right | WACS_D_URCORNER | U+2557 | double corner |
| Lower-left | WACS_D_LLCORNER | U+255A | double corner |
| Lower-right | WACS_D_LRCORNER | U+255D | double corner |

### Rounded Corners (manual cchar_t via setcchar)
| Part | Unicode | Codepoint | Note |
|------|---------|-----------|------|
| Upper-left arc | U+256D | 0x256D | No WACS_ macro; use setcchar() |
| Upper-right arc | U+256E | 0x256E | No WACS_ macro; use setcchar() |
| Lower-right arc | U+256F | 0x256F | No WACS_ macro; use setcchar() |
| Lower-left arc | U+2570 | 0x2570 | No WACS_ macro; use setcchar() |
| Horizontal | WACS_HLINE | U+2500 | Same as single-line |
| Vertical | WACS_VLINE | U+2502 | Same as single-line |

Rounded style uses single-line horizontal/vertical with arc corners. There are no rounded variants for double-line style (this matches Clay's behavior -- cornerRadius is only meaningful for single-line borders).

## Drawing Function Inventory

Complete list of functions this phase should implement:

| Function | Requirement | Parameters | Notes |
|----------|-------------|------------|-------|
| `tui_draw_fill_rect` | DRAW-01 | ctx, rect, fill_char, style | Filled rectangle |
| `tui_draw_border_rect` | DRAW-02, DRAW-06, DRAW-07 | ctx, rect, border_style, style | Outlined rectangle with configurable corners |
| `tui_draw_text` | DRAW-03 | ctx, x, y, text, style | Positioned text with UTF-8 support |
| `tui_draw_text_bounded` | DRAW-04 | ctx, x, y, text, max_cols, style | Text clamped to max column width |
| `tui_draw_border` | DRAW-05 | ctx, rect, sides, border_style, style | Per-side border control |
| `tui_draw_hline` | DRAW-11 | ctx, x, y, length, border_style, style | Horizontal line |
| `tui_draw_vline` | DRAW-11 | ctx, x, y, length, border_style, style | Vertical line |
| `tui_push_scissor` | DRAW-08 | ctx, rect | Push clip region |
| `tui_pop_scissor` | DRAW-09 | ctx | Pop clip region |
| `tui_scissor_reset` | (internal) | ctx | Reset stack for new frame |

## Border Style Enum

```c
typedef enum TUI_BorderStyle {
    TUI_BORDER_SINGLE,    /* WACS_HLINE / WACS_VLINE / WACS_*CORNER */
    TUI_BORDER_DOUBLE,    /* WACS_D_HLINE / WACS_D_VLINE / WACS_D_*CORNER */
    TUI_BORDER_ROUNDED,   /* WACS_HLINE / WACS_VLINE / custom arc corners */
} TUI_BorderStyle;
```

## Border Sides Bitmask

```c
#define TUI_SIDE_TOP     0x01
#define TUI_SIDE_RIGHT   0x02
#define TUI_SIDE_BOTTOM  0x04
#define TUI_SIDE_LEFT    0x08
#define TUI_SIDE_ALL     (TUI_SIDE_TOP | TUI_SIDE_RIGHT | TUI_SIDE_BOTTOM | TUI_SIDE_LEFT)
```

Corner logic: a corner character is placed only when BOTH adjacent sides are enabled. For example, the upper-left corner is drawn only when both TUI_SIDE_TOP and TUI_SIDE_LEFT are set. If only one adjacent side is enabled, extend that side's line into the corner position.

## State of the Art

| Old Approach (current code) | Current Approach (Phase 2) | When Changed | Impact |
|------|------|------|------|
| `mvprintw(row, 0, "+----+")` hardcoded ASCII borders | `mvwadd_wch(win, y, x, WACS_ULCORNER)` + `mvwhline_set` Unicode box-drawing | ncursesw since ncurses 5.4 | Proper Unicode box chars, theme-consistent |
| `ACS_HLINE` (chtype, non-wide) | `WACS_HLINE` (cchar_t, wide) | ncursesw wide-char support | Full Unicode support, double/rounded styles |
| No clipping -- draw to stdscr | Clip against `TUI_DrawContext.clip` before every draw | Design decision | Enables scissor/scroll containers |
| No border style variants | Single, double, rounded via `TUI_BorderStyle` enum | Design decision | Matches Clay border configuration |
| No text bounding | `tui_draw_text_bounded` with column-accurate clipping | Design decision | Clay text bounding support |
| `printw("%s", text)` | `mvwaddnwstr(win, y, x, wbuf, n)` column-aware | Design decision | Correct CJK/emoji support |

## Open Questions

Things that couldn't be fully resolved:

1. **Stack-allocated vs heap-allocated wchar_t buffer for text**
   - What we know: A stack buffer of 256 wchar_t covers most UI text. Longer strings (rare in TUI) would need dynamic allocation.
   - What's unclear: Whether to hard-limit at 256 or support arbitrary length with a fallback malloc.
   - Recommendation: Use a stack buffer of 256 wchar_t. If mbstowcs indicates the string is longer, fall back to malloc. This keeps the common case allocation-free while supporting edge cases. Most TUI text elements are under 100 characters.

2. **Per-corner radius vs boolean for rounded corners**
   - What we know: DRAW-07 says "radius > 0 uses rounded Unicode box-drawing corners" -- this is boolean per-corner. Clay provides `Clay_CornerRadius` with float per-corner.
   - What's unclear: Whether we need a per-corner boolean array or a simpler approach.
   - Recommendation: The drawing function takes `TUI_BorderStyle` (single/double/rounded) which applies to ALL corners uniformly. Per-corner control is handled at a higher level -- when the cels-clay bridge translates Clay_CornerRadius, it checks each corner: radius > 0 uses the rounded cchar_t, radius == 0 uses the standard corner. This per-corner override can be a separate function or parameter.

3. **Interaction between border sides and corner radius**
   - What we know: If only top and left borders are drawn, the top-left corner should be drawn (both adjacent sides present) but the other three corners should not.
   - What's unclear: What happens if top border is drawn with rounded style but left border is not -- should the top edge extend into the corner position with a horizontal line character?
   - Recommendation: Yes. When one adjacent side is missing, extend the present side's line character into the corner cell. This produces visually correct open-ended borders.

## Sources

### Primary (HIGH confidence)
- ncurses 6.5 man pages on target system:
  - curs_addch(3X) -- mvwaddch behavior, ACS_ macros, out-of-bounds return ERR
  - curs_add_wch(3X) -- mvwadd_wch, WACS_ macros (single, double, thick), setcchar
  - curs_border_set(3X) -- wborder_set, mvwhline_set, mvwvline_set signatures
  - curs_addwstr(3X) -- mvwaddnwstr, n counts wchar_t elements not columns
  - curs_getcchar(3X) -- setcchar for custom cchar_t creation
- ncurses.h on target system -- WACS_D_*, WACS_T_* macro definitions verified
- wcwidth(3) man page -- column width measurement, POSIX standard
- mbstowcs(3) man page -- UTF-8 to wchar_t conversion
- mbrtowc(3) man page -- thread-safe alternative to mbtowc

### Secondary (MEDIUM confidence)
- [Clay UI library clay.h](https://github.com/nicbarker/clay/blob/main/clay.h) -- Clay_RenderCommandType enum, Clay_BorderRenderData, Clay_CornerRadius, Clay_BorderWidth structs
- [Clay ncurses renderer PR #569](https://github.com/nicbarker/clay/pull/569) -- reference implementation patterns for scissor stack, text clipping, border drawing
- Unicode Standard Box Drawing block (U+2500-U+257F) -- rounded corner characters U+256D-U+2570

### Tertiary (LOW confidence)
- None -- all claims verified with primary or secondary sources

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all ncurses functions verified on target system man pages and headers
- Architecture: HIGH -- patterns derived from verified ncurses API semantics and Phase 1 foundation
- Pitfalls: HIGH -- sourced from man page return value documentation and upstream PR anti-patterns
- Box-drawing chars: HIGH -- WACS_D_ macros verified in /usr/include/ncurses.h; rounded corners verified as missing from WACS_ and available via Unicode/setcchar
- Text clipping: HIGH -- wcwidth, mbstowcs, mvwaddnwstr semantics verified via man pages

**Research date:** 2026-02-07
**Valid until:** 2026-05-07 (stable domain -- ncurses APIs and Unicode box-drawing characters do not change)
