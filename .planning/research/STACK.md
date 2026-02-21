# Stack Research: cels-ncurses v1.1

**Research Date:** 2026-02-20
**Confidence:** HIGH (all APIs verified against ncurses 6.6 on target system)
**Domain:** Terminal graphics API -- advanced capabilities (mouse, sub-cell, true color, damage, transparency)
**Scope:** NEW for v1.1 only. v1.0 stack (panels, drawing primitives, color pairs, frame pipeline) is validated and not repeated here.

## System Baseline

| Component | Version | Verified |
|-----------|---------|----------|
| ncurses | 6.6.20251230 | `ncursesw6-config --version` |
| NCURSES_EXT_COLORS | 20251230 | compile test |
| NCURSES_MOUSE_VERSION | 2 | compile test |
| GCC | 15.2.1 | `cc --version` |
| Terminfo `xterm-direct` | Present | `infocmp xterm-direct` |
| `alloc_pair` / `init_extended_pair` | Linked | compile+link test |
| `overlay` / `overwrite` / `copywin` | Linked | compile+link test |

No new external dependencies. Every API needed for v1.1 is already in the installed ncurses 6.6.

---

## 1. Mouse Input

### Recommended APIs

| Function | Purpose | Why |
|----------|---------|-----|
| `mousemask(mask, NULL)` | Enable mouse events at startup | Required once; returns actual supported mask |
| `getmouse(&event)` | Read MEVENT from queue | Called when `wgetch` returns `KEY_MOUSE` |
| `wenclose(win, y, x)` | Hit-test: is (y,x) inside window? | Layer hit-testing for panel dispatch |
| `wmouse_trafo(win, &y, &x, FALSE)` | Convert screen coords to window-local | Transforms MEVENT screen coords to layer-local coords |
| `mouseinterval(0)` | Disable click coalescing | Gives raw press/release for custom drag detection |

### Event Constants (verified on ncurses 6.6)

| Constant | Value | Use |
|----------|-------|-----|
| `BUTTON1_PRESSED` | 0x2 | Left mouse down |
| `BUTTON1_RELEASED` | 0x1 | Left mouse up |
| `BUTTON1_CLICKED` | 0x4 | Left click (press+release within mouseinterval) |
| `BUTTON1_DOUBLE_CLICKED` | 0x8 | Double-click |
| `BUTTON4_PRESSED` | 0x10000 | Scroll wheel up |
| `BUTTON5_PRESSED` | 0x200000 | Scroll wheel down |
| `ALL_MOUSE_EVENTS` | 0xFFFFFFF | All button events (does NOT include motion) |
| `REPORT_MOUSE_POSITION` | 0x10000000 | Motion/hover reporting (separate from ALL_MOUSE_EVENTS) |
| `BUTTON_SHIFT` | 0x4000000 | Shift modifier held during event |
| `BUTTON_CTRL` | 0x2000000 | Ctrl modifier held during event |
| `BUTTON_ALT` | 0x8000000 | Alt modifier held during event |
| `KEY_MOUSE` | (system) | Pseudo-key returned by wgetch when mouse event queued |

### Mouse Protocol: SGR 1006

ncurses 6.6 supports SGR 1006 extended mouse protocol natively. This is critical for terminals wider than 223 columns (the limit of the classic X10 protocol). The terminal must have `XM` capability in its terminfo entry. Modern terminals (kitty, alacritty, foot, xterm) all support SGR 1006.

ncurses handles protocol negotiation internally when the terminfo `kmous` and `XM` capabilities are present. No manual escape sequence emission is needed -- just call `mousemask()`.

### Mouse Mode Recommendation

```c
/* Enable all button events + motion reporting */
mmask_t desired = ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION;
mmask_t actual = 0;
mousemask(desired, &actual);

/* Disable click coalescing for raw press/release events.
 * Default mouseinterval is 166ms -- too slow for interactive apps.
 * Set to 0 for immediate press/release reporting. */
mouseinterval(0);
```

**Use `ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION`**, not just `ALL_MOUSE_EVENTS`. The position reporting flag is separate and required for hover/motion. Without it, you only get button press/release/click events.

### MEVENT Struct

```c
typedef struct {
    short id;       /* Device ID (usually 0) */
    int x, y, z;    /* Screen-relative coordinates. z unused. */
    mmask_t bstate; /* Bitmask of event type + modifiers */
} MEVENT;
```

### Integration with TUI_Input

The current `tui_read_input_ncurses()` reads `wgetch(stdscr)`. Mouse events arrive as `KEY_MOUSE` in the same getch stream. Add a `case KEY_MOUSE:` handler that calls `getmouse()` and populates a new `TUI_MouseState` struct.

**Recommendation:** Do NOT extend `CELS_Input` (a cross-backend type). Instead, create a `TUI_MouseState` global with extern linkage (matching INTERFACE library pattern) populated per-frame by the input system. This keeps mouse state ncurses-specific.

```c
typedef struct TUI_MouseState {
    int x, y;               /* Screen-relative position */
    bool button_left;        /* Left button currently down */
    bool button_right;       /* Right button currently down */
    bool button_middle;      /* Middle button currently down */
    bool clicked_left;       /* Left click this frame */
    bool clicked_right;      /* Right click this frame */
    int scroll_delta;        /* +1 up, -1 down, 0 none */
    bool has_motion;         /* Position changed this frame */
    bool has_event;          /* Any mouse event this frame */
    uint32_t modifiers;      /* BUTTON_SHIFT | BUTTON_CTRL | BUTTON_ALT */
} TUI_MouseState;
```

### Drag Detection Strategy

ncurses does not provide drag events natively. Implement drag detection in userspace:

1. On `BUTTON1_PRESSED`: record `drag_start_x`, `drag_start_y`, set `dragging_pending = true`
2. On `REPORT_MOUSE_POSITION` while `dragging_pending`: if position differs from start, set `is_dragging = true`
3. On `BUTTON1_RELEASED`: if `is_dragging`, emit drag-end; if not, emit click
4. Setting `mouseinterval(0)` is essential -- otherwise ncurses holds the press event waiting for a click timeout

### Layer Hit-Testing

```c
/* Given MEVENT event with screen coords event.y, event.x: */
for (int i = g_layer_count - 1; i >= 0; i--) {  /* top-to-bottom */
    if (!g_layers[i].visible) continue;
    if (wenclose(g_layers[i].win, event.y, event.x)) {
        int local_y = event.y, local_x = event.x;
        wmouse_trafo(g_layers[i].win, &local_y, &local_x, FALSE);
        /* Hit: layer i at local coords (local_x, local_y) */
        break;
    }
}
```

### What NOT to Do

| Avoid | Why |
|-------|-----|
| Manual escape sequences (`\033[?1000h`) | ncurses handles protocol negotiation via terminfo |
| `mouseinterval(166)` (default) | Too slow; introduces 166ms lag on all press events |
| `BUTTON_CLICKED` for interactive UI | Coalesced clicks add latency; use PRESSED/RELEASED |
| Reading mouse from `stdscr` only | Must check `wenclose` per layer for correct hit-testing |

---

## 2. Sub-Cell Rendering (Half-Block + Braille)

### Unicode Block Elements (2x vertical resolution)

Half-block characters double the vertical resolution: each cell becomes 1 column x 2 rows of sub-pixels.

| Character | Codepoint | Effect |
|-----------|-----------|--------|
| Upper Half Block | U+2580 | Top half filled, bottom empty |
| Lower Half Block | U+2584 | Bottom half filled, top empty |
| Full Block | U+2588 | Entire cell filled |
| Left Half Block | U+258C | Left half filled |
| Right Half Block | U+2590 | Right half filled |

**Rendering technique:** For a 2-pixel-tall column at position (col, row):
- If both sub-pixels on: use U+2588 (full block) with fg color
- If top only: use U+2580 with fg=top_color, bg=bottom_color (or terminal default)
- If bottom only: use U+2584 with fg=bottom_color, bg=top_color
- If both off: use space with bg color

The key insight: **fg and bg colors represent the two sub-pixels**. This means each half-block cell needs its own color pair.

### Braille Characters (8x resolution per cell)

Braille patterns (U+2800..U+28FF) provide a 2-column x 4-row dot matrix per cell, giving 8 sub-pixels per character cell.

**Dot-to-bit mapping:**

```
Dot layout:    Bit positions:
(0) (3)        bit 0  bit 3
(1) (4)        bit 1  bit 4
(2) (5)        bit 2  bit 5
(6) (7)        bit 6  bit 7
```

**Codepoint formula:** `0x2800 + bitmask` where bitmask is the OR of active dot bits.

```c
/* Set a sub-pixel at (sx, sy) within a braille cell.
 * sx: 0 or 1 (column within cell)
 * sy: 0..3 (row within cell) */
static const uint8_t braille_dot_map[2][4] = {
    {0x01, 0x02, 0x04, 0x40},  /* left column: bits 0,1,2,6 */
    {0x08, 0x10, 0x20, 0x80},  /* right column: bits 3,4,5,7 */
};
wchar_t braille_char = 0x2800 | braille_dot_map[sx][sy];
```

**Limitation:** Braille characters are single-color (fg only). The 8 dots within a cell all share one foreground color. For color per-dot, you need half-block rendering instead.

### ncurses Wide-Char API for Sub-Cell Characters

```c
/* Construct a cchar_t for a half-block or braille character */
wchar_t wc[2] = { 0x2584, L'\0' };  /* lower half block */
cchar_t cc;
setcchar(&cc, wc, A_NORMAL, pair_num, NULL);
mvwadd_wch(win, row, col, &cc);
```

This uses the same `setcchar()` + `mvwadd_wch()` pattern already used for rounded corners in v1.0.

### Recommendation

Use **half-block rendering** as the primary sub-cell mode (2x vertical resolution with full fg/bg color per sub-pixel). Provide braille as an optional high-resolution mode (8x resolution, monochrome per cell). Do NOT make braille the default -- it is harder to read and single-color.

---

## 3. True Color (24-bit RGB)

### Current State

v1.0 uses `tui_color_rgb(r, g, b)` which eagerly maps RGB to the nearest xterm-256 palette index via `rgb_to_nearest_256()`. Color pairs are created with `alloc_pair(fg_index, bg_index)` where indices are 0-255.

### Direct Color Architecture

ncurses 6.1+ supports **direct color mode** where color values passed to `init_extended_pair()` are raw packed RGB (0xRRGGBB) instead of palette indices. This requires:

1. Terminal supports 24-bit color (nearly all modern terminals: kitty, alacritty, foot, wezterm, xterm, iTerm2, Windows Terminal)
2. `$TERM` set to a direct-color terminfo entry (e.g., `xterm-direct`, `kitty-direct`, `foot-direct`, `alacritty-direct`)
3. Application uses `init_extended_pair()` instead of `init_pair()` and `alloc_pair()`

### Available Terminfo Entries (verified on system)

| Entry | Terminal |
|-------|----------|
| `xterm-direct` | xterm, generic |
| `kitty-direct` | kitty |
| `alacritty-direct` | alacritty |
| `foot-direct` | foot |
| `contour-direct` | contour |

The `xterm-direct` entry reports: `colors#0x1000000` (16,777,216 colors), `pairs#0x10000` (65,536 pairs).

### Detection Strategy

```c
#include <term.h>

bool tui_color_has_direct_color(void) {
    int result = tigetflag("RGB");
    /* tigetflag returns:
     *   1  = boolean capability present and true
     *   0  = boolean capability present and false
     *  -1  = capability not found
     * Also check tigetnum("RGB") for some entries that use numeric RGB */
    if (result > 0) return true;
    int num_result = tigetnum("RGB");
    if (num_result > 0) return true;
    return false;
}
```

### API for Direct Color

```c
/* Palette-indexed mode (current v1.0 -- works everywhere): */
int pair = alloc_pair(fg_index_0_255, bg_index_0_255);

/* Direct color mode (v1.1 -- requires direct-color terminal): */
int fg_rgb = (r << 16) | (g << 8) | b;  /* packed 0xRRGGBB */
int bg_rgb = (br << 16) | (bg << 8) | bb;
init_extended_pair(pair_number, fg_rgb, bg_rgb);
/* Then use wattr_set(win, attrs, (short)pair_number, NULL); */
```

**Critical difference:** In direct color mode, `alloc_pair()` still works but expects packed RGB values as the fg/bg arguments, NOT palette indices. The behavior depends entirely on the active terminfo entry.

### Recommended Architecture

**Dual-mode color system with runtime detection:**

```c
typedef struct TUI_Color {
    int index;      /* -1 = default, 0-255 = palette (v1.0 path) */
    uint8_t r, g, b; /* Original RGB values (new in v1.1) */
    bool has_rgb;    /* true if created from tui_color_rgb() */
} TUI_Color;
```

At startup, detect whether direct color is available. Store the mode globally:

```c
typedef enum {
    TUI_COLOR_MODE_PALETTE,  /* 256-color palette (v1.0 behavior) */
    TUI_COLOR_MODE_DIRECT,   /* 24-bit direct color */
} TUI_ColorMode;

extern TUI_ColorMode g_color_mode;
```

In `tui_style_apply()`:
- If `g_color_mode == TUI_COLOR_MODE_PALETTE`: use existing `alloc_pair(fg.index, bg.index)` path
- If `g_color_mode == TUI_COLOR_MODE_DIRECT`: compute packed RGB, use `alloc_pair(fg_packed, bg_packed)`

### Color Pair Limits

| Mode | Max Pairs | Max Colors |
|------|-----------|------------|
| Palette (v1.0) | 65,535 (alloc_pair limit) | 256 |
| Direct | 65,536 (xterm-direct pairs limit) | 16,777,216 |

The pair limit in direct color mode (65,536) means you cannot have every possible fg/bg combination simultaneously. This is adequate for TUI use but warrants a pair eviction strategy for long-running applications. `alloc_pair` already handles this internally via LRU eviction in ncurses.

### What NOT to Do

| Avoid | Why |
|-------|-----|
| `init_color()` to redefine palette entries | Not all terminals support `can_change_color()`; destroys shared palette |
| Raw `\033[38;2;R;G;Bm` escape sequences | Bypasses ncurses -- corrupts internal state tracking |
| Assuming direct color always available | Must fall back to palette mode gracefully |
| Removing palette support | Many SSH sessions, tmux, and screen use 256-color TERM |

---

## 4. Damage Tracking (Dirty Rectangles)

### Current State

v1.0 has per-layer dirty tracking: `TUI_Layer.dirty` is a boolean set by `tui_layer_get_draw_context()`. In `tui_frame_begin()`, dirty layers are `werase()`d. This is whole-layer granularity.

### ncurses Internal Damage Tracking

ncurses already tracks damage at per-line granularity internally:

| Function | Purpose |
|----------|---------|
| `touchwin(win)` | Mark entire window dirty |
| `touchline(win, start, count)` | Mark specific lines dirty |
| `untouchwin(win)` | Mark entire window clean |
| `wtouchln(win, y, n, changed)` | Mark n lines starting at y as changed (1) or unchanged (0) |
| `is_linetouched(win, line)` | Query: was this line modified since last refresh? |
| `is_wintouched(win)` | Query: was any line modified since last refresh? |

ncurses uses this internally during `wnoutrefresh()` / `doupdate()` to emit only the changed lines to the terminal. **The terminal I/O optimization is already happening.**

### What Dirty Rectangles Add

The value of dirty rectangles at the application level is NOT about terminal I/O (ncurses handles that). It is about **avoiding unnecessary redraw computation**:

1. **Skip `werase()` for clean layers** -- already done in v1.0
2. **Skip `werase()` for clean regions within a layer** -- new in v1.1
3. **Skip draw calls for cells outside dirty rectangles** -- application-level optimization
4. **Minimize `setcchar()` / `mvwadd_wch()` calls** -- each one costs CPU even if ncurses deduplicates at I/O time

### Recommended Architecture

**Per-layer dirty rectangle list** rather than per-cell bitmaps:

```c
#define TUI_MAX_DIRTY_RECTS 16

typedef struct TUI_DirtyRegion {
    TUI_CellRect rects[TUI_MAX_DIRTY_RECTS];
    int count;
    bool full_dirty;  /* Overflow or explicit invalidate -> entire layer dirty */
} TUI_DirtyRegion;
```

**Integration points:**

1. Drawing primitives (`tui_draw_fill_rect`, `tui_draw_text`, etc.) optionally record dirty rects into the layer's `TUI_DirtyRegion`
2. `tui_frame_begin()` clears only dirty rects (via targeted `mvwhline` with spaces) instead of `werase()` on the entire window
3. On overflow (>16 rects), fall back to `full_dirty` which triggers `werase()` (current behavior)
4. On resize: `full_dirty = true` for all layers (already done via `tui_frame_invalidate_all()`)

### Leveraging ncurses Touch Functions

Use `wtouchln()` to tell ncurses exactly which lines were modified, avoiding per-line diff during `wnoutrefresh()`:

```c
/* After selective clearing of dirty rects: */
for (int i = 0; i < dirty_region.count; i++) {
    TUI_CellRect r = dirty_region.rects[i];
    touchline(layer->win, r.y, r.h);
}
```

This is a micro-optimization -- ncurses already diffs internally. But for large windows with small dirty regions, telling ncurses exactly which lines changed avoids scanning clean lines.

### What NOT to Do

| Avoid | Why |
|-------|-----|
| Per-cell dirty bitmaps | 200x50 = 10,000 bools per layer; cache-hostile, wasteful |
| Shadow buffer (double-buffer at cell level) | Duplicates ncurses internal virtual screen; wastes memory |
| `clearok(win, TRUE)` for "refresh" | Forces complete terminal repaint -- defeats all optimization |
| Bypassing `werase()` entirely | Some applications need full clear; keep as fallback |

---

## 5. Layer Transparency

### Terminal Transparency Landscape

Terminals have NO alpha channel. Each cell has exactly one foreground color and one background color. There is no concept of partial transparency or alpha blending at the terminal output level.

What IS possible:

1. **Binary transparency** via `overlay()`: copy non-blank cells from source to destination
2. **Full-layer copy** via `overwrite()`: copy all cells including blanks
3. **Region copy** via `copywin()`: fine-grained source/dest region specification
4. **Default background transparency** via `use_default_colors()` + color index `-1`

### ncurses Overlay Functions (verified on system)

| Function | Signature | Behavior |
|----------|-----------|----------|
| `overlay(src, dst)` | `int overlay(const WINDOW*, WINDOW*)` | Non-destructive: copies non-blank chars from src to dst |
| `overwrite(src, dst)` | `int overwrite(const WINDOW*, WINDOW*)` | Destructive: copies ALL chars from src to dst |
| `copywin(src, dst, sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol, overlay_flag)` | See man page | Fine-grained region copy; if overlay_flag=TRUE, non-destructive |

**How `overlay()` determines "blank":** A character is blank if it matches the background character of the source window (set via `wbkgdset()`). By default, the background character is a space.

### Recommended Architecture: Software Compositing

Since panels handle z-order but NOT transparency, implement a **software compositing pass** between the draw phase and the panel refresh phase:

1. Layers with `opacity < 1.0` are marked as transparent
2. Before `update_panels()`, transparent layers are composited into the layer below using `overlay()` or `copywin()`
3. Only non-blank cells are copied (binary transparency)

```c
typedef struct TUI_Layer {
    /* ... existing fields ... */
    float opacity;              /* 1.0 = fully opaque (default, panel handles), < 1.0 = transparent */
    bool transparent;           /* Cached: opacity < 1.0 */
} TUI_Layer;
```

**For binary transparency (recommended first pass):**

```c
/* In frame_end, before update_panels(): */
for each transparent layer (top-to-bottom):
    /* Get the layer below in z-order */
    TUI_Layer* below = get_layer_below(layer);
    if (below) {
        overlay(layer->win, below->win);
        hide_panel(layer->panel);  /* Don't let panels draw it again */
    }
```

**For simulated alpha blending (optional, advanced):**

True alpha blending is not possible in terminals, but you can simulate it:
- Dim text attributes (`A_DIM`) on the "transparent" layer
- Character mixing: replace some cells with the background layer's content based on a dither pattern
- Color blending: read both layer's colors, blend in software, write blended color to output

This is expensive and fragile. Recommend starting with binary transparency only.

### use_default_colors() for Terminal Transparency

The existing `use_default_colors()` + `assume_default_colors(-1, -1)` call in `tui_hook_startup()` already enables terminal-native transparency. When the terminal emulator itself has transparency configured (e.g., Kitty's `background_opacity`), cells with bg=-1 show through to the desktop.

No additional work needed for this form of transparency -- it already works.

### What NOT to Do

| Avoid | Why |
|-------|-----|
| Per-cell alpha channel | Terminals cannot display fractional transparency |
| `overwrite()` for "transparency" | Copies blanks too -- destroys content below |
| Hiding panels then redrawing | Causes flicker; composite first, then refresh |
| Assuming `overlay()` is fast for large windows | It copies cell-by-cell; only use on small overlay layers |

---

## Build System Changes

No new dependencies. No CMakeLists.txt changes needed for v1.1. All required APIs are already in `libncursesw` and `libpanelw` which are linked in v1.0.

The only addition needed is `#include <term.h>` in the color module for `tigetflag()` / `tigetnum()` (true color detection). This header is part of ncurses.

---

## Summary: Technology Decisions

| Capability | Primary API | Fallback | Confidence |
|------------|-------------|----------|------------|
| Mouse input | `mousemask()` + `getmouse()` + `wenclose()` | None needed (ncurses standard) | HIGH |
| Sub-cell rendering | `setcchar()` + U+2580/U+2584 half-blocks | Braille U+2800 for higher resolution | HIGH |
| True color | `alloc_pair()` with packed RGB + `tigetflag("RGB")` detection | Fall back to 256-color palette | HIGH |
| Damage tracking | Custom `TUI_DirtyRegion` + `touchline()` hints | `werase()` full layer (current v1.0) | HIGH |
| Layer transparency | `overlay()` for binary compositing | `use_default_colors()` for terminal-level transparency | HIGH |

---

## Roadmap Implications

1. **Mouse input** has zero dependencies on other v1.1 features. Start here -- it unblocks interactive UI work immediately.
2. **Sub-cell rendering** is pure drawing-layer work. Can proceed in parallel with mouse.
3. **True color** requires modifying `TUI_Color` struct and `tui_style_apply()` -- touches the foundation. Do this before damage tracking since damage tracking depends on the style system.
4. **Damage tracking** refines `tui_frame_begin()` and drawing primitives. Do after true color to avoid rework.
5. **Layer transparency** depends on the compositing happening before `update_panels()` in `tui_frame_end()`. Do last since it modifies the critical frame pipeline path.

## Sources

- [ncurses curs_mouse man page (6.5)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html)
- [ncurses curs_color man page (6.6)](https://invisible-island.net/ncurses/man/curs_color.3x.html)
- [ncurses direct color discussion](https://lists.gnu.org/archive/html/bug-ncurses/2018-04/msg00011.html)
- [SGR 1006 extended mouse in ncurses](https://lists.gnu.org/archive/html/bug-ncurses/2013-01/msg00012.html)
- [Braille Patterns Unicode block](https://en.wikipedia.org/wiki/Braille_Patterns)
- [Block Elements Unicode block](https://en.wikipedia.org/wiki/Block_Elements)
- [drawille: pixel graphics with braille](https://github.com/asciimoo/drawille)
- [ncurses overlay/copywin man page](https://manpages.debian.org/testing/ncurses-doc/copywin.3ncurses.en.html)
- [ncurses touch functions man page](https://linux.die.net/man/3/touchwin)
- [Arch Linux ncurses mouse events thread](https://bbs.archlinux.org/viewtopic.php?id=279584)
- [Ncurses transparency techniques](https://hoop.dev/blog/ncurses-processing-transparency-without-hacks/)

---
*Stack research v1.1: 2026-02-20*
