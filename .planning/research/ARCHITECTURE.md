# Architecture Research: cels-ncurses v1.1 Enhanced Rendering

**Research Date:** 2026-02-20
**Confidence:** HIGH (verified against installed ncurses 6.6 headers and existing codebase)
**Domain:** Integration of mouse input, sub-cell rendering, true color, damage tracking, and layer transparency into panel-backed TUI renderer
**Previous:** v1.0 architecture research (2026-02-07) -- all 5 phases complete, 39 requirements verified

## Executive Summary

The five v1.1 features vary dramatically in how they interact with the existing architecture. Mouse input and sub-cell rendering are **additive** -- they introduce new APIs without modifying existing ones. True color requires a **surgical modification** to `tui_color.c` (the color resolution path). Damage tracking requires **instrumenting** the draw primitives to record what changed. Layer transparency is the most **invasive** -- it fundamentally conflicts with how ncurses panels composite, requiring either a custom compositing pass or abandoning panel-managed compositing for transparent layers.

The critical architectural insight: **ncurses panels have no transparency concept**. The panel library composites by painting opaque windows in z-order. Transparency requires either (a) reading cell content from lower layers and manually blending before writing to the top-most visible window, or (b) using ncurses `overlay()` which treats spaces as transparent -- but only for full-window copies, not per-cell alpha.

## Existing Architecture (v1.0 Baseline)

```
                     ECS Frame Loop
                          |
    OnLoad: TUI_InputSystem (wgetch -> CELS_Input)
                          |
    PreStore: TUI_FrameBeginSystem
        - werase dirty+visible layers
        - update timing (delta_time, fps)
        - block SIGWINCH
                          |
    OnStore: App renderer systems
        - tui_layer_get_draw_context(layer) -> TUI_DrawContext
        - tui_draw_* primitives write to WINDOW* via ctx->clip
        - tui_push/pop_scissor for nested clipping
                          |
    PostFrame: TUI_FrameEndSystem
        - update_panels() + doupdate()
        - unblock SIGWINCH
```

### Key Structs and Their Roles

| Struct | Location | Role | v1.1 Impact |
|--------|----------|------|-------------|
| `TUI_DrawContext` | tui_draw_context.h | Wraps WINDOW* + clip rect | Unchanged for mouse/sub-cell; damage tracking adds dirty-rect accumulator |
| `TUI_Color` | tui_color.h | Wraps xterm-256 index (.index field) | **Must change**: add RGB storage for true color path |
| `TUI_Style` | tui_color.h | fg + bg TUI_Color + attrs | Unchanged (colors carry their own resolution) |
| `TUI_Layer` | tui_layer.h | PANEL* + WINDOW* + metadata | Add opacity/transparency flag for layer transparency |
| `TUI_FrameState` | tui_frame.h | Timing + lifecycle | Add damage rect accumulator for damage tracking |
| `TUI_CellRect` | tui_types.h | Integer cell coordinates | Sub-cell rendering introduces sub-cell coordinate types alongside |
| `CELS_Input` | backend.h (legacy) | Backend-agnostic input | **Must extend** with mouse fields (or add TUI_MouseEvent) |

## Feature 1: Mouse Input

### Architecture Impact: ADDITIVE (no existing API changes)

Mouse input integrates cleanly because it flows through the same `wgetch(stdscr)` path that keyboard input uses. When `mousemask()` is enabled, `wgetch()` returns `KEY_MOUSE` and the event is retrieved via `getmouse()`.

### Data Flow

```
wgetch(stdscr) returns KEY_MOUSE
    |
getmouse(&event)  -> MEVENT { .x, .y, .z, .bstate }
    |
Map to CELS_Input mouse fields (screen coordinates)
    |
Optional: wmouse_trafo(layer->win, &y, &x, FALSE)
    -> converts screen coords to layer-local coords
    |
Optional: wenclose(layer->win, y, x)
    -> hit-test which layer contains the point
```

### ncurses API (verified in /usr/include/curses.h, ncurses 6.6)

```c
// Enable mouse events (call once at startup)
mmask_t mousemask(mmask_t newmask, mmask_t *oldmask);

// Retrieve mouse event after KEY_MOUSE from wgetch
int getmouse(MEVENT *event);

// MEVENT struct
typedef struct {
    short id;       // device ID (usually 0)
    int x, y, z;    // screen-relative character-cell coordinates
    mmask_t bstate; // button state bitmask
} MEVENT;

// Hit test: is screen point (y,x) inside window?
bool wenclose(const WINDOW *, int y, int x);

// Coordinate transform: screen <-> window-local
bool wmouse_trafo(const WINDOW *, int *y, int *x, bool to_screen);
```

**Mouse events available (NCURSES_MOUSE_VERSION=2 on this system):**
- BUTTON1-5: RELEASED, PRESSED, CLICKED, DOUBLE_CLICKED, TRIPLE_CLICKED
- BUTTON4/5: scroll wheel (up/down) -- these are the standard scroll events
- REPORT_MOUSE_POSITION: motion tracking (hover)
- ALL_MOUSE_EVENTS: convenience mask

### Integration Point: tui_input.c

The change is surgical: in `TUI_Input_use()`, add `mousemask()` call. In `tui_read_input_ncurses()`, add a `KEY_MOUSE` case that calls `getmouse()` and maps to CELS_Input fields.

**CELS_Input extension options:**

Option A: Extend CELS_Input struct (currently 44 bytes, fits cache line).
```c
// Add to CELS_Input:
int mouse_x, mouse_y;        // screen coordinates (+8 bytes)
int mouse_button;             // which button (0=none, 1-5) (+4 bytes)
bool mouse_pressed;           // press event this frame
bool mouse_released;          // release event this frame
bool mouse_clicked;           // click event this frame
bool mouse_scroll_up;         // scroll up
bool mouse_scroll_down;       // scroll down
bool mouse_motion;            // position changed
// Total: +14 bytes -> 58 bytes (still under 64-byte cache line)
```

Option B: Separate TUI_MouseEvent struct (keeps CELS_Input backend-agnostic).

**Recommendation: Option A** because CELS_Input is already backend-specific in practice (the struct definition lives in the backend module's include path, and the input system is ncurses-specific). Adding mouse fields keeps the single-struct-per-frame pattern. However, this requires modifying `CELS_Input` which lives in the cels core repo's legacy backend.h. If that's not feasible, use a separate `TUI_MouseState` global with extern linkage (same pattern as `g_frame_state`).

### Layer Hit Testing

ncurses provides `wenclose()` and `wmouse_trafo()` which work with PANEL-managed windows. To determine which layer a click targets:

```c
// Iterate layers top-to-bottom (reverse z-order)
for (int i = g_layer_count - 1; i >= 0; i--) {
    if (!g_layers[i].visible) continue;
    if (wenclose(g_layers[i].win, mouse_y, mouse_x)) {
        // This layer contains the click
        // Convert to layer-local coords:
        int local_y = mouse_y, local_x = mouse_x;
        wmouse_trafo(g_layers[i].win, &local_y, &local_x, FALSE);
        break;
    }
}
```

**Caveat:** `wenclose()` checks the window's bounding rectangle, not visibility through the panel stack. If a higher layer occludes a lower one, `wenclose()` still returns true for the lower layer. True z-aware hit testing requires iterating from top to bottom and stopping at the first hit.

### Startup Configuration

```c
// In TUI_Input_use() or tui_hook_startup():
mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
mouseinterval(0);  // Disable click detection delay for responsiveness
```

**REPORT_MOUSE_POSITION caveat:** Enabling motion tracking generates many events per frame (every cell the mouse crosses). This can flood `wgetch()`. The input system already reads only one key per frame -- motion events would queue and lag. Two options:
1. Drain all mouse events per frame (loop until non-mouse or ERR)
2. Only enable motion tracking on demand (e.g., during drag)

**Recommendation:** Drain loop for mouse events, keeping only the last position. This matches the existing KEY_RESIZE drain pattern in the codebase.

### Existing Code Changes Required

| File | Change | Scope |
|------|--------|-------|
| `src/input/tui_input.c` | Add KEY_MOUSE case, getmouse(), field mapping | ~30 lines |
| `src/input/tui_input.c` | Add mousemask() in TUI_Input_use() | ~3 lines |
| `src/window/tui_window.c` | None (mouse events come through wgetch on stdscr) | None |
| CELS_Input or new TUI_MouseState | Add mouse fields | New fields or new struct |

### New APIs

```c
// Optional convenience functions (new header: tui_mouse.h)
TUI_Layer* tui_mouse_hit_test(int screen_x, int screen_y);
void tui_mouse_to_layer_coords(TUI_Layer* layer, int screen_x, int screen_y,
                                int* local_x, int* local_y);
```

## Feature 2: Sub-Cell Rendering

### Architecture Impact: ADDITIVE (new draw functions, no existing API changes)

Sub-cell rendering uses Unicode characters that divide a terminal cell into sub-regions. This is purely a new set of drawing primitives that use the existing `TUI_DrawContext` and `TUI_Style` infrastructure.

### Unicode Block Elements (U+2580-U+259F)

| Technique | Resolution | Characters | Use Case |
|-----------|-----------|------------|----------|
| Half-blocks | 1x2 per cell | Upper half (U+2580), Lower half (U+2584), Full (U+2588) | Smooth vertical gradients, 2x vertical resolution |
| Quadrants | 2x2 per cell | 16 combinations (U+2596-U+259F) + halves | Simple pixel art, icons |
| Sextants | 2x3 per cell | 64 combinations (U+1FB00-U+1FB3B) | Higher-res graphics |
| Braille | 2x4 per cell | 256 patterns (U+2800-U+28FF) | Plotting, graphs, highest text-mode resolution |

### Half-Block Color Trick

The most impactful sub-cell technique: use the **upper half block** (U+2580) with foreground = top color, background = bottom color. This gives 2x vertical resolution for colored rectangles.

```c
// Each cell renders TWO vertical pixels:
// Foreground color = top half
// Background color = bottom half
TUI_Style half_style = {
    .fg = top_pixel_color,    // upper half block uses fg
    .bg = bottom_pixel_color, // lower half is background
    .attrs = TUI_ATTR_NORMAL
};
// Print U+2580 (UPPER HALF BLOCK) at position
```

This is the technique that makes terminal UIs look dramatically better -- smooth gradients, anti-aliased edges, high-resolution color blocks.

### Braille Coordinate System

Braille patterns encode an 8-dot grid (2 columns x 4 rows) per cell:

```
Dot positions:    Bit values:
  1  4              0x01  0x08
  2  5              0x02  0x10
  3  6              0x04  0x20
  7  8              0x40  0x80

codepoint = 0x2800 + (bit pattern)
```

### New Types

```c
// Sub-cell coordinate (2x vertical for half-blocks, 2x4 for braille)
typedef struct TUI_SubCellPoint {
    int x, y;  // Sub-cell coordinates (resolution depends on mode)
} TUI_SubCellPoint;

typedef enum TUI_SubCellMode {
    TUI_SUBCELL_HALF_BLOCK,  // 1x2 per cell (most useful)
    TUI_SUBCELL_QUADRANT,    // 2x2 per cell
    TUI_SUBCELL_BRAILLE,     // 2x4 per cell (monochrome per cell)
} TUI_SubCellMode;
```

### New Draw Functions

```c
// Half-block pixel drawing (2x vertical resolution)
void tui_draw_halfblock_pixel(TUI_DrawContext* ctx, int cell_x, int cell_y,
                               TUI_Color top_color, TUI_Color bottom_color);

// Sub-cell rectangle fill (uses half-blocks for smooth edges)
void tui_draw_subcell_rect(TUI_DrawContext* ctx, TUI_CellRect rect,
                            TUI_Color color, TUI_SubCellMode mode);

// Braille canvas: set individual dots within cells
void tui_draw_braille_set(TUI_DrawContext* ctx, int sub_x, int sub_y, bool on);
void tui_draw_braille_line(TUI_DrawContext* ctx, int x0, int y0, int x1, int y1);
void tui_draw_braille_flush(TUI_DrawContext* ctx, TUI_Style style);
```

### Braille Canvas Buffer

Braille rendering needs a staging buffer because each cell encodes 8 dots, and multiple `set` calls accumulate into the same cell:

```c
typedef struct TUI_BrailleCanvas {
    int cell_width, cell_height;   // Size in cells
    uint8_t* dots;                 // Bit patterns, one byte per cell
    bool dirty;
} TUI_BrailleCanvas;
```

### Integration with Existing Architecture

- Half-block rendering uses `mvwadd_wch()` (already used by border drawing) with `setcchar()` for the block characters
- Braille uses the same wide-character path
- All sub-cell draw functions take `TUI_DrawContext*` and clip against `ctx->clip`
- No modification to existing draw functions needed
- Style application uses existing `tui_style_apply()` (half-block trick requires per-cell style changes)

### Existing Code Changes Required

| File | Change | Scope |
|------|--------|-------|
| Existing files | **None** | Zero changes to existing code |
| New: `src/graphics/tui_subcell.c` | Half-block + braille implementations | New file |
| New: `include/cels-ncurses/tui_subcell.h` | Sub-cell API declarations | New file |
| `CMakeLists.txt` | Add new source file | 1 line |

## Feature 3: True Color (24-bit RGB)

### Architecture Impact: MODIFICATION of tui_color.c color resolution path

The current system eagerly maps RGB to xterm-256 at creation time (`tui_color_rgb()` -> `rgb_to_nearest_256()`). True color requires **deferring** the mapping or bypassing it entirely.

### ncurses Direct Color Support

**System status (verified):**
- ncurses 6.6 installed with `NCURSES_EXT_COLORS` enabled
- `NCURSES_RGB_COLORS` is **disabled** (compiled as 0)
- `init_extended_pair()` and `init_extended_color()` are available
- `alloc_pair()` is available (already used)

**What NCURSES_RGB_COLORS=0 means:** The ncurses library was NOT compiled to treat color indices as packed RGB values. `init_color()` uses the traditional 0-1000 range per channel to redefine palette entries. We cannot pass `0xFF0000` to `alloc_pair()` and expect red.

### True Color Strategy

There are three approaches, ordered by complexity:

**Approach A: init_color() palette redefinition (MEDIUM confidence)**
If `can_change_color()` returns true, use `init_color(index, r*1000/255, g*1000/255, b*1000/255)` to redefine palette entries. Combined with `alloc_pair()`, this gives true 24-bit color by dynamically reprogramming the terminal palette.

- Pros: Works within ncurses, no escape sequence hacking
- Cons: Limited by `COLORS` and `COLOR_PAIRS` counts; palette is shared; can break default colors
- **This is the most practical approach for this system**

**Approach B: Direct escape sequences (bypass ncurses)**
Write `\033[38;2;R;G;Bm` / `\033[48;2;R;G;Bm` directly to the terminal, bypassing ncurses color pair system entirely.

- Pros: Unlimited colors, works regardless of ncurses compile flags
- Cons: Fights ncurses -- ncurses tracks color state internally and will overwrite our escape sequences on refresh. Requires careful coordination with `putp()` or writing to the WINDOW's internal buffer.
- **Not recommended** because it conflicts with the panel compositing model

**Approach C: Recompile ncurses with NCURSES_RGB_COLORS**
Rebuild ncurses with `--enable-direct-color` so color indices are packed RGB.

- Pros: cleanest integration
- Cons: requires custom ncurses build, not portable

**Recommendation: Approach A** (`init_color()` + `can_change_color()` + `alloc_pair()`).

### TUI_Color Struct Modification

```c
typedef struct TUI_Color {
    int index;      // -1 = default, 0-255 = xterm-256 (current behavior)
    uint8_t r, g, b; // Original RGB (new -- stored for true color path)
    bool is_rgb;     // true = use RGB values, false = use index (new)
} TUI_Color;
```

**Size impact:** Current TUI_Color is 4 bytes (int). New would be 8 bytes (int + 3 bytes + bool + padding). TUI_Style goes from 12 bytes to 20 bytes. This is still stack-allocated, passed by value -- acceptable.

### Color Resolution Path

```c
TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    if (true_color_available()) {
        // Allocate a palette slot and redefine it
        int slot = allocate_color_slot();
        init_color(slot, r * 1000 / 255, g * 1000 / 255, b * 1000 / 255);
        return (TUI_Color){ .index = slot, .r = r, .g = g, .b = b, .is_rgb = true };
    } else {
        // Fallback: existing xterm-256 nearest-match
        return (TUI_Color){ .index = rgb_to_nearest_256(r, g, b),
                            .r = r, .g = g, .b = b, .is_rgb = false };
    }
}
```

### Color Slot Management

`init_color()` modifies palette entries in-place. With a 256-color terminal, slots 0-15 are system colors (should not touch), 16-231 are the color cube, 232-255 are greyscale. We can safely redefine 16-255 for true color.

With `COLORS` typically being 256, that gives 240 simultaneously active true colors. If the terminal supports more (some report 32768 or higher), we get more slots.

**LRU eviction:** The existing `alloc_pair()` already does LRU pair eviction. For color slots, implement a similar allocator:

```c
#define TUI_TRUE_COLOR_SLOT_MIN 16
#define TUI_TRUE_COLOR_SLOT_MAX (COLORS - 1)

static int g_next_color_slot = TUI_TRUE_COLOR_SLOT_MIN;
static uint32_t g_color_slot_rgb[256]; // cached RGB for dedup
```

### Deduplication

Before allocating a new slot, check if the exact RGB already has a slot:
```c
uint32_t packed = (r << 16) | (g << 8) | b;
for (int i = TUI_TRUE_COLOR_SLOT_MIN; i <= g_next_color_slot; i++) {
    if (g_color_slot_rgb[i] == packed) return i; // reuse
}
```

### Existing Code Changes Required

| File | Change | Scope |
|------|--------|-------|
| `include/cels-ncurses/tui_color.h` | Extend TUI_Color struct with r,g,b,is_rgb | Struct change |
| `src/graphics/tui_color.c` | Add true color path in tui_color_rgb() | ~40 lines |
| `src/graphics/tui_color.c` | Add color slot allocator | ~30 lines |
| `src/window/tui_window.c` | Detect can_change_color() at startup | ~5 lines |
| Callers of tui_color_rgb() | **None** -- API unchanged | Zero |

### Risk: TUI_Color size change

TUI_Color is embedded in TUI_Style which is passed by value throughout the codebase. Changing its size from 4 to 8 bytes doubles TUI_Style from 12 to 20 bytes. All existing code will **still work** -- C passes structs by value regardless of size. The only risk is binary compatibility if any consumer cached sizeof(TUI_Color), which no code in this codebase does.

## Feature 4: Damage Tracking

### Architecture Impact: INSTRUMENTATION of draw primitives + frame pipeline

Damage tracking answers: "which cells changed this frame?" Currently, `tui_frame_begin()` calls `werase()` on ALL dirty layers, and `tui_frame_end()` calls `update_panels() + doupdate()` which lets ncurses figure out what changed. The current approach works but has two inefficiencies:

1. **Layer-level granularity:** A layer is either entirely dirty or entirely clean. One draw call marks the entire layer dirty, causing full-layer werase.
2. **ncurses internal tracking is opaque:** ncurses tracks changed lines internally (via `is_linetouched()`) but we cannot influence or read this efficiently.

### What "Damage Tracking" Means Here

Two interpretations:

**Interpretation A: Application-level damage tracking.**
Track which `TUI_CellRect` regions were drawn to this frame, skip `werase()` for untouched regions, and only clear what was previously drawn but is no longer drawn.

**Interpretation B: ncurses-level optimization.**
Use `wtouchln()` to precisely mark which lines need refresh instead of `werase()` on entire windows.

**Recommendation: Interpretation A** -- application-level dirty rectangles. This gives the most control and enables future optimizations like partial-layer redraw.

### Dirty Rectangle Accumulator

```c
#define TUI_DAMAGE_RECT_MAX 64

typedef struct TUI_DamageTracker {
    TUI_CellRect rects[TUI_DAMAGE_RECT_MAX];
    int count;
    bool full_dirty;  // If true, entire layer needs redraw (overflow fallback)
} TUI_DamageTracker;
```

### Integration with Draw Primitives

Every draw function already computes a `visible` rect (intersection of draw area with clip). Record this:

```c
// In tui_draw_fill_rect():
TUI_CellRect visible = tui_cell_rect_intersect(rect, ctx->clip);
if (visible.w <= 0 || visible.h <= 0) return;
tui_damage_record(ctx, visible);  // NEW: record damage
// ... existing draw code ...
```

### Integration with Frame Pipeline

```c
void tui_frame_begin(void) {
    // Instead of werase(entire layer):
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].dirty && g_layers[i].visible) {
            if (g_layers[i].damage.full_dirty) {
                werase(g_layers[i].win);  // Fallback: full clear
            } else {
                // Clear only previously-damaged regions
                for (int r = 0; r < g_layers[i].damage.count; r++) {
                    clear_rect(g_layers[i].win, g_layers[i].damage.rects[r]);
                }
            }
            g_layers[i].damage = (TUI_DamageTracker){0};  // Reset for new frame
        }
    }
}
```

### Per-Layer Damage State

Add `TUI_DamageTracker` to `TUI_Layer`:

```c
typedef struct TUI_Layer {
    // ... existing fields ...
    TUI_DamageTracker prev_damage;  // What was drawn last frame (need to clear)
    TUI_DamageTracker curr_damage;  // What is being drawn this frame
} TUI_Layer;
```

### Double-Buffer Pattern

Frame N-1's damage tells us what to clear at the start of frame N. Frame N's damage accumulates during draw calls. At frame end, swap:

```
frame_begin:
    clear(prev_damage regions)  // erase what was drawn last frame

draw phase:
    curr_damage accumulates

frame_end:
    prev_damage = curr_damage   // remember for next frame
    curr_damage = empty
    update_panels() + doupdate()
```

### Existing Code Changes Required

| File | Change | Scope |
|------|--------|-------|
| `include/cels-ncurses/tui_layer.h` | Add TUI_DamageTracker to TUI_Layer | Struct addition |
| `src/frame/tui_frame.c` | Replace werase() with damage-aware clearing | ~20 lines modified |
| `src/graphics/tui_draw.c` | Add tui_damage_record() call in each draw function | ~1 line per function (6 functions) |
| New: `include/cels-ncurses/tui_damage.h` | TUI_DamageTracker type + API | New header |
| New: `src/graphics/tui_damage.c` | Damage accumulation + rect merging | New file |

### Open Question: Damage Rect Merging

With many small draw calls, the damage list can fill up quickly. Options:
1. **Union bounding box:** Merge overlapping/adjacent rects into their union (reduces count, increases area)
2. **Grid-based:** Divide screen into NxM tiles, track dirty tiles (fixed memory, O(1) per draw)
3. **Overflow to full-dirty:** If count > MAX, set `full_dirty = true` (simplest)

**Recommendation:** Start with option 3 (overflow to full-dirty) with MAX=64. Most frames have fewer than 64 distinct draw regions. Optimize later if profiling shows waste.

## Feature 5: Layer Transparency

### Architecture Impact: INVASIVE -- conflicts with ncurses panel compositing

This is the hardest feature. ncurses panels are fundamentally opaque: `update_panels()` paints each panel window onto the virtual screen in z-order, with each pixel overwriting whatever was below it.

### The Problem

When a panel window contains a space character (blank), that space is still painted opaque with the window's background color. There is no concept of "this cell is transparent, show whatever is underneath."

### ncurses Built-in "Transparency"

**`overlay(src, dst)`**: Copies non-blank characters from src to dst (blanks ARE transparent). This is the closest ncurses gets to transparency.

**`overwrite(src, dst)`**: Copies ALL characters from src to dst (no transparency).

**`copywin(src, dst, ...)`**: Like overlay/overwrite but for sub-regions.

The v1.0 architecture explicitly removed `overwrite(stdscr)` bridging. Reintroducing overlay-based compositing would work but has limitations:

1. `overlay()` treats ANY space character as transparent -- you cannot have an intentional opaque space
2. `overlay()` operates on the full window overlap region (no sub-rect control)
3. `overlay()` is window-to-window, not panel-aware (ignores z-order)

### Transparency Approaches

**Approach A: Marker-character transparency**
Use a sentinel character (e.g., NUL or a private-use Unicode codepoint) to mark transparent cells. Before `update_panels()`, run a custom compositing pass that reads cell content from each layer and writes the composite result to a staging window.

```c
// Custom compositing (replaces update_panels for transparent layers):
for each screen cell (x, y):
    iterate layers top-to-bottom:
        read cell via mvwin_wch(layer->win, y, x, &cch)
        if cell is not transparent marker:
            write to composite buffer
            break
    write composite cell to output
```

- Pros: Full per-cell transparency control
- Cons: O(cells * layers) per frame, bypasses panel optimization, complex

**Approach B: overlay()-based compositing**
Use `overlay()` to merge visible layers bottom-to-top onto the background layer's window. Spaces in upper layers show through to lower layers.

```c
// In tui_frame_end(), before update_panels():
for (int i = 0; i < g_layer_count; i++) {
    if (g_layers[i].visible && g_layers[i].transparent) {
        // overlay copies non-blank chars from layer onto background
        overlay(g_layers[i].win, background_win);
    }
}
```

- Pros: Uses ncurses built-in, simple
- Cons: Space = transparent (no opaque spaces), full-window copy, layers must overlap correctly

**Approach C: Per-layer opacity flag with selective panel management**
Transparent layers are removed from the panel stack. Their content is manually composited onto the layer below them using `overlay()` or cell-by-cell reading.

```c
typedef struct TUI_Layer {
    // ... existing fields ...
    bool transparent;  // If true, NOT in panel stack; composited manually
} TUI_Layer;
```

- Pros: Non-transparent layers keep full panel performance, only transparent layers have overhead
- Cons: Mixing panel-managed and manual layers adds complexity

**Recommendation: Approach C** -- opt-in per-layer transparency with manual compositing for transparent layers only. Most layers (background, main UI) are opaque. Only specific layers (tooltips, overlays, HUDs) need transparency.

### Cell Reading for Compositing

ncurses provides `mvwin_wch(WINDOW*, y, x, cchar_t*)` to read a wide character from a window position. This is verified available in the installed headers. Reading is O(1) per cell.

### Transparent Layer Workflow

```
tui_layer_create("tooltip", x, y, w, h);
tui_layer_set_transparent(tooltip, true);
// This removes tooltip from panel stack (hide_panel)
// but keeps the WINDOW* alive for drawing

// During frame_end:
// 1. update_panels() composites opaque layers normally
// 2. For each transparent layer (bottom-to-top):
//    - For each cell in the transparent layer:
//      - Read cell via mvwin_wch
//      - If cell is not blank/marker: write to screen via mvwadd_wch on stdscr
//    - (OR use overlay() for simpler but less controlled compositing)
```

### Existing Code Changes Required

| File | Change | Scope |
|------|--------|-------|
| `include/cels-ncurses/tui_layer.h` | Add `bool transparent` to TUI_Layer | Struct addition |
| `src/layer/tui_layer.c` | Add tui_layer_set_transparent() | ~15 lines |
| `src/frame/tui_frame.c` | Add transparent layer compositing pass after update_panels() | ~40 lines |
| `src/frame/tui_frame.c` | Modified tui_frame_end() flow | ~20 lines modified |

### Performance Concern

Full-screen transparent layer compositing reads every cell: 80x24 = 1,920 reads per transparent layer per frame. At 60fps, that is 115,200 reads/sec per transparent layer. This is within ncurses performance budget (ncurses itself does similar work internally), but stacking multiple transparent layers multiplies the cost.

## Component Boundary Map

```
+--------------------------------------------------+
|  tui_input.c                                      |
|  [Mouse: mousemask, KEY_MOUSE, getmouse, wenclose]|
|  -> CELS_Input (extended with mouse fields)        |
+--------------------------------------------------+
        |
        v (screen coords from mouse event)
+--------------------------------------------------+
|  tui_mouse.h (NEW)                                |
|  [Hit testing: tui_mouse_hit_test()]              |
|  [Coord transform: tui_mouse_to_layer_coords()]  |
|  Iterates g_layers, uses wenclose/wmouse_trafo    |
+--------------------------------------------------+
        |
        v (layer-local coords)
+--------------------------------------------------+
|  tui_layer.h/.c                                   |
|  [Existing: create, destroy, show, hide, z-order] |
|  [NEW: transparent flag, damage tracker]          |
|  -> tui_layer_get_draw_context() (unchanged)      |
+--------------------------------------------------+
        |
        v (TUI_DrawContext with WINDOW*)
+--------------------------------------------------+
|  tui_draw.h/.c                                    |
|  [Existing: fill_rect, border_rect, text, etc.]   |
|  [MODIFIED: each draw call records damage rect]   |
|  [Uses tui_style_apply -> tui_color.c]            |
+--------------------------------------------------+
        |                       |
        v                       v
+------------------+  +-------------------------+
| tui_subcell.h/.c |  | tui_color.h/.c          |
| (NEW)            |  | [MODIFIED: TUI_Color    |
| half-block,      |  |  struct adds r,g,b]     |
| braille canvas   |  | [NEW: true color path   |
| Uses same        |  |  via init_color()]      |
| DrawContext+Style |  +-------------------------+
+------------------+
        |
        v (all drawing complete)
+--------------------------------------------------+
|  tui_frame.h/.c                                   |
|  [frame_begin: damage-aware clearing]             |
|  [frame_end: update_panels + transparent compose] |
|  [NEW: tui_damage.h for TUI_DamageTracker]       |
+--------------------------------------------------+
        |
        v
    Terminal output (doupdate)
```

## Dependency Graph Between Features

```
                    Mouse Input
                        |
                (independent, no deps)

    Sub-Cell Rendering          True Color
         |                         |
    (independent)            (independent,
                              but sub-cell benefits
                              from true color for
                              half-block coloring)

            Damage Tracking
                 |
         (depends on draw primitive
          instrumentation; benefits
          all other features)

            Layer Transparency
                 |
         (depends on frame pipeline;
          benefits from damage tracking
          to avoid re-compositing
          unchanged transparent layers)
```

### Build Order (Recommended)

| Order | Feature | Rationale |
|-------|---------|-----------|
| 1 | **Mouse Input** | Completely independent, smallest surface area, highest user-facing value. Unblocks interactive features. |
| 2 | **True Color** | Independent, modifies only tui_color.c. Must be done before sub-cell rendering to get full benefit of half-block color trick. |
| 3 | **Sub-Cell Rendering** | Independent but benefits from true color (half-block technique uses 2 colors per cell -- true color makes this much more powerful). |
| 4 | **Damage Tracking** | Instruments draw primitives (must be done after sub-cell adds new draw functions to instrument). Required foundation for efficient transparency. |
| 5 | **Layer Transparency** | Most invasive, benefits from damage tracking (skip re-compositing unchanged transparent layers). Build last because it modifies the frame pipeline. |

### Alternative Ordering Rationale

Damage tracking could theoretically come first since it is foundational. However, it requires touching every draw function, and the sub-cell draw functions do not exist yet. Building sub-cell first means damage tracking instruments all draw functions in one pass rather than being retrofitted when sub-cell is added.

## Anti-Patterns to Avoid

### Anti-Pattern 1: Direct Terminal Escape Sequences
**What:** Writing `\033[38;2;R;G;Bm` directly for true color, bypassing ncurses.
**Why bad:** ncurses tracks terminal state internally. Direct writes desync ncurses state, causing corruption on the next `doupdate()`. Panel compositing breaks.
**Instead:** Use `init_color()` + `alloc_pair()` within ncurses's color system.

### Anti-Pattern 2: Full-Screen Transparency Compositing
**What:** Reading and writing every cell every frame for transparency.
**Why bad:** O(COLS * LINES * transparent_layers) per frame, most of which is unchanged.
**Instead:** Use damage tracking to only re-composite changed cells. Use overlay() for full-window cases.

### Anti-Pattern 3: Mouse Motion Without Drain
**What:** Enabling REPORT_MOUSE_POSITION and reading one event per frame.
**Why bad:** Mouse generates many events per second. They queue in the input buffer and the cursor lags behind the actual mouse position.
**Instead:** Drain all pending mouse events per frame, keep only the latest position.

### Anti-Pattern 4: Modifying CELS_Input in cels core
**What:** Adding ncurses-specific mouse fields to the framework-level input struct.
**Why bad:** CELS_Input is designed to be backend-agnostic. Adding mouse coordinates couples it to pointing-device backends.
**Instead:** Add TUI_MouseState as a module-level global (same extern pattern as g_frame_state), or extend CELS_Input only with backend-agnostic mouse fields (position + button state, not ncurses-specific bstate).

### Anti-Pattern 5: Per-Draw-Call init_color()
**What:** Calling `init_color()` inside every `tui_style_apply()`.
**Why bad:** `init_color()` sends escape sequences to the terminal. Calling it 500 times per frame (once per draw) floods the terminal with palette updates.
**Instead:** Deduplicate and cache. Allocate color slots eagerly at `tui_color_rgb()` time, not at style-apply time. Use a hash map or linear scan with ~240 slots.

## Scalability Considerations

| Feature | At 1 layer | At 10 layers | At 32 layers (max) |
|---------|-----------|-------------|-------------------|
| Mouse hit test | O(1) | O(10) | O(32) -- fine |
| Sub-cell rendering | Same as draw | Same as draw | Same as draw |
| True color | ~240 colors | Same | Same (shared palette) |
| Damage tracking | 64 rects/layer | 640 total | 2048 total -- fine |
| Transparency | N/A | 1-2 transparent | Performance concern if >3 transparent |

## Sources

- ncurses 6.6 header: `/usr/include/curses.h` (verified directly, NCURSES_VERSION "6.6", NCURSES_EXT_COLORS enabled, NCURSES_RGB_COLORS=0, NCURSES_MOUSE_VERSION=2)
- [ncurses mouse man page](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- MEVENT struct, mousemask, getmouse
- [ncurses color man page](https://man7.org/linux/man-pages/man3/curs_color.3x.html) -- init_color, init_extended_pair, alloc_pair
- [True color terminal support](https://gist.github.com/sindresorhus/bed863fb8bedf023b833c88c322e44f9) -- 24-bit color terminal compatibility
- [Chad Austin: 24-bit color in terminals](https://chadaustin.me/2024/01/truecolor-terminal-emacs/) -- practical true color implementation
- [ncurses direct color discussion](https://lists.gnu.org/archive/html/bug-ncurses/2018-04/msg00011.html) -- how to use init_extended_pair with direct color
- [Drawille (braille terminal graphics)](https://github.com/asciimoo/drawille) -- braille canvas pattern
- [Block Elements Unicode block](https://grokipedia.com/page/Block_Elements) -- U+2580-U+259F reference
- [ncurses transparency techniques](https://hoop.dev/blog/ncurses-processing-transparency-without-hacks/) -- overlay/overwrite patterns
- [TLDP ncurses mouse tutorial](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html) -- mouse input basics
- Existing codebase: all source files in `src/` and `include/` read and analyzed

---
*Architecture research for v1.1 Enhanced Rendering: 2026-02-20*
