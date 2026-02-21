# Phase 8: Sub-Cell Rendering - Research

**Researched:** 2026-02-21
**Domain:** Unicode block/braille characters, ncurses wide-char rendering, shadow buffer compositing
**Confidence:** HIGH

## Summary

Sub-cell rendering uses Unicode block element characters (half-blocks, quadrants) and braille patterns to achieve pixel resolutions finer than one terminal character cell. The three modes provide 1x2 (half-block), 2x2 (quadrant), and 2x4 (braille) virtual pixels per cell. All three modes share the same rendering primitive: construct a `cchar_t` via `setcchar()` and write it via `mvwadd_wch()`, exactly as the existing `tui_draw.c` already does for rounded corners and box-drawing characters.

The critical architectural challenge is the shadow buffer -- a per-layer in-memory grid that tracks sub-cell state so that drawing to one part of a cell preserves the other parts. This is needed because ncurses cells are atomic: writing a character replaces the entire cell. The shadow buffer must track dot states (braille), half colors (half-block), and quadrant states independently. The buffer resets when `werase()` runs at frame begin, which the existing frame pipeline already handles for dirty layers.

**Primary recommendation:** Implement a unified `TUI_SubCellBuffer` with one entry per cell that stores the current sub-cell mode and mode-specific state. Allocate it alongside each layer, reset it in the frame pipeline's dirty-layer clearing loop, and use it as the source of truth for all sub-cell rendering.

## Standard Stack

No additional libraries are needed. Everything is built on existing project infrastructure.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses (wide) | 6.x | `setcchar()` + `mvwadd_wch()` for Unicode output | Already in use, provides all needed wide-char APIs |
| Unicode 3.2+ | N/A | Block Elements (U+2580-U+259F) and Braille Patterns (U+2800-U+28FF) | Universal terminal support since Unicode 3.2 (2002) |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<wchar.h>` | C99 | `wchar_t` type for character construction | Already included in tui_draw.c |
| `<stdlib.h>` | C99 | `malloc`/`free` for shadow buffer allocation | Already included in tui_draw.c |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Shadow buffer | `mvin_wch()` to read back from ncurses window | Reading back is unreliable (may read wrong attrs), slow, and decision explicitly forbids it |
| Per-cell struct | Separate arrays per mode | Wastes memory, complicates mode-mixing logic |

**Installation:**
No new dependencies. Everything is already linked.

## Architecture Patterns

### Recommended Project Structure
```
src/
  graphics/
    tui_draw.c           # Existing -- add sub-cell draw functions here
    tui_subcell.c         # NEW -- shadow buffer management + internal helpers
include/
  cels-ncurses/
    tui_draw.h            # Existing -- add sub-cell function declarations
    tui_subcell.h         # NEW -- TUI_SubCellBuffer type + management API
```

### Pattern 1: Shadow Buffer Per Layer

**What:** Each layer gets a `TUI_SubCellBuffer*` that tracks sub-cell state for every cell position. The buffer is allocated when the layer is created and freed when destroyed. It is cleared during `tui_frame_begin()` alongside `werase()`.

**When to use:** Always -- this is the core infrastructure for all three sub-cell modes.

**Design:**
```c
// Sub-cell mode tag -- each cell is in exactly one mode at a time
typedef enum TUI_SubCellMode {
    TUI_SUBCELL_NONE,       // Cell has no sub-cell content (normal character)
    TUI_SUBCELL_HALFBLOCK,  // Half-block mode: top/bottom colors
    TUI_SUBCELL_QUADRANT,   // Quadrant mode: 4 quadrants, 2-color constraint
    TUI_SUBCELL_BRAILLE     // Braille mode: 8 dots on/off
} TUI_SubCellMode;

// Per-cell state -- union discriminated by mode
typedef struct TUI_SubCell {
    TUI_SubCellMode mode;
    union {
        struct {
            TUI_Color top;      // Foreground color (upper half)
            TUI_Color bottom;   // Background color (lower half)
            bool has_top;       // Whether top half has been drawn
            bool has_bottom;    // Whether bottom half has been drawn
        } halfblock;
        struct {
            uint8_t mask;       // Bitmask: bit0=UL, bit1=UR, bit2=LL, bit3=LR
            TUI_Color fg;       // Foreground color (filled quadrants)
            TUI_Color bg;       // Background color (empty quadrants)
        } quadrant;
        struct {
            uint8_t dots;       // Bitmask: 8 bits for 8 dot positions
            TUI_Color fg;       // Dot color (foreground)
        } braille;
    };
} TUI_SubCell;

// Buffer wrapping the grid
typedef struct TUI_SubCellBuffer {
    TUI_SubCell* cells;     // width * height array
    int width, height;      // Matches layer dimensions (in terminal cells)
} TUI_SubCellBuffer;
```

### Pattern 2: Pixel-to-Cell Coordinate Conversion

**What:** All public API functions accept virtual pixel coordinates. Internally, the library divides by the mode's resolution factor to get the cell position and sub-cell offset.

**When to use:** Every sub-cell draw function.

**Conversion formulas:**
```c
// Half-block: 1x2 virtual pixels per cell
//   pixel_x maps 1:1 to cell_x
//   pixel_y / 2 = cell_y
//   pixel_y % 2 = sub_position (0=top, 1=bottom)

// Quadrant: 2x2 virtual pixels per cell
//   pixel_x / 2 = cell_x
//   pixel_y / 2 = cell_y
//   (pixel_x % 2, pixel_y % 2) = quadrant position

// Braille: 2x4 virtual pixels per cell
//   pixel_x / 2 = cell_x
//   pixel_y / 4 = cell_y
//   (pixel_x % 2, pixel_y % 4) = dot position -> bit index
```

### Pattern 3: Write-Through to ncurses

**What:** After updating the shadow buffer for any sub-cell operation, immediately construct the appropriate `cchar_t` from the buffer state and write it to the ncurses window via `mvwadd_wch()`. The shadow buffer is the source of truth; ncurses is just the output.

**When to use:** Every sub-cell draw call.

**Example (braille):**
```c
// After updating dots in shadow buffer:
TUI_SubCell* cell = &buf->cells[cell_y * buf->width + cell_x];
wchar_t wc[2] = { 0x2800 + cell->braille.dots, L'\0' };
cchar_t cc;
setcchar(&cc, wc, A_NORMAL, 0, NULL);
// Apply style for braille foreground color + default/black bg
tui_style_apply(ctx->win, braille_style);
mvwadd_wch(ctx->win, cell_y, cell_x, &cc);
```

### Pattern 4: Mode Mixing -- Last Mode Wins

**What:** When a sub-cell draw targets a cell that is already in a different mode, the old mode state is discarded entirely. The cell switches to the new mode.

**When to use:** Always enforced automatically by the shadow buffer update logic.

```c
// In every sub-cell write:
if (cell->mode != desired_mode) {
    memset(cell, 0, sizeof(TUI_SubCell));
    cell->mode = desired_mode;
}
// Then update mode-specific state...
```

### Anti-Patterns to Avoid
- **Reading back from ncurses window:** `mvin_wch()` returns what ncurses last wrote, but attribute/color information may be incomplete or unreliable across terminals. Always use the shadow buffer as source of truth.
- **Global shadow buffer:** Each layer has its own coordinate space. The buffer must be per-layer, not global.
- **Clearing sub-cells separately from werase:** The frame pipeline already clears dirty layers. Adding a separate sub-cell clear path creates ordering bugs. Reset the buffer in the same loop.
- **Allocating the buffer on the DrawContext:** DrawContext is stack-allocated and temporary. The buffer must live on the TUI_Layer (persistent across frames until werase).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Braille codepoint computation | Character lookup table | `0x2800 + dots_bitmask` | Direct bit-to-codepoint mapping is by Unicode design |
| Quadrant codepoint computation | Complex if/else chain | 16-entry lookup table indexed by 4-bit mask | All 16 combinations map to known codepoints (see table below) |
| Color pair management | Manual pair tracking | `alloc_pair()` already in `tui_style_apply()` | Phase 7 infrastructure handles this transparently |
| Wide character output | Manual terminal escape codes | `setcchar()` + `mvwadd_wch()` | Existing pattern from tui_draw.c rounded corners |
| Nearest color fallback | Custom distance calculation | `tui_color_rgb()` already does mode-aware resolution | Phase 7 handles 256-color fallback automatically |

**Key insight:** The codepoint computation for all three modes is trivial arithmetic or a small lookup table. The hard part is the shadow buffer compositing, which must be built because no library provides it.

## Common Pitfalls

### Pitfall 1: Half-Block Color Pair Direction
**What goes wrong:** Confusing which color is foreground and which is background for half-block characters.
**Why it happens:** Upper half block (U+2580) uses foreground for the TOP half and background for the BOTTOM half. Lower half block (U+2584) uses foreground for the BOTTOM half and background for the TOP half.
**How to avoid:** Always use U+2584 (lower half block) as the canonical character. Then: fg = bottom color, bg = top color. This is the standard convention because "bottom fills up" is the natural mental model. When only the top half is drawn, use U+2580 with fg = top color, bg = default/transparent.
**Warning signs:** Colors appear swapped between top and bottom halves.

### Pitfall 2: Quadrant Two-Color Constraint
**What goes wrong:** Caller draws quadrants with 3+ different colors in the same cell, expecting all to render.
**Why it happens:** Each terminal cell has exactly one fg and one bg color. Quadrant characters use fg for "filled" quadrants and bg for "empty" quadrants. You cannot have more than 2 colors.
**How to avoid:** The API tracks which two colors occupy each cell. When a new draw introduces a third color, the shadow buffer must handle it. Decision: the new color replaces one of the existing colors (last-write-wins for the specific quadrant being drawn, other quadrants keep their state but may shift between fg/bg).
**Warning signs:** Unexpected color changes in quadrants that weren't drawn this frame.

### Pitfall 3: Braille Dot Bit Ordering
**What goes wrong:** Dots appear in wrong positions because bit-to-dot mapping is assumed to be left-to-right, top-to-bottom.
**Why it happens:** Unicode braille bit ordering is NOT simple row-major. The mapping is:
```
Bit 0 -> dot 1 (row 0, col 0)    Bit 3 -> dot 4 (row 0, col 1)
Bit 1 -> dot 2 (row 1, col 0)    Bit 4 -> dot 5 (row 1, col 1)
Bit 2 -> dot 3 (row 2, col 0)    Bit 5 -> dot 6 (row 2, col 1)
Bit 6 -> dot 7 (row 3, col 0)    Bit 7 -> dot 8 (row 3, col 1)
```
**How to avoid:** Use a lookup table that converts (sub_x, sub_y) -> bit index:
```c
static const uint8_t BRAILLE_DOT_BIT[2][4] = {
    /* col 0 */  { 0x01, 0x02, 0x04, 0x40 },  /* bits 0,1,2,6 */
    /* col 1 */  { 0x08, 0x10, 0x20, 0x80 },  /* bits 3,4,5,7 */
};
// Usage: dots |= BRAILLE_DOT_BIT[sub_x][sub_y];
```
**Warning signs:** Dots appear mirrored, column-swapped, or in wrong rows.

### Pitfall 4: Shadow Buffer Size Mismatch After Resize
**What goes wrong:** Drawing sub-cells after `tui_layer_resize()` writes out of bounds or reads stale data.
**Why it happens:** The shadow buffer dimensions must match the layer's window dimensions. Resize changes the window but not the buffer.
**How to avoid:** Hook into `tui_layer_resize()` to reallocate the shadow buffer. Or lazily check dimensions on each draw call.
**Warning signs:** Segfault or visual corruption after terminal resize.

### Pitfall 5: Scissor Clipping in Pixel vs Cell Coordinates
**What goes wrong:** Sub-cell pixels near scissor boundaries are drawn when they should be clipped, or entire cells are clipped when only part should be.
**Why it happens:** The scissor system works in cell coordinates. Sub-cell pixels must be converted to cell coordinates for clipping, and the clipping granularity is at the cell level (not sub-pixel).
**How to avoid:** Convert pixel coordinates to cell coordinates, then check `tui_cell_rect_contains(ctx->clip, cell_x, cell_y)` before any cell write. Clipping is per-cell, not per-sub-pixel. This means a partially-clipped cell still renders all its sub-pixels (the entire cell is either in or out).
**Warning signs:** Sub-cell content bleeds past scissor boundaries.

### Pitfall 6: setcchar Color Pair vs wattr_set
**What goes wrong:** Colors don't appear on the braille/block character.
**Why it happens:** `setcchar()` takes a color pair parameter, but the existing codebase uses `tui_style_apply()` (which calls `wattr_set()`) to set colors BEFORE calling `mvwadd_wch()`. If `setcchar()` is called with pair=0, the character inherits the window's current attributes.
**How to avoid:** Follow the existing pattern: call `tui_style_apply(ctx->win, style)` first, then call `mvwadd_wch()`. Pass 0 for the pair parameter in `setcchar()`. This is exactly what `tui_draw.c` already does for rounded corners (see `init_rounded_corners()`).
**Warning signs:** Characters render but in wrong colors or default colors.

## Code Examples

### Half-Block Rendering (Writing One Cell)

```c
// Source: Derived from existing tui_draw.c setcchar pattern + Unicode spec
// Render a half-block cell where top and bottom may have different colors
static void subcell_write_halfblock(TUI_DrawContext* ctx,
                                     TUI_SubCellBuffer* buf,
                                     int cell_x, int cell_y) {
    TUI_SubCell* cell = &buf->cells[cell_y * buf->width + cell_x];
    if (cell->mode != TUI_SUBCELL_HALFBLOCK) return;

    wchar_t wc[2] = { 0, L'\0' };
    TUI_Style style = { .fg = TUI_COLOR_DEFAULT, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_NORMAL };

    if (cell->halfblock.has_top && cell->halfblock.has_bottom) {
        // Both halves drawn: use LOWER HALF BLOCK (U+2584)
        // fg = bottom color, bg = top color
        wc[0] = 0x2584;
        style.fg = cell->halfblock.bottom;
        style.bg = cell->halfblock.top;
    } else if (cell->halfblock.has_top) {
        // Only top: use UPPER HALF BLOCK (U+2580)
        // fg = top color, bg = default (transparent)
        wc[0] = 0x2580;
        style.fg = cell->halfblock.top;
        style.bg = TUI_COLOR_DEFAULT;
    } else if (cell->halfblock.has_bottom) {
        // Only bottom: use LOWER HALF BLOCK (U+2584)
        // fg = bottom color, bg = default (transparent)
        wc[0] = 0x2584;
        style.fg = cell->halfblock.bottom;
        style.bg = TUI_COLOR_DEFAULT;
    } else {
        return; // Nothing to draw
    }

    cchar_t cc;
    setcchar(&cc, wc, A_NORMAL, 0, NULL);
    tui_style_apply(ctx->win, style);
    mvwadd_wch(ctx->win, cell_y, cell_x, &cc);
}
```

### Quadrant Lookup Table (All 16 Combinations)

```c
// Source: Unicode Block Elements spec (U+2580-U+259F)
// Complete mapping from 4-bit quadrant mask to Unicode codepoint
// Bit layout: bit0=UL, bit1=UR, bit2=LL, bit3=LR
static const wchar_t QUADRANT_CHARS[16] = {
    L' ',    // 0b0000 = empty (space)
    0x2598,  // 0b0001 = UL
    0x259D,  // 0b0010 = UR
    0x2580,  // 0b0011 = UL+UR (upper half block)
    0x2596,  // 0b0100 = LL
    0x258C,  // 0b0101 = UL+LL (left half block)
    0x259E,  // 0b0110 = UR+LL
    0x259B,  // 0b0111 = UL+UR+LL
    0x2597,  // 0b1000 = LR
    0x259A,  // 0b1001 = UL+LR
    0x2590,  // 0b1010 = UR+LR (right half block)
    0x259C,  // 0b1011 = UL+UR+LR
    0x2584,  // 0b1100 = LL+LR (lower half block)
    0x2599,  // 0b1101 = UL+LL+LR
    0x259F,  // 0b1110 = UR+LL+LR
    0x2588,  // 0b1111 = all (full block)
};

// Convert sub-pixel (sx, sy) within cell to quadrant bit
// sx: 0=left, 1=right; sy: 0=upper, 1=lower
static inline uint8_t quadrant_bit(int sx, int sy) {
    // bit0=UL(0,0), bit1=UR(1,0), bit2=LL(0,1), bit3=LR(1,1)
    return (uint8_t)(1 << (sy * 2 + sx));
}
```

### Braille Dot-to-Bit Mapping

```c
// Source: Unicode Braille Patterns spec (U+2800-U+28FF)
// Braille cell layout:     Bit mapping:
//   dot1  dot4             bit0  bit3
//   dot2  dot5             bit1  bit4
//   dot3  dot6             bit2  bit5
//   dot7  dot8             bit6  bit7
//
// NOTE: bits 6,7 are NOT sequential with bits 0-5!
// The 6-dot braille uses bits 0-5, 8-dot adds bits 6-7.

static const uint8_t BRAILLE_DOT_BIT[2][4] = {
    /* col 0 (left)  */ { 0x01, 0x02, 0x04, 0x40 },  /* bits 0, 1, 2, 6 */
    /* col 1 (right) */ { 0x08, 0x10, 0x20, 0x80 },  /* bits 3, 4, 5, 7 */
};

// Plot a single braille dot
static void braille_set_dot(TUI_SubCell* cell, int sub_x, int sub_y) {
    cell->braille.dots |= BRAILLE_DOT_BIT[sub_x][sub_y];
}

// Unplot a single braille dot
static void braille_clear_dot(TUI_SubCell* cell, int sub_x, int sub_y) {
    cell->braille.dots &= ~BRAILLE_DOT_BIT[sub_x][sub_y];
}

// Convert dots bitmask to Unicode codepoint
static inline wchar_t braille_codepoint(uint8_t dots) {
    return 0x2800 + dots;
}
```

### Shadow Buffer Lifecycle

```c
// Allocation (called from tui_layer_create or lazily on first sub-cell draw)
TUI_SubCellBuffer* tui_subcell_buffer_create(int width, int height) {
    TUI_SubCellBuffer* buf = malloc(sizeof(TUI_SubCellBuffer));
    if (!buf) return NULL;
    buf->width = width;
    buf->height = height;
    buf->cells = calloc((size_t)(width * height), sizeof(TUI_SubCell));
    if (!buf->cells) { free(buf); return NULL; }
    return buf;
}

// Reset (called from tui_frame_begin alongside werase)
void tui_subcell_buffer_clear(TUI_SubCellBuffer* buf) {
    if (!buf || !buf->cells) return;
    memset(buf->cells, 0, (size_t)(buf->width * buf->height) * sizeof(TUI_SubCell));
}

// Resize (called from tui_layer_resize)
void tui_subcell_buffer_resize(TUI_SubCellBuffer* buf, int width, int height) {
    if (!buf) return;
    TUI_SubCell* new_cells = calloc((size_t)(width * height), sizeof(TUI_SubCell));
    if (!new_cells) return;
    free(buf->cells);
    buf->cells = new_cells;
    buf->width = width;
    buf->height = height;
}

// Destruction (called from tui_layer_destroy)
void tui_subcell_buffer_destroy(TUI_SubCellBuffer* buf) {
    if (!buf) return;
    free(buf->cells);
    free(buf);
}
```

### Filled Rect in Half-Block Mode (Public API Example)

```c
// Pattern: follows tui_draw_fill_rect structure
void tui_draw_halfblock_fill_rect(TUI_DrawContext* ctx,
                                   int px, int py, int pw, int ph,
                                   TUI_Style style) {
    // px, py are in pixel coordinates (1x2 resolution)
    // pw, ph are pixel dimensions
    TUI_SubCellBuffer* buf = /* get from layer */;
    if (!buf) return;

    for (int pixel_y = py; pixel_y < py + ph; pixel_y++) {
        for (int pixel_x = px; pixel_x < px + pw; pixel_x++) {
            int cell_x = pixel_x;       // 1:1 horizontal
            int cell_y = pixel_y / 2;   // 2x vertical
            int sub_y = pixel_y % 2;    // 0=top, 1=bottom

            // Scissor clip at cell level
            if (!tui_cell_rect_contains(ctx->clip, cell_x, cell_y)) continue;

            TUI_SubCell* cell = &buf->cells[cell_y * buf->width + cell_x];

            // Mode mixing: last mode wins
            if (cell->mode != TUI_SUBCELL_HALFBLOCK) {
                memset(cell, 0, sizeof(TUI_SubCell));
                cell->mode = TUI_SUBCELL_HALFBLOCK;
            }

            if (sub_y == 0) {
                cell->halfblock.top = style.fg;
                cell->halfblock.has_top = true;
            } else {
                cell->halfblock.bottom = style.fg;
                cell->halfblock.has_bottom = true;
            }

            // Write through to ncurses
            subcell_write_halfblock(ctx, buf, cell_x, cell_y);
        }
    }
}
```

## Unicode Reference Tables

### Half-Block Characters
| Codepoint | Character | Name | Usage |
|-----------|-----------|------|-------|
| U+2580 | Upper Half Block | Top half filled | fg=top color when only top drawn |
| U+2584 | Lower Half Block | Bottom half filled | Primary: fg=bottom, bg=top |
| U+2588 | Full Block | Entire cell filled | When top==bottom color |

### Quadrant Characters (Complete 16-Entry Table)
| Mask | Bits (UL,UR,LL,LR) | Codepoint | Character | Notes |
|------|---------------------|-----------|-----------|-------|
| 0x0 | 0000 | U+0020 | (space) | Empty cell |
| 0x1 | 0001 | U+2598 | Quadrant UL | |
| 0x2 | 0010 | U+259D | Quadrant UR | |
| 0x3 | 0011 | U+2580 | Upper Half Block | From half-block range |
| 0x4 | 0100 | U+2596 | Quadrant LL | |
| 0x5 | 0101 | U+258C | Left Half Block | From half-block range |
| 0x6 | 0110 | U+259E | Quadrant UR+LL | |
| 0x7 | 0111 | U+259B | Quadrant UL+UR+LL | |
| 0x8 | 1000 | U+2597 | Quadrant LR | |
| 0x9 | 1001 | U+259A | Quadrant UL+LR | |
| 0xA | 1010 | U+2590 | Right Half Block | From half-block range |
| 0xB | 1011 | U+259C | Quadrant UL+UR+LR | |
| 0xC | 1100 | U+2584 | Lower Half Block | From half-block range |
| 0xD | 1101 | U+2599 | Quadrant UL+LL+LR | |
| 0xE | 1110 | U+259F | Quadrant UR+LL+LR | |
| 0xF | 1111 | U+2588 | Full Block | From block range |

Note: 6 of the 16 combinations use characters from outside the U+2596-U+259F quadrant range (space, upper/lower/left/right half blocks, full block). The lookup table unifies them all.

### Braille Dot Bit Mapping
| Grid Position | Dot Number | Bit Index | Bit Value |
|---------------|------------|-----------|-----------|
| (0,0) top-left | 1 | 0 | 0x01 |
| (0,1) mid-left | 2 | 1 | 0x02 |
| (0,2) low-left | 3 | 2 | 0x04 |
| (1,0) top-right | 4 | 3 | 0x08 |
| (1,1) mid-right | 5 | 4 | 0x10 |
| (1,2) low-right | 6 | 5 | 0x20 |
| (0,3) bot-left | 7 | 6 | 0x40 |
| (1,3) bot-right | 8 | 7 | 0x80 |

Codepoint formula: `U+2800 + dots_byte` (direct binary encoding by Unicode design).

## Shadow Buffer Integration Points

### Where the Buffer Lives

The `TUI_SubCellBuffer*` must be added to `TUI_Layer`:

```c
typedef struct TUI_Layer {
    /* existing fields... */
    TUI_SubCellBuffer* subcell_buf;  // NEW -- NULL until first sub-cell draw
} TUI_Layer;
```

**Lazy allocation rationale:** Most layers will never use sub-cell rendering (UI panels, text layers). Allocating on first use avoids wasting memory. The buffer pointer starts NULL and is allocated on the first sub-cell draw call.

### Integration with Frame Pipeline

In `tui_frame_begin()`, the dirty-layer clearing loop must also clear the sub-cell buffer:

```c
for (int i = 0; i < g_layer_count; i++) {
    if (g_layers[i].dirty && g_layers[i].visible) {
        werase(g_layers[i].win);
        wattr_set(g_layers[i].win, A_NORMAL, 0, NULL);
        if (g_layers[i].subcell_buf) {
            tui_subcell_buffer_clear(g_layers[i].subcell_buf);
        }
        g_layers[i].dirty = false;
    }
}
```

### Integration with Layer Resize

In `tui_layer_resize()`, if the buffer exists, resize it:

```c
void tui_layer_resize(TUI_Layer* layer, int w, int h) {
    /* existing resize logic... */
    if (layer->subcell_buf) {
        tui_subcell_buffer_resize(layer->subcell_buf, w, h);
    }
}
```

### Integration with Layer Destroy

In `tui_layer_destroy()`, free the buffer:

```c
void tui_layer_destroy(TUI_Layer* layer) {
    /* existing cleanup... */
    if (layer->subcell_buf) {
        tui_subcell_buffer_destroy(layer->subcell_buf);
        layer->subcell_buf = NULL;
    }
}
```

### Getting the Buffer from DrawContext

The sub-cell draw functions receive `TUI_DrawContext*`, which has a `WINDOW*` but no direct reference to the layer or buffer. Options:

1. **Add buffer pointer to DrawContext** -- simplest, follows existing pattern of ctx carrying everything needed
2. **Lookup by window pointer** -- iterate g_layers to find matching WINDOW*, fragile
3. **Require caller to pass buffer** -- breaks API consistency

**Recommendation:** Add `TUI_SubCellBuffer**` (pointer-to-pointer for lazy alloc) to `TUI_DrawContext`. This requires modifying `tui_draw_context_create()` and `tui_layer_get_draw_context()`:

```c
typedef struct TUI_DrawContext {
    WINDOW* win;
    int x, y;
    int width, height;
    TUI_CellRect clip;
    TUI_SubCellBuffer** subcell_buf;  // NEW -- points to layer->subcell_buf, NULL for non-layer contexts
} TUI_DrawContext;
```

The double pointer allows lazy allocation: the sub-cell draw function can check `*ctx->subcell_buf == NULL` and allocate if needed.

## Proposed Public API

Based on decisions from CONTEXT.md:

```c
// === Sub-cell mode enum (for resolution query) ===
typedef enum TUI_SubCellMode {
    TUI_SUBCELL_HALFBLOCK,
    TUI_SUBCELL_QUADRANT,
    TUI_SUBCELL_BRAILLE
} TUI_SubCellMode;

// === Resolution query ===
void tui_draw_subcell_resolution(TUI_DrawContext* ctx, TUI_SubCellMode mode,
                                  int* width, int* height);
// Returns virtual pixel dimensions for the given mode:
//   HALFBLOCK: width = ctx->width,     height = ctx->height * 2
//   QUADRANT:  width = ctx->width * 2, height = ctx->height * 2
//   BRAILLE:   width = ctx->width * 2, height = ctx->height * 4

// === Half-block primitives ===
void tui_draw_halfblock_plot(TUI_DrawContext* ctx, int px, int py, TUI_Style style);
void tui_draw_halfblock_fill_rect(TUI_DrawContext* ctx, int px, int py,
                                   int pw, int ph, TUI_Style style);

// === Quadrant primitives ===
void tui_draw_quadrant_plot(TUI_DrawContext* ctx, int px, int py, TUI_Style style);
void tui_draw_quadrant_fill_rect(TUI_DrawContext* ctx, int px, int py,
                                  int pw, int ph, TUI_Style style);

// === Braille primitives ===
void tui_draw_braille_plot(TUI_DrawContext* ctx, int px, int py, TUI_Style style);
void tui_draw_braille_unplot(TUI_DrawContext* ctx, int px, int py);
void tui_draw_braille_fill_rect(TUI_DrawContext* ctx, int px, int py,
                                 int pw, int ph, TUI_Style style);
```

**Color convention:** `style.fg` is the pixel color. For half-block, the "other half" preserves its existing color (or defaults to transparent/default if not yet drawn). For quadrant, fg is the filled quadrant color. For braille, fg is the dot color.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Terminal pixel graphics via sixel | Unicode block/braille characters | Ongoing since Unicode 3.2 | Block chars work everywhere; sixel requires specific terminal support |
| Read-back via mvin_wch for compositing | Shadow buffer (application-side tracking) | Standard practice | Eliminates terminal read-back unreliability |
| 6-dot braille (U+2800-U+283F) | 8-dot braille (U+2800-U+28FF) | Unicode 3.0 (1999) | 2x4 resolution per cell instead of 2x3 |

**Terminal compatibility note:** Half-block and braille characters are universally supported in modern terminals (kitty, alacritty, foot, wezterm, xterm, gnome-terminal, Windows Terminal). Quadrant characters (U+2596-U+259F) have slightly less consistent rendering in some older terminals but are supported in all terminals the project targets.

## Open Questions

1. **DrawContext modification scope**
   - What we know: Adding `TUI_SubCellBuffer**` to DrawContext is cleanest for API consistency
   - What's unclear: Whether this breaks any existing code that constructs DrawContext directly (not via `tui_layer_get_draw_context`)
   - Recommendation: Check all DrawContext creation sites. The `draw_test.c` example uses `tui_layer_get_draw_context()` for layer contexts and `tui_draw_context_create()` for non-layer contexts. Non-layer contexts should pass NULL for the subcell_buf field.

2. **Quadrant two-color enforcement strategy**
   - What we know: Each cell can have at most 2 colors (fg + bg). Decision says "caller is aware of the limit."
   - What's unclear: Exact behavior when a third color is introduced. Options: (a) new draw replaces the cell's fg, existing quadrants mapped to the two current colors; (b) entire cell resets to just the new quadrant.
   - Recommendation: Use approach (a) -- track two colors per cell. When a new color arrives that matches neither, re-assign fg to the new color and remap existing quadrants. Quadrants that were the old fg but are now the bg keep their position but visually change to bg color. This is the simplest approach that preserves spatial state.

3. **Half-block "transparent" background**
   - What we know: Drawing only the top half should preserve whatever was below. Decision says "one color per plot/rect call + transparent other half."
   - What's unclear: How "transparent" interacts with the background fill that `tui_draw_fill_rect` typically handles.
   - Recommendation: Use `TUI_COLOR_DEFAULT` for the undrawn half. This makes the undrawn half inherit the terminal/layer background. If both halves are drawn, both colors are explicit.

## Sources

### Primary (HIGH confidence)
- Unicode Block Elements chart (U+2580-U+259F) -- verified complete quadrant mapping via symbl.cc
- Unicode Braille Patterns (U+2800-U+28FF) -- verified 8-dot bit mapping via grokipedia.com
- Existing codebase: `tui_draw.c` `setcchar`/`mvwadd_wch` pattern for rounded corners
- Existing codebase: `tui_layer.c` layer lifecycle and `tui_frame.c` dirty clearing

### Secondary (MEDIUM confidence)
- [braille-framebuffer](https://github.com/edw/braille-framebuffer) -- validates shadow buffer approach for braille rendering
- [drawille](https://github.com/asciimoo/drawille) -- validates pixel-coordinate to braille dot mapping
- [ncurses add_wch documentation](https://invisible-island.net/ncurses/man/curs_add_wch.3x.html) -- confirms mvwadd_wch behavior

### Tertiary (LOW confidence)
- Terminal compatibility claims for quadrant characters (based on general knowledge, not systematically tested)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, all ncurses APIs already in use
- Unicode mappings: HIGH -- verified against Unicode charts and reference implementations
- Shadow buffer design: HIGH -- follows established pattern from braille-framebuffer and drawille libraries, informed by existing codebase architecture
- Half-block color handling: HIGH -- directly follows from Unicode character semantics (fg/bg)
- Quadrant two-color strategy: MEDIUM -- the spatial behavior when introducing a third color has multiple valid approaches
- API surface: HIGH -- constrained by CONTEXT.md decisions, follows existing draw function patterns
- Frame pipeline integration: HIGH -- modification points are clearly identified in existing code

**Research date:** 2026-02-21
**Valid until:** 2026-03-21 (stable domain, Unicode is not changing)
