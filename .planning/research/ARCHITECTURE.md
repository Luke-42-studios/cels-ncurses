# Architecture Research: cels-ncurses Graphics API

**Research Date:** 2026-02-07
**Confidence:** MEDIUM-HIGH
**Domain:** Terminal graphics API architecture

## Key Finding

The architecture follows a well-established pattern shared by notcurses (planes/cells/channels), termbox2 (cell buffer/present), and Clay renderers (command array iteration): all converge on a layered model with explicit drawing context, compositing layers, and a clear frame pipeline.

## Components (6)

### 1. TUI_DrawContext — Drawing State

Per-layer state struct passed to all draw calls:
- `WINDOW* target` — the ncurses window to draw into
- Scissor stack (array of clip rects)
- Position offsets (for nested drawing)
- Current style reference

### 2. TUI_Style + Color System (`tui_color.h/c`)

Value-type style struct replacing stateful `attron`/`attroff`:
- Foreground color, background color
- Attributes (bold, dim, underline, reverse)
- Dynamic color pair registration (pool-based allocation)
- Replaces hardcoded `CP_*` constants

### 3. Drawing Primitives (`tui_draw.h/c`)

Core draw functions, all taking a `TUI_DrawContext*`:
- `tui_draw_cell(ctx, x, y, ch, style)` — single cell
- `tui_draw_rect(ctx, rect, style)` — filled rectangle
- `tui_draw_text(ctx, x, y, text, style)` — positioned text
- `tui_draw_text_n(ctx, x, y, text, max_len, style)` — bounded text (critical for Clay)
- `tui_draw_border(ctx, rect, border_style)` — box-drawing border
- `tui_draw_border_ex(ctx, rect, per_side_config)` — per-side border control
- `tui_draw_hline(ctx, x, y, len, ch, style)` — horizontal line
- `tui_draw_vline(ctx, x, y, len, ch, style)` — vertical line
- `tui_draw_fill(ctx, rect, ch, style)` — fill region with character
- `tui_push_scissor(ctx, rect)` — push clip region
- `tui_pop_scissor(ctx)` — pop clip region

### 4. Layer Manager (`tui_layer.h/c`)

Thin wrapper over ncurses PANEL library:
- `tui_layer_create(name, x, y, w, h)` — create named layer
- `tui_layer_destroy(layer)` — remove layer
- `tui_layer_show/hide(layer)` — toggle visibility
- `tui_layer_raise/lower(layer)` — reorder in z-stack
- `tui_layer_move(layer, x, y)` — reposition
- `tui_layer_resize(layer, w, h)` — resize (uses `wresize` + `replace_panel`)
- `tui_layer_get_context(layer)` — get `TUI_DrawContext` for drawing

### 5. Frame Pipeline (`tui_frame.h/c`)

Frame lifecycle management:
- `tui_frame_begin()` — erase all layer contents, reset state
- `tui_frame_end()` — call `update_panels()` (composites all panels with z-order)
- `doupdate()` called by existing window frame loop after `tui_frame_end()`

### 6. Graphics Provider (refactored `tui_renderer.c`)

CELS feature registration only — no app-specific rendering:
- `CEL_DefineFeature(Renderable)` registration
- Provides the frame pipeline hooks
- Consumer apps register their own providers for their components

## Data Flow

```
Consumer draw calls (with DrawContext)
  → Drawing primitives apply scissor check + style
    → Cell writes to ncurses WINDOW (via w* functions)
      → tui_frame_end() calls update_panels()
        → Window frame loop calls doupdate()
          → Terminal output
```

## Build Order (5 Phases)

### Phase 1: Foundation
- `TUI_Style` struct and style helpers
- `TUI_Rect` struct (x, y, w, h in cell units)
- `TUI_DrawContext` struct
- Color pair management (dynamic pool)
- **No CELS integration needed — can test with plain ncurses**

### Phase 2: Drawing Core
Build in dependency order:
1. Scissor/clipping (derwin-based)
2. Single cell drawing
3. Shapes (rect filled/outlined, hline, vline, fill)
4. Text (positioned, bounded)
5. Borders (box-drawing chars, single/double/rounded, per-side)
- **Each builds on previous — can test incrementally**

### Phase 3: Layer System
- Layer manager (PANEL-backed)
- Frame pipeline (begin/end)
- Add `#include <panel.h>` and link `-lpanelw`
- **Requires CMake change**

### Phase 4: Integration
- Refactor renderer provider to use new graphics API
- Update window/engine modules for frame pipeline
- Wire `update_panels()` into frame loop (replaces `wnoutrefresh(stdscr)`)
- **Riskiest phase — changes the rendering path**

### Phase 5: Migration
- Extract app-specific code (Canvas, Button, Slider rendering) to example app
- Remove `tui_components.h` from module
- Example app uses new graphics API for its rendering
- **Must happen last — example needs complete API**

## New File Structure

**New files:**
- `include/cels-ncurses/tui_draw.h` — drawing primitives API
- `include/cels-ncurses/tui_layer.h` — layer manager API
- `include/cels-ncurses/tui_color.h` — color/style system API
- `include/cels-ncurses/tui_frame.h` — frame pipeline API
- `include/cels-ncurses/tui_graphics.h` — convenience header (includes all above)
- `src/graphics/tui_draw.c` — primitives implementation
- `src/graphics/tui_layer.c` — layer manager implementation
- `src/graphics/tui_color.c` — color system implementation
- `src/graphics/tui_frame.c` — frame pipeline implementation

**Refactored:**
- `src/renderer/tui_renderer.c` — stripped to CELS feature registration only

**Removed from module (moved to example):**
- `include/cels-ncurses/tui_components.h` — app-specific widget renderers

## Clay Renderer Mapping

| Clay Command | cels-ncurses Primitive |
|-------------|----------------------|
| `CLAY_RENDER_COMMAND_TYPE_RECTANGLE` | `tui_draw_rect()` + `tui_draw_border()` |
| `CLAY_RENDER_COMMAND_TYPE_TEXT` | `tui_draw_text_n()` |
| `CLAY_RENDER_COMMAND_TYPE_BORDER` | `tui_draw_border_ex()` |
| `CLAY_RENDER_COMMAND_TYPE_SCISSOR_START` | `tui_push_scissor()` |
| `CLAY_RENDER_COMMAND_TYPE_SCISSOR_END` | `tui_pop_scissor()` |
| `CLAY_RENDER_COMMAND_TYPE_IMAGE` | Not applicable in terminal |

## Open Questions

- INTERFACE library model means `static` globals in new `.c` files are per-translation-unit — verify layer manager's global state works correctly
- ncurses PANEL transparency handling (overlapping panels with empty cells) may need testing
- Wide-character text measurement for Clay integration needs `wcswidth()` or similar

---
*Architecture research: 2026-02-07*
