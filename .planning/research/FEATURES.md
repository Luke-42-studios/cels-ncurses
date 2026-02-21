# Feature Landscape: cels-ncurses v1.1 Enhanced Rendering

**Domain:** Terminal graphics API / ncurses rendering engine
**Researched:** 2026-02-20
**Confidence:** HIGH (ncurses header verified on system, ecosystem researched)
**Sources:** ncurses 6.6 system headers, notcurses architecture, ratatui rendering model, ncurses official docs

## Overview

v1.0 delivered the foundational graphics backend: drawing primitives, panel-backed layers, frame pipeline, color/attribute system. v1.1 promotes five features that were explicitly deferred from v1.0 (mouse input, sub-cell rendering, true color, damage tracking, layer transparency) into active development.

Each feature area is analyzed below with: expected behavior from the developer's perspective, ncurses implementation details verified against the installed system (ncurses 6.6.20251230), and categorization into table stakes / differentiators / anti-features.

---

## Feature Area 1: Mouse Input

### Table Stakes

#### M-1: Click Detection (Left/Right/Middle)

**Why expected:** Any interactive terminal UI needs click-to-select. The downstream cels-clay consumer renders interactive elements (buttons, list items) that require click hit-testing.

**Expected developer API:**
```c
// In CELS_Input struct (or new TUI_MouseEvent):
bool mouse_clicked;        // true on click event this frame
int mouse_x, mouse_y;     // cell coordinates of click
int mouse_button;          // 1=left, 2=middle, 3=right
```

**ncurses implementation (verified):**
- `mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL)` enables reporting
- `getch()` returns `KEY_MOUSE`, then `getmouse(&event)` fills `MEVENT`
- `MEVENT` struct: `{ short id; int x, y, z; mmask_t bstate; }`
- Click constants: `BUTTON1_CLICKED`, `BUTTON2_CLICKED`, `BUTTON3_CLICKED`
- Press/release: `BUTTON1_PRESSED`, `BUTTON1_RELEASED` (per button)
- Modifier keys: `BUTTON_SHIFT`, `BUTTON_CTRL`, `BUTTON_ALT` (bitmask in bstate)
- `mouseinterval(0)` disables click resolution delay for snappier response

**Coordinate translation:** `wmouse_trafo(win, &y, &x, false)` converts screen coordinates to window-local. Critical for panel-backed layers where each layer's WINDOW has its own coordinate system.

**Complexity:** Medium
**Dependencies:** Existing tui_input.c (extends KEY_MOUSE handling in switch statement)

#### M-2: Scroll Wheel

**Why expected:** Scroll containers are a primary Clay use case. Without scroll wheel input, users cannot interact with scrollable content.

**Expected developer API:**
```c
int mouse_scroll;     // +1 scroll up, -1 scroll down, 0 no scroll
int mouse_scroll_x;   // cell x where scroll occurred
int mouse_scroll_y;   // cell y where scroll occurred
```

**ncurses implementation (verified):**
- Scroll up: `BUTTON4_PRESSED` (no matching release -- xterm convention)
- Scroll down: `BUTTON5_PRESSED` (no matching release)
- Both buttons 4 and 5 are present in `/usr/include/ncurses.h` (lines 2001-2017)
- The `z` field in MEVENT is not used by standard terminals for scroll amount

**Complexity:** Low (two additional cases in mouse event parsing)
**Dependencies:** M-1 (shared mouse infrastructure)

#### M-3: Layer-Aware Hit Testing

**Why expected:** With panel-backed layers, a click at screen position (10, 5) may hit a popup layer on top, not the background. The developer needs to know which layer received the click.

**Expected developer API:**
```c
// Option A: Module resolves layer automatically
TUI_Layer* hit_layer = tui_mouse_get_hit_layer(screen_x, screen_y);

// Option B: Developer tests manually
bool hit = tui_cell_rect_contains(layer_rect, mouse_x, mouse_y);
```

**ncurses support:** `wenclose(win, y, x)` tests if screen coordinates fall within a WINDOW. Combined with panel z-order iteration (top-to-bottom), this gives accurate hit testing.

**Complexity:** Medium (z-order traversal, visibility check)
**Dependencies:** M-1, existing TUI_Layer system

### Differentiators

#### M-4: Drag Detection

**Why expected by power users:** Drag enables resize handles, slider controls, selection rectangles. Not every terminal UI needs it, but the infrastructure (press + motion + release) makes it natural to expose.

**Expected developer API:**
```c
bool mouse_dragging;       // true while button held + position changed
int mouse_drag_start_x;   // where drag began
int mouse_drag_start_y;
int mouse_drag_button;     // which button initiated drag
```

**ncurses implementation:**
- Track BUTTON_PRESSED -> REPORT_MOUSE_POSITION (motion while held) -> BUTTON_RELEASED
- `mouseinterval(0)` is required -- otherwise ncurses synthesizes CLICKED from fast press+release and never reports the raw press/release pair
- Requires `REPORT_MOUSE_POSITION` in mousemask for motion reporting during drag
- Motion events are high-frequency; the input system should throttle or only report on frame boundaries

**Complexity:** Medium-High (state machine: idle -> pressing -> dragging -> released)
**Dependencies:** M-1

#### M-5: Hover / Motion Tracking

**Why expected by UI frameworks:** Hover state (highlight on mouseover) is standard in GUI toolkits. Clay's downstream consumers will want hover feedback.

**Expected developer API:**
```c
bool mouse_moved;         // true if mouse position changed this frame
int mouse_x, mouse_y;    // current position (updated every frame with motion)
```

**ncurses implementation:**
- `REPORT_MOUSE_POSITION` mask enables motion reporting
- WARNING: Motion events are generated for every cell the mouse moves through. At 60fps this is manageable, but the input system must drain the queue and only report the final position per frame.
- Terminal support varies: xterm, kitty, foot, alacritty all support motion tracking. Some older terminals do not.

**Complexity:** Medium (queue draining, performance consideration)
**Dependencies:** M-1

### Anti-Features (Mouse)

#### MA-1: Mouse Gesture Recognition

Do NOT build swipe/pinch/gesture detection. This is application-level logic built on top of raw events. The graphics backend provides coordinates and button state only.

#### MA-2: Mouse Cursor Rendering

Do NOT render a custom mouse cursor. The terminal emulator owns cursor rendering. The module provides position data, not cursor drawing.

#### MA-3: Double/Triple Click Semantic Handling

ncurses provides `BUTTON1_DOUBLE_CLICKED` and `BUTTON1_TRIPLE_CLICKED`. Expose these as raw events if they arrive, but do NOT build semantic handling (word-select on double-click, line-select on triple-click). That is widget-level behavior.

---

## Feature Area 2: Sub-Cell Rendering

### Table Stakes

#### S-1: Half-Block Characters (2x vertical resolution)

**Why expected:** Half-block rendering is the standard technique for doubling vertical resolution in terminal graphics. Every serious terminal renderer supports it (notcurses NCBLIT_2x1, drawille, ratatui's canvas widget).

**How it works:** Each terminal cell becomes a 1x2 pixel grid using:
- U+2580 `UPPER HALF BLOCK` (top pixel on)
- U+2584 `LOWER HALF BLOCK` (bottom pixel on)
- U+2588 `FULL BLOCK` (both pixels on)
- Space ` ` (both pixels off)

The foreground color represents the "on" pixel, background color represents the "off" pixel. When both pixels are different colors, use UPPER HALF BLOCK with fg=top_color, bg=bottom_color.

**Expected developer API:**
```c
// Draw a "pixel" at sub-cell resolution
void tui_draw_halfblock_pixel(TUI_DrawContext* ctx, int px, int py,
                               TUI_Color color);

// Fill a rectangle at sub-cell resolution (2x vertical)
void tui_draw_halfblock_rect(TUI_DrawContext* ctx, TUI_CellRect cell_rect,
                              TUI_Color color);
```

**Coordinate model:** Sub-cell pixel coordinates where py=0..2*height-1 (doubled vertical). The draw function converts: cell_y = py / 2, half = py % 2. When two pixels share a cell, the function reads current cell state and composites.

**Complexity:** Medium (requires reading current cell to composite top/bottom halves)
**Dependencies:** Existing TUI_DrawContext, TUI_Color, wide-char support (already using cchar_t)

#### S-2: Quadrant Block Characters (2x2 resolution)

**Why expected:** Quadrant characters provide 2x resolution in both axes. Available since Unicode 1.0, widely supported.

**Characters (U+2596-U+259F):**
- U+2596 `QUADRANT LOWER LEFT`
- U+2597 `QUADRANT LOWER RIGHT`
- U+2598 `QUADRANT UPPER LEFT`
- U+2599 `QUADRANT UPPER LEFT AND LOWER LEFT AND LOWER RIGHT`
- U+259A `QUADRANT UPPER LEFT AND LOWER RIGHT`
- U+259B `QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER LEFT`
- U+259C `QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER RIGHT`
- U+259D `QUADRANT UPPER RIGHT`
- U+259E `QUADRANT UPPER RIGHT AND LOWER LEFT`
- U+259F `QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT`

Plus U+2588 (FULL BLOCK), U+2580 (UPPER HALF), U+2584 (LOWER HALF), U+258C (LEFT HALF), U+2590 (RIGHT HALF).

**Limitation:** Only 2 colors per cell (fg + bg). When a quadrant pattern requires 3+ colors, approximation is needed.

**Complexity:** Medium-High (16 possible states per cell, color constraint)
**Dependencies:** S-1

### Differentiators

#### S-3: Braille Pattern Characters (2x4 resolution)

**Why expected by data visualization apps:** Braille patterns (U+2800-U+28FF) provide 2 columns x 4 rows = 8 sub-pixels per cell. This is the highest resolution available in pure Unicode terminal rendering.

**How it works:** Each braille character encodes 8 dots as a bitmask:
```
Dot layout:    Bit positions:
[1] [4]        bit 0, bit 3
[2] [5]        bit 1, bit 4
[3] [6]        bit 2, bit 5
[7] [8]        bit 6, bit 7
```
Character = U+2800 + bitmask (0x00 to 0xFF = 256 patterns).

**Limitation:** Only 2 colors per cell (same as quadrants). Braille is best for single-color plotting, not filled rectangles.

**Expected developer API:**
```c
// Set a braille sub-pixel at (px, py) where px=0..2*width-1, py=0..4*height-1
void tui_draw_braille_pixel(TUI_DrawContext* ctx, int px, int py,
                             TUI_Color color);

// Higher-level: plot a line at braille resolution
void tui_draw_braille_line(TUI_DrawContext* ctx, int x0, int y0,
                            int x1, int y1, TUI_Color color);
```

**Complexity:** Medium (bitmask arithmetic is straightforward; compositing multiple pixels in same cell requires read-modify-write)
**Dependencies:** S-1

#### S-4: Sextant Characters (2x3 resolution)

**Added in Unicode 13 (2020).** Sextants provide 6 sub-pixels per cell (2 columns x 3 rows). This is a middle ground between quadrants (2x2=4) and braille (2x4=8).

**Terminal support:** Newer fonts (e.g., JetBrains Mono, Cascadia Code) include sextant glyphs. Older fonts may not. Font coverage check is recommended.

**Complexity:** Medium
**Dependencies:** S-1

### Anti-Features (Sub-Cell)

#### SA-1: Sixel / Kitty Graphics Protocol

Do NOT implement terminal-specific image protocols. These bypass the character cell model entirely and create terminal compatibility issues. The sub-cell rendering system operates within the Unicode character model.

#### SA-2: Sub-Cell Text Rendering

Do NOT attempt to render text at sub-cell resolution. Text belongs in full cells. Sub-cell rendering is for graphical elements (fills, borders, plots) only.

#### SA-3: Automatic Resolution Selection

Do NOT auto-detect "best" sub-cell mode. Let the developer explicitly choose half-block, quadrant, or braille. Different use cases (UI elements vs. data plots) need different modes.

---

## Feature Area 3: True Color (24-bit RGB)

### Table Stakes

#### C-1: Direct Color Mode (Packed RGB as Color Index)

**Why expected:** The v1.0 color system maps RGB to the nearest xterm-256 index, losing precision. Modern terminals (kitty, foot, alacritty, wezterm, ghostty, Windows Terminal, iTerm2) all support 24-bit color. Clay provides exact RGB values that should be preserved.

**Current state:** `tui_color_rgb(r, g, b)` calls `rgb_to_nearest_256()` and stores a 256-color index in `TUI_Color.index`. This works but quantizes colors.

**ncurses direct color support (verified):**
- ncurses 6.1+ supports direct color mode via `init_extended_pair(pair, fg, bg)` where fg/bg are packed RGB values (e.g., `0xFF7F00` for orange)
- Detection: `tigetflag("RGB")` returns non-zero when terminal supports direct color
- Requires `TERM=xterm-direct` or a terminfo entry with the RGB flag
- `alloc_pair(fg, bg)` (already used in v1.0) works with extended color values when in direct color mode
- `NCURSES_EXT_COLORS` is defined as `20251230` on this system (confirmed)

**Expected developer API:**
```c
// No API change needed! tui_color_rgb() signature stays the same.
// Internally: if direct color available, store packed RGB instead of 256-index.

typedef struct TUI_Color {
    int index;     // -1 = default, 0-255 = xterm-256, or packed RGB in direct mode
} TUI_Color;

// The change is internal to tui_color_rgb() and tui_style_apply():
// - At init: detect direct color via tigetflag("RGB")
// - If direct: tui_color_rgb(r,g,b) returns packed (r<<16 | g<<8 | b)
// - tui_style_apply uses init_extended_pair() with packed RGB values
```

**Complexity:** Medium (detection logic, dual code path in color.c, testing across terminal types)
**Dependencies:** None -- extends existing TUI_Color/TUI_Style

#### C-2: Graceful Fallback to 256-Color

**Why expected:** Not all terminals support direct color. SSH sessions, tmux without proper configuration, and older terminal emulators fall back to 256 colors. The renderer must degrade gracefully.

**Implementation:** At startup, probe terminal capabilities:
1. Check `tigetflag("RGB")` for direct color
2. Check `COLORS` variable (set by `start_color()`) for palette size
3. Store capability level in a global: `DIRECT_COLOR`, `COLOR_256`, `COLOR_16`, `COLOR_8`
4. `tui_color_rgb()` uses the appropriate mapping based on detected level

**Complexity:** Low-Medium (detection + switch on capability level)
**Dependencies:** C-1

#### C-3: Environment Variable Override

**Why expected:** Developers running under tmux/screen may need to force a color mode. The `COLORTERM=truecolor` environment variable is a de facto standard hint.

**Implementation:**
- Check `$COLORTERM` environment variable: if `"truecolor"` or `"24bit"`, enable direct color
- Check `$TERM` for `xterm-direct` or similar
- Allow explicit override via TUI_Window config: `.color_mode = TUI_COLOR_DIRECT`

**Complexity:** Low
**Dependencies:** C-1, C-2

### Differentiators

#### C-4: Color Caching / Pair Pool Optimization

**Why expected by high-performance apps:** `alloc_pair()` has an internal LRU cache in ncurses, but for direct color mode the pair space is larger. Caching frequently-used color pairs at the application level can reduce `alloc_pair` calls.

**Complexity:** Medium
**Dependencies:** C-1

### Anti-Features (True Color)

#### CA-1: Terminal-Specific Escape Sequences

Do NOT bypass ncurses to write raw ANSI escape sequences (e.g., `\033[38;2;R;G;Bm`). This breaks ncurses' internal state tracking and will cause rendering corruption. Always use ncurses extended color APIs.

**Why this matters:** Notcurses explicitly bypasses ncurses for performance, but it also takes over ALL terminal output. cels-ncurses uses ncurses panels and must work within the ncurses ecosystem. Raw escape sequences and ncurses cannot coexist on the same terminal session.

#### CA-2: HDR / Wide Gamut Color

Do NOT implement anything beyond 8-bit-per-channel RGB. No terminal supports HDR color.

#### CA-3: Color Space Conversion

Do NOT implement HSL/HSV/LAB color spaces in the core module. If needed, this is a utility library concern. The color API takes RGB.

---

## Feature Area 4: Damage Tracking

### Table Stakes

#### D-1: Per-Cell Dirty Tracking (Shadow Buffer)

**Why expected:** The v1.0 frame pipeline redraws ALL dirty layers every frame (werase + full redraw). With complex UIs, this wastes bandwidth sending unchanged cells to the terminal. Every mature terminal renderer uses damage tracking:
- ncurses itself has internal change tracking per WINDOW
- notcurses maintains a "lastframe" buffer and a "damagemap" bitmap
- ratatui compares current buffer to previous buffer cell-by-cell

**How it works:**
1. Maintain a shadow buffer: array of `TUI_Cell` structs (character + fg + bg + attrs) sized to terminal dimensions
2. On each `tui_frame_end()`, compare each drawn cell against shadow buffer
3. Only emit ncurses calls for cells that differ
4. Update shadow buffer with new values

**Expected developer API:**
```c
// No API change for the developer! Damage tracking is internal to frame pipeline.
// Developer still calls:
tui_draw_fill_rect(&ctx, rect, ' ', style);
tui_draw_text(&ctx, x, y, "hello", style);
// ... and frame_end() optimizes the output automatically.

// Optional: force full redraw (already exists)
tui_frame_invalidate_all();  // marks all cells dirty
```

**Architecture note:** ncurses already has internal change tracking via `wnoutrefresh()` which only marks changed lines. However, ncurses operates at the WINDOW level -- `update_panels()` + `doupdate()` still processes all visible panels. The shadow buffer operates at the application level, above ncurses, deciding which cells to write to the WINDOW in the first place.

**Wait -- should we fight ncurses or work with it?**

There are two approaches:

**Approach A: Application-level shadow buffer (bypass ncurses optimization)**
- Maintain our own cell grid
- Compare before writing to ncurses WINDOWs
- Skip ncurses calls for unchanged cells
- Pro: Full control over what gets written
- Con: Duplicates work ncurses already does internally

**Approach B: Trust ncurses optimization + optimize draw calls**
- ncurses already tracks which characters changed in each WINDOW
- `wnoutrefresh()` marks changed lines for `doupdate()`
- `doupdate()` uses terminal cursor optimization to minimize output
- Our job: avoid calling draw functions for unchanged regions
- Pro: Works with ncurses, not against it
- Con: Less control, still redraws at WINDOW granularity

**Recommendation: Approach B (trust ncurses) with selective optimization.**
The v1.0 frame pipeline already uses `werase()` on dirty layers, which erases ALL content and forces full redraw. The key optimization is: stop calling `werase()` on every frame. Instead:
1. Track which REGIONS of each layer changed (not per-cell, but per-rect)
2. Only erase and redraw changed regions
3. Let ncurses handle terminal output optimization via wnoutrefresh/doupdate

**Complexity:** Medium-High
**Dependencies:** Existing frame pipeline

#### D-2: Layer-Level Dirty Tracking (Already Exists -- Extend)

**Why expected:** v1.0 already marks layers dirty when `tui_layer_get_draw_context()` is called. The improvement is: don't mark the entire layer dirty, mark specific rectangles.

**Current behavior:** `layer->dirty = true` -> entire layer is `werase()`d on next `tui_frame_begin()`.

**Improved behavior:** Track dirty rectangles per layer:
```c
typedef struct TUI_Layer {
    // ... existing fields ...
    TUI_CellRect dirty_rects[TUI_DIRTY_RECT_MAX];
    int dirty_rect_count;
    bool fully_dirty;  // fallback: entire layer needs redraw
} TUI_Layer;
```

When drawing, only erase the dirty regions instead of the entire layer.

**Complexity:** Medium
**Dependencies:** D-1, existing TUI_Layer

### Differentiators

#### D-3: Frame Skipping (No-Change Detection)

**Why expected by game-like apps:** If nothing changed since last frame, skip the entire render pass. The ECS system already has reactivity -- if no state changed, no composition reruns, no draw calls issued.

**Implementation:** Count draw calls per frame. If zero draw calls were issued, skip `update_panels()` + `doupdate()` entirely.

**Complexity:** Low (counter in frame pipeline)
**Dependencies:** D-1

#### D-4: Synchronized Output Protocol

**Why expected by modern terminals:** The synchronized output protocol brackets terminal writes with escape sequences that tell the terminal to buffer output and display atomically, preventing visible tearing.

**Implementation:**
- Before `doupdate()`: write `\033[?2026h` (begin synchronized update)
- After `doupdate()`: write `\033[?2026l` (end synchronized update)
- Detection: check `$TERM_PROGRAM` or use `tigetstr("Sync")` if available

**Terminal support:** kitty, foot, iTerm2, WezTerm, Contour, Windows Terminal. xterm does NOT support it. Must be opt-in or detected.

**Complexity:** Low
**Dependencies:** Frame pipeline

### Anti-Features (Damage Tracking)

#### DA-1: Per-Cell Application-Level Shadow Buffer

Do NOT maintain a full application-level cell grid that duplicates ncurses' internal tracking. ncurses already tracks changes per WINDOW. Building a second tracking layer adds memory overhead (COLS * LINES * sizeof(TUI_Cell) per layer) and complexity without proportional benefit.

Instead: optimize WHAT gets drawn (skip unchanged regions), not WHETHER ncurses notices it changed.

#### DA-2: Diff-Based Terminal Output

Do NOT bypass ncurses to compute terminal diffs and emit raw escape sequences. Notcurses does this because it replaces ncurses entirely. cels-ncurses uses ncurses panels and must let ncurses own terminal output.

#### DA-3: Multi-Threaded Rendering

Do NOT introduce threads for rendering. ncurses is not thread-safe. All drawing must happen on the main thread.

---

## Feature Area 5: Layer Transparency

### Table Stakes

#### T-1: Binary Transparency (Transparent/Opaque Cells)

**Why expected:** When a popup layer overlaps the background, uncovered cells should show through. Without transparency, panels render with opaque backgrounds, hiding the layer below even where no content exists.

**Current behavior:** ncurses panels handle overlap by copying the top panel's content over lower panels. Empty cells in a panel are space characters with default attributes -- they are OPAQUE, hiding content below.

**How binary transparency works:**
- Define a "transparent" cell state: cell has not been drawn to
- During compositing, skip transparent cells -- let lower layer content show through
- ncurses panel library does NOT natively support per-cell transparency

**Implementation approaches:**

**Approach A: Panel Transparency (ncurses native -- limited)**
The ncurses `set_panel_userptr()` + custom overlay logic. Not directly supported; the panel library composites opaquely.

**Approach B: Manual Compositing with overwrite()**
- Use `overwrite(src_win, dst_win)` to copy non-blank cells from one window to another
- Mark transparent cells with a sentinel character (e.g., NUL)
- Iterate cells and selectively copy

**Approach C: Application-Level Compositing Buffer**
- Maintain a final compositing buffer (separate WINDOW)
- Iterate layers bottom-to-top
- For each cell in each layer: if cell is "drawn" (not transparent), write to buffer
- This replaces `update_panels()` for the transparent layers

**Recommendation: Approach C** -- application-level compositing. This is how notcurses handles it. The ncurses panel library does not support transparency, so we must composite manually for transparent layers.

However, the cost is significant: we now own the compositing loop instead of delegating to `update_panels()`. This is a fundamental architecture change.

**Simpler alternative: Use panel library for opaque layers, add transparency only for specific layers.**
- Most layers (background, main content) are opaque -- panels handle these efficiently
- Only popup/overlay layers need transparency
- For transparent layers: before `update_panels()`, manually copy non-blank cells from the transparent layer to the layer below it

**Expected developer API:**
```c
TUI_Layer* popup = tui_layer_create("popup", 10, 5, 40, 10);
tui_layer_set_transparent(popup, true);  // enable transparency

// Drawing to the layer works normally:
TUI_DrawContext ctx = tui_layer_get_draw_context(popup);
tui_draw_fill_rect(&ctx, inner_rect, ' ', bg_style);  // opaque region
tui_draw_border_rect(&ctx, border_rect, TUI_BORDER_ROUNDED, style);
// Cells NOT drawn to remain transparent (show layer below)
```

**Complexity:** High (compositing logic, sentinel cell detection, interaction with panel library)
**Dependencies:** Existing TUI_Layer, frame pipeline

#### T-2: Transparent Background (Layer-Wide)

**Why expected:** The most common transparency use case: a layer whose background is transparent, but drawn content is opaque. Think: a floating label with no background box.

**Implementation:** Set the layer's WINDOW background to a sentinel, then during compositing, skip cells that match the sentinel.

**Complexity:** Medium (subset of T-1)
**Dependencies:** T-1

### Differentiators

#### T-3: Alpha Blending (Color Mixing)

**Why expected by advanced UIs:** Semi-transparent overlays where the overlay color blends with the background color. Example: a 50% transparent dark overlay dims the content behind it.

**How it works in terminals:** Terminals have no native alpha channel. Alpha blending must be computed at the application level:
```c
// For each overlapping cell:
result_r = (overlay_r * alpha + base_r * (1.0 - alpha));
result_g = (overlay_g * alpha + base_g * (1.0 - alpha));
result_b = (overlay_b * alpha + base_b * (1.0 - alpha));
// Write result color to the compositing buffer
```

**Requirements:** Requires knowing the RGB color of the cell below. This means:
1. True color mode (C-1) -- cannot blend 256-color indices meaningfully
2. Application-level compositing (T-1 Approach C) -- must read base layer colors
3. Per-cell color storage in both layers

**Complexity:** High (color extraction from ncurses cells, RGB arithmetic, dual-layer read)
**Dependencies:** T-1, C-1

#### T-4: Character-Preserving Transparency

**Why expected for text overlays:** When overlaying text on a background, preserve the background character but change colors. Example: syntax highlighting overlay that tints background cells.

**How it works:** During compositing, if overlay cell has only a color/attribute change (no character), apply the attribute to the base cell's character.

**Complexity:** Medium-High
**Dependencies:** T-1

### Anti-Features (Transparency)

#### TA-1: GPU-Style Blend Modes

Do NOT implement multiply, screen, overlay, or other Photoshop-style blend modes. Only source-over (alpha blend) and binary (on/off) transparency. Terminal rendering does not benefit from complex blend modes.

#### TA-2: Per-Cell Alpha Channel in TUI_Style

Do NOT add an alpha field to TUI_Style. Transparency is a LAYER property, not a per-draw-call property. Adding alpha to every style struct creates confusion about when it applies (answer: only during compositing, not during drawing).

Instead: `tui_layer_set_alpha(layer, 0.5f)` sets layer-wide alpha.

#### TA-3: Terminal Compositor Integration

Do NOT attempt to use the terminal emulator's own transparency features (e.g., WezTerm background alpha, Kitty's protocol). These are terminal-specific and affect the entire terminal window, not individual layers.

---

## Feature Dependencies

```
                     C-1 (True Color)
                    /        \
            C-2 (Fallback)   T-3 (Alpha Blending)
                              |
M-1 (Click) ----> M-3 (Hit Test)     T-1 (Binary Transparency)
    |                                  |
M-2 (Scroll)                    T-2 (Transparent BG)
    |
M-4 (Drag) ----> M-5 (Hover)

S-1 (Half-Block) ----> S-2 (Quadrant) ----> S-3 (Braille)
                                              |
                                        S-4 (Sextant)

D-1 (Dirty Tracking) ----> D-2 (Layer Dirty Rects)
    |                            |
D-3 (Frame Skip)           D-4 (Sync Output)
```

**Inter-feature dependencies:**
- T-3 (Alpha Blending) requires C-1 (True Color) -- cannot blend palette indices
- T-1 (Binary Transparency) requires D-1 (Damage Tracking) awareness -- transparent cells must not trigger dirty marking
- All features are independent at the table-stakes level and can be built in parallel

## MVP Recommendation

**Phase priority for v1.1 (suggested order):**

1. **Mouse Input (M-1, M-2, M-3)** -- Unlocks interactivity. Scroll wheel is required for cels-clay scroll containers. Relatively isolated change (extends tui_input.c).

2. **True Color (C-1, C-2, C-3)** -- Visual fidelity improvement. Small change (extends tui_color.c). No architectural impact.

3. **Sub-Cell Rendering (S-1, S-2)** -- New drawing primitives. Additive (new functions, no changes to existing code). Half-block is the high-value target; quadrant is a natural extension.

4. **Damage Tracking (D-1, D-2)** -- Performance optimization. Requires careful integration with frame pipeline. Should come after the new drawing features are stable.

5. **Layer Transparency (T-1, T-2)** -- Architectural change to compositing. Most complex. Should come last because it may require rethinking the panel-based architecture.

**Defer to post-v1.1:**
- M-4, M-5 (Drag, Hover): Nice-to-have but not required for cels-clay
- S-3, S-4 (Braille, Sextant): Specialized, not needed for UI rendering
- T-3 (Alpha Blending): Very complex, requires true color, compositing buffer
- D-4 (Synchronized Output): Low-effort but terminal-specific

---
*Features research: 2026-02-20*
*Supersedes v1.0 features research (2026-02-07) for deferred features*
