# Features Research: cels-ncurses Graphics API

**Research Date:** 2026-02-07
**Confidence:** HIGH
**Domain:** Terminal graphics API / ncurses rendering engine
**Sources:** Clay SDL3 renderer, notcurses, termbox2, existing codebase

## Table Stakes (Must Have for Clay-Compatible Terminal Renderer)

### TS-1: Rectangle Drawing (Filled)

**Why expected:** `CLAY_RENDER_COMMAND_TYPE_RECTANGLE` is the most common Clay render command. Every UI element starts as a rectangle with position, size, color, and optional corner radius.

**Terminal mapping:** Fill a rectangular region with a character (default space) and background color. Uses `mvwhline(win, y+row, x, ch, w)` in a loop targeting a specific WINDOW.

**Complexity:** Low
**Dependencies:** TS-4 (Color System), TS-7 (Drawing Surface)

### TS-2: Text Rendering

**Why expected:** `CLAY_RENDER_COMMAND_TYPE_TEXT` renders text strings at specific positions. Clay provides text content, bounding box, font ID, font size, and color.

**Terminal mapping:** Position text at (col, row) using `mvwaddnstr(win, y, x, text, max_len)`. Font size maps to ncurses attributes (bold=heading, dim=secondary). The bounded variant is critical for Clay — text must not overflow its bounding box.

**Complexity:** Medium (wide character handling, text truncation)
**Dependencies:** TS-4 (Color System), TS-7 (Drawing Surface)

### TS-3: Border Drawing

**Why expected:** `CLAY_RENDER_COMMAND_TYPE_BORDER` draws borders around elements with per-side width control. Clay's `Clay_BorderRenderData` has `left`, `right`, `top`, `bottom` widths and corner radius.

**Terminal mapping:** Box-drawing characters — single (`WACS_HLINE`, `WACS_VLINE`, corner chars), double (`WACS_D_*`), rounded (U+256D-U+2570 via `setcchar()`). Per-side control means drawing horizontal or vertical separator lines using T-junction characters.

**Complexity:** Medium (per-side control, corner character selection, interaction with corner radius)
**Dependencies:** TS-4 (Color System), TS-7 (Drawing Surface), TS-6 (Corner Radius)

### TS-4: Color/Attribute System

**Why expected:** Every Clay render command carries color data (`Clay_Color` with r,g,b,a). The system must translate Clay's RGBA colors to terminal color representation.

**Design — color mode hierarchy:**
1. **8-color mode** (baseline): Map Clay colors to nearest of 8 basic colors. Use attributes (BOLD for bright variants) for 16 effective foreground colors.
2. **256-color mode** (standard modern terminals): Map Clay colors to nearest of 256 palette entries using color cube (6x6x6 = 216 colors + 24 grayscale).

**Alpha handling:** Terminal cells are inherently opaque — no per-cell alpha blending.
- `a == 0`: Skip drawing (fully transparent)
- `a < threshold`: Skip drawing
- `a >= threshold`: Draw fully opaque with the RGB color

**Attribute mapping from Clay fontId:**

| fontId | ncurses attributes | Semantic meaning |
|--------|-------------------|-----------------|
| 0 | A_NORMAL | Body text |
| 1 | A_BOLD | Headings / emphasis |
| 2 | A_DIM | Secondary / muted text |
| 3 | A_UNDERLINE | Links / interactive |
| 4 | A_ITALIC | Emphasis variant |

**Complexity:** Medium-High (color quantization, managing ncurses color pair table)
**Dependencies:** None (foundational)

### TS-5: Scissor/Clipping Regions

**Why expected:** `CLAY_RENDER_COMMAND_TYPE_SCISSOR_START` and `CLAY_RENDER_COMMAND_TYPE_SCISSOR_END` are emitted by Clay for scroll containers and overflow: hidden elements.

**Terminal mapping:** Software clipping — before writing any cell, check whether coordinate falls within active clipping rectangle. Implement as a scissor stack (push on START, pop on END).

**Stack implementation:**
```
Scissor stack (max depth 8-16):
  SCISSOR_START: push intersection(current_clip, new_rect)
  SCISSOR_END:   pop, restoring previous clip
  Draw operations: check cell against top-of-stack clip rect
```

**Complexity:** Medium (4 comparisons per cell; complexity in intersecting nested rects)
**Dependencies:** TS-7 (Drawing Surface)

### TS-6: Corner Radius Approximation

**Why expected:** `Clay_CornerRadius` appears on both rectangle and border render data. Every Clay renderer must handle this field.

**Terminal mapping:** Binary — if radius > 0, use Unicode rounded box-drawing corners (U+256D, U+256E, U+2570, U+256F). Per-corner control supported.

**Complexity:** Low (character selection only)
**Dependencies:** TS-3 (Border Drawing)

### TS-7: Drawing Surface / Render Target

**Why expected:** Every graphics API needs "where drawing goes." Currently cels-ncurses draws directly to `stdscr`, which prevents layering and multiple render targets.

**Terminal mapping:** Wraps an ncurses `WINDOW*` with metadata: dimensions, position, clipping state, current attributes, dirty flag.

**Complexity:** Medium (lifecycle management, coordinate transformation)
**Dependencies:** None (foundational)

### TS-8: Layer System with Z-Order

**Why expected:** Clay's `Clay_RenderCommand` has a `zIndex` field. Tooltips, dropdowns, and modals need to render above siblings. Overlapping text in the same buffer is destructive.

**Design:**
```
Layer:
    name     -- string, for debugging
    z_order  -- int, sort key for compositing
    surface  -- Surface, the drawing target
    visible  -- bool, skip during compositing if false
```

**Compositing:** Sort layers by z_order. For each layer (low to high), for each non-empty cell: write to output buffer. Higher layers overwrite lower ones.

**Complexity:** High (compositing loop, z-order management, dirty tracking, memory management)
**Dependencies:** TS-7 (Drawing Surface), TS-4 (Color System)

### TS-9: Frame-Based Rendering Pipeline

**Why expected:** The existing module has a frame loop, but rendering is ad-hoc. A graphics API needs: begin frame → draw to layers → composite → flush.

**Pipeline stages:**

| Stage | Purpose | ncurses operations |
|-------|---------|-------------------|
| `tui_begin_frame()` | Reset state, clear layers | `werase()` on each layer's WINDOW |
| Draw phase | App/Clay issues draw calls | `mvwprintw`, `mvwaddch`, etc. on layer WINDOWs |
| `tui_composite()` | Merge layers in z-order | Read layer WINDOWs, write to output |
| `tui_end_frame()` | Push composed result | `wnoutrefresh()` + `doupdate()` in frame loop |

**Complexity:** Medium (pipeline orchestration)
**Dependencies:** TS-7 (Drawing Surface), TS-8 (Layer System)

### TS-10: Coordinate System (Float-to-Cell Mapping)

**Why expected:** Clay uses float coordinates. Terminal renderer must convert to integer cell coordinates consistently.

**Mapping:** `floorf` for position (snap to top-left cell), `ceilf` for dimensions (no content clipped). Consistent rounding prevents 1-cell gaps between adjacent elements.

**Complexity:** Low (pure math, but consistency is critical)
**Dependencies:** None (used by all drawing primitives)

## Differentiators (Nice-to-Have)

### D-1: Damage Tracking / Dirty Rectangles

Only redraw cells that changed since last frame. The single largest performance optimization in terminal rendering.

**Complexity:** Medium-High
**Dependencies:** TS-7, TS-9

### D-2: Fill Character Customization

Rectangle fills with characters for texture: `.` for empty, `#` for solid, `ACS_CKBOARD` for shading.

**Complexity:** Low
**Dependencies:** TS-1

### D-3: Unicode Box-Drawing Character Set Support

Multiple border styles: light, heavy, double, rounded, dashed.

**Complexity:** Low (data table)
**Dependencies:** TS-3

### D-4: Horizontal/Vertical Line Drawing

Standalone line primitives for separators and dividers.

**Complexity:** Low
**Dependencies:** TS-7, D-3

### D-5: Text Truncation and Ellipsis

Truncate text exceeding bounding box with ellipsis character.

**Complexity:** Low-Medium
**Dependencies:** TS-2, TS-5

### D-6: Terminal Resize Handling

Detect SIGWINCH, update dimensions, resize layer surfaces, notify observers.

**Complexity:** Medium
**Dependencies:** TS-7, TS-8, TS-9

### D-7: Debug Overlay / Wireframe Mode

Optional rendering of bounding box outlines, layer boundaries, frame stats.

**Complexity:** Low-Medium
**Dependencies:** TS-8

### D-8: Cell Read-Back

Read current content of a cell at position. Useful for compositing and debugging.

**Complexity:** Low
**Dependencies:** TS-7

## Anti-Features (Do NOT Build)

### AF-1: Widget Library

The graphics API provides primitives (rectangles, text, borders), NOT widgets (buttons, sliders, lists). Widgets belong in app code, cels-clay module, or a separate cels-widgets module.

**The boundary:** If thinking about user interaction semantics (selected, hovered, clicked), you are building a widget, not a primitive.

### AF-2: Layout Engine

Clay IS the layout engine. The ncurses module should not compute positions, sizes, padding, margins, or flex/grid layouts.

### AF-3: Animation / Tweening System

Explicitly out of scope. Animation is application-level. The graphics API draws static frames.

### AF-4: Text Editing / Input Fields

Text input requires cursor management, selection state, clipboard, undo/redo. Widget-level concern.

### AF-5: Mouse Input Processing

Explicitly out of scope for v1. Input provider is a separate concern.

### AF-6: True Color Support (v1)

Deferred. Design the color API so true color can be added later without API changes.

### AF-7: Image / Bitmap Rendering

Terminal image rendering (Sixel, Kitty protocol) is terminal-specific and not part of the character-cell model.

### AF-8: Sub-Cell Rendering / Braille Patterns

Specialized visualization concern. Complicates coordinate system.

### AF-9: Scroll State Management

Clay manages scroll state. Graphics API only clips via SCISSOR_START/END.

## Feature Dependencies

```
                   TS-10 (Coordinate Mapping)
                         |
                   TS-4 (Color System)
                   /     |     \
            TS-7 (Surface)    TS-6 (Corner Radius)
            /    |    \            |
    TS-1 (Rect) TS-2 (Text)  TS-3 (Border)
         |       |
    TS-5 (Scissor)
         |
    TS-8 (Layer System)
         |
    TS-9 (Frame Pipeline)
```

**Critical path for Clay compatibility:**
```
TS-10 -> TS-4 -> TS-7 -> TS-1/TS-2/TS-3 -> TS-5 -> TS-8 -> TS-9
```

## MVP Recommendation

Build order for Clay-compatible terminal renderer:

1. **TS-10 + TS-4** — Foundational utilities
2. **TS-7** — Drawing surface wrapping ncurses WINDOW
3. **TS-1 + TS-2 + TS-3 + TS-6** — Core drawing primitives
4. **TS-5** — Clipping for scroll containers
5. **TS-8** — Z-ordered compositing
6. **TS-9** — Frame lifecycle pipeline

**Defer to post-MVP:** D-1 (damage tracking), D-6 (resize), D-7 (debug overlay)

---
*Features research: 2026-02-07*
