# Phase 3: Layer System - Research

**Researched:** 2026-02-08
**Domain:** ncurses panel library, z-ordered layer management, terminal resize handling
**Confidence:** HIGH

## Summary

The ncurses panel library (`<panel.h>`, `-lpanelw`) provides native z-ordered window compositing that maps directly to the layer system requirements. Panels stack above stdscr (which acts as the implicit bottom layer), with `update_panels()` handling all the `wnoutrefresh()` calls needed to resolve overlaps before `doupdate()` flushes to the physical screen.

The key integration challenge is the INTERFACE library global state problem: the layer manager needs a single global array/registry of layers, but INTERFACE library sources compile in the consumer's translation unit. The existing codebase already solves this pattern with `extern` declarations in headers and definitions in `.c` files (see `TUI_WindowState` in `tui_window.h`/`tui_window.c`). The layer manager should follow this same pattern.

The highest-risk aspect identified in STATE.md -- "stdscr-to-panels migration must be complete, no partial adoption" -- is confirmed by official documentation: you must NOT mix `wrefresh()`/`wnoutrefresh()` with panel-managed windows. All drawing must go through panel-backed windows, with `update_panels()` + `doupdate()` replacing the current `wnoutrefresh(stdscr)` + `doupdate()` pattern.

**Primary recommendation:** Use ncurses panel library with `wresize()` + `replace_panel()` for resize, `move_panel()` for repositioning, `set_panel_userptr()` for layer metadata, and `update_panels()` + `doupdate()` as the sole screen update path. The layer manager global state must use `extern` declarations with single definitions in a dedicated `.c` file.

## Standard Stack

The established libraries/tools for this domain:

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses panel library | 6.5 (system) | Z-ordered window compositing | Native ncurses extension, designed exactly for this purpose |
| `<panel.h>` | (header) | Panel API declarations | Standard ncurses panel header |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `wresize()` | ncurses 6.5 | Resize a WINDOW in-place | Panel resize (wresize + replace_panel) |
| `set_panel_userptr()` / `panel_userptr()` | ncurses 6.5 | Associate metadata with panel | Store TUI_Layer pointer for reverse lookup |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Panel library z-order | Manual wnoutrefresh ordering | Panels handle overlap resolution automatically; manual is error-prone |
| wresize + replace_panel | delwin + newwin + replace_panel | wresize is simpler and preserves window state; delwin/newwin loses content |
| move_panel | mvwin | move_panel integrates with panel stack management; mvwin bypasses it |

**Build system change required:**
```cmake
# panel library is NOT in CURSES_LIBRARIES -- must add explicitly
find_library(PANEL_LIBRARY NAMES panelw panel)
target_link_libraries(cels-ncurses INTERFACE
    ${CURSES_LIBRARIES}
    ${PANEL_LIBRARY}   # <-- NEW: panel library for layer system
    cels
    m
)
```

**Verification:** On this system, `pkg-config --libs panelw` returns `-lpanelw` and the library exists at `/usr/lib/libpanelw.so`. The header is at `/usr/include/panel.h`.

## Architecture Patterns

### Recommended Project Structure
```
src/
  layer/
    tui_layer.c          # Layer manager implementation (global state, create/destroy, operations)
include/cels-ncurses/
    tui_layer.h          # TUI_Layer struct, TUI_LayerManager API (extern declarations)
```

### Pattern 1: Layer Struct with Panel Backing
**What:** Each layer is a struct wrapping a PANEL* and its associated WINDOW*, with metadata (name, position, dimensions, z-order hint, visibility).
**When to use:** Always -- this is the core data structure.
**Example:**
```c
// Source: ncurses panel.3x man page + codebase conventions
typedef struct TUI_Layer {
    char name[64];          // Human-readable identifier
    PANEL* panel;           // Owned -- del_panel on destroy
    WINDOW* win;            // Owned -- delwin on destroy
    int x, y;               // Position (redundant with panel, but convenient)
    int width, height;      // Dimensions
    bool visible;           // Track visibility state
} TUI_Layer;
```

### Pattern 2: Global Layer Manager with Extern
**What:** A fixed-capacity array of layer pointers as global state, with extern declarations in the header and definitions in a single `.c` file.
**When to use:** Required by INTERFACE library pattern -- static globals would be per-TU.
**Example:**
```c
// tui_layer.h
#define TUI_LAYER_MAX 32

extern TUI_Layer g_layers[TUI_LAYER_MAX];
extern int g_layer_count;

// tui_layer.c
TUI_Layer g_layers[TUI_LAYER_MAX];
int g_layer_count = 0;
```

**Why this works:** The `.c` file is listed in `target_sources(cels-ncurses INTERFACE ...)`, so it compiles in the consumer's single translation unit. Since there is only ONE consumer binary, static or extern both result in a single instance. However, `extern` is the explicit, correct pattern that would survive if the library type changed.

### Pattern 3: Panel Userptr for Reverse Lookup
**What:** Store a pointer to the TUI_Layer struct in the panel's userptr so you can go from PANEL* back to TUI_Layer*.
**When to use:** When iterating the panel stack via panel_above/panel_below.
**Example:**
```c
// Source: ncurses panel HOWTO
TUI_Layer* layer = &g_layers[g_layer_count];
layer->win = newwin(h, w, y, x);
layer->panel = new_panel(layer->win);
set_panel_userptr(layer->panel, layer);

// Later, to get layer from panel:
TUI_Layer* layer = (TUI_Layer*)panel_userptr(some_panel);
```

### Pattern 4: Layer-to-DrawContext Bridge
**What:** Create a TUI_DrawContext from a layer's WINDOW, using (0, 0) as origin and the layer's dimensions.
**When to use:** When drawing into a layer with Phase 2 primitives.
**Example:**
```c
// Source: existing tui_draw_context.h API
TUI_DrawContext tui_layer_get_draw_context(TUI_Layer* layer) {
    return tui_draw_context_create(layer->win, 0, 0, layer->width, layer->height);
}
```

**Key insight:** Draw coordinates within a layer are LOCAL to the layer's window (0,0 is top-left of the layer, not the screen). This is because each panel has its own WINDOW with its own coordinate system.

### Pattern 5: Frame-Loop Integration with update_panels
**What:** Replace the current `wnoutrefresh(stdscr) + doupdate()` with `update_panels() + doupdate()`.
**When to use:** In the frame loop (tui_window.c tui_window_frame_loop).
**Example:**
```c
// Current (Phase 2):
while (g_running) {
    Engine_Progress(delta);
    doupdate();
    usleep(...);
}

// Phase 3 (with panels):
while (g_running) {
    Engine_Progress(delta);
    update_panels();   // <-- replaces all wnoutrefresh calls
    doupdate();        // flush to physical screen
    usleep(...);
}
```

### Pattern 6: Terminal Resize Handling
**What:** On KEY_RESIZE detection, resize stdscr, then wresize + replace_panel for each layer, then update WindowState.
**When to use:** When terminal size changes (SIGWINCH -> KEY_RESIZE).
**Example:**
```c
// Source: ncurses resizeterm.3x man page
case KEY_RESIZE:
    // stdscr is already resized by ncurses internally
    // Update each layer:
    for (int i = 0; i < g_layer_count; i++) {
        TUI_Layer* layer = &g_layers[i];
        // Application-specific: decide new size for each layer
        // e.g., full-screen layers resize to new LINES/COLS
        wresize(layer->win, new_h, new_w);
        replace_panel(layer->panel, layer->win);
        layer->width = new_w;
        layer->height = new_h;
    }
    // Update window state and notify observers
    TUI_WindowState.width = COLS;
    TUI_WindowState.height = LINES;
    TUI_WindowState.state = WINDOW_STATE_RESIZING;
    cels_state_notify_change(TUI_WindowStateID);
    break;
```

### Anti-Patterns to Avoid
- **Drawing to stdscr when panels are active:** Causes visual corruption. ALL drawing must go through panel-backed WINDOWs. The only exception is that stdscr serves as the implicit background layer (below all panels), but you should not call `wnoutrefresh(stdscr)` when using panels.
- **Using wrefresh() on panel windows:** Use `update_panels()` + `doupdate()` instead. `wrefresh()` bypasses panel stack ordering and produces visual corruption.
- **Using mvwin() on panel windows:** Use `move_panel()` instead. `mvwin()` bypasses the panel library's position tracking.
- **Calling wnoutrefresh() on individual panel windows:** The `update_panels()` function handles all necessary `wnoutrefresh()` calls. Calling it manually disrupts the panel compositing.
- **Mixing static globals for manager state:** In an INTERFACE library, use `extern` with definitions in a single `.c` file -- not `static` globals in headers.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Z-order management | Custom linked list of windows | Panel library (top_panel, bottom_panel) | Panel library handles overlap resolution, partial visibility, and refresh ordering automatically |
| Window overlap detection | Manual rect intersection per frame | update_panels() | Native implementation handles all overlap computation internally |
| Window stacking refresh | Manual bottom-up wnoutrefresh loop | update_panels() + doupdate() | Panel library does this correctly; manual ordering is error-prone |
| Layer metadata storage | Separate hash map from panel to data | set_panel_userptr() / panel_userptr() | Built into the panel library, no allocation overhead |
| Panel repositioning | mvwin() wrapper | move_panel() | move_panel integrates with panel stack; mvwin does not |

**Key insight:** The panel library IS the layer system. The TUI_Layer struct is a thin wrapper that adds naming, typed metadata, and the DrawContext bridge. Do not reimplement panel functionality.

## Common Pitfalls

### Pitfall 1: Panel Library Not Linked
**What goes wrong:** Linker errors: `undefined reference to new_panel`, `undefined reference to update_panels`.
**Why it happens:** CMake's `find_package(Curses)` and `CURSES_LIBRARIES` do NOT include the panel library.
**How to avoid:** Explicitly find and link the panel library:
```cmake
find_library(PANEL_LIBRARY NAMES panelw panel)
target_link_libraries(... ${PANEL_LIBRARY})
```
**Warning signs:** Undefined reference errors mentioning any `*_panel` function.

### Pitfall 2: Mixing stdscr Drawing with Panels
**What goes wrong:** Visual corruption -- parts of panels flicker, disappear, or show through each other incorrectly.
**Why it happens:** `wnoutrefresh(stdscr)` or `wrefresh()` on panel windows bypasses the panel compositing logic. The current `tui_renderer.c` calls `wnoutrefresh(stdscr)` at the end of rendering -- this must be removed once panels are active.
**How to avoid:** ALL screen updates go through `update_panels()` + `doupdate()`. No direct `wrefresh()` or `wnoutrefresh()` on any panel-managed window.
**Warning signs:** Flickering, panels showing wrong content, visual artifacts when overlapping.

### Pitfall 3: wresize Without replace_panel
**What goes wrong:** Panel library does not know the window size changed, causing stale overlap calculations.
**Why it happens:** `wresize()` modifies the WINDOW but the PANEL tracks its own copy of dimensions.
**How to avoid:** Always call `replace_panel(pan, win)` after `wresize(win, ...)` -- even though the window pointer hasn't changed, this updates the panel's internal bookkeeping.
**Warning signs:** Resize appears to work but panel overlap/compositing is wrong after resize.

### Pitfall 4: Using mvwin Instead of move_panel
**What goes wrong:** Panel position tracking becomes inconsistent with actual window position.
**Why it happens:** `mvwin()` moves the window but does not update the panel library's internal position record.
**How to avoid:** Always use `move_panel(pan, y, x)` for panel windows. Never call `mvwin()` directly on a panel-backed WINDOW.
**Warning signs:** Panel drawn at wrong position, overlap detection fails.

### Pitfall 5: Forgetting to delwin After del_panel
**What goes wrong:** Memory leak -- `del_panel()` frees the PANEL structure but NOT the associated WINDOW.
**Why it happens:** Panel library does not own the window -- the caller created it with `newwin()` and must destroy it with `delwin()`.
**How to avoid:** Layer destroy must call `del_panel(layer->panel)` THEN `delwin(layer->win)`, in that order.
**Warning signs:** Gradual memory growth as layers are created and destroyed.

### Pitfall 6: KEY_RESIZE Not Detected Because wgetch Uses Wrong Window
**What goes wrong:** Terminal resize is not detected, layers are not resized.
**Why it happens:** `KEY_RESIZE` is delivered via `wgetch()`. The current code uses `wgetch(stdscr)` in `tui_input.c`. This should continue to work with panels since stdscr is still valid -- but the input window must have `keypad(win, TRUE)` enabled.
**How to avoid:** Keep using `wgetch(stdscr)` for input (stdscr remains valid as the background layer). Ensure `keypad(stdscr, TRUE)` stays set (already done in `tui_window_startup`).
**Warning signs:** Resize events not triggering.

### Pitfall 7: Resize Observer Notification Ordering
**What goes wrong:** Observer code tries to draw into a layer before it has been resized.
**Why it happens:** If `cels_state_notify_change(TUI_WindowStateID)` is called before all layers are resized, observer code may run with stale layer dimensions.
**How to avoid:** Resize ALL layers first, THEN notify observers of the size change.
**Warning signs:** Drawing corruption immediately after terminal resize.

### Pitfall 8: INTERFACE Library Static Globals for Layer Manager
**What goes wrong:** If layer manager state uses `static` globals in a header, each TU that includes it gets its own copy.
**Why it happens:** INTERFACE library sources compile in the consumer context. If multiple `.c` files included a header with `static` layer manager state, they'd each have independent copies.
**How to avoid:** Use `extern` declarations in header, single definition in `tui_layer.c`. This is the same pattern used by `TUI_WindowState` in the existing code.
**Warning signs:** Layer operations appear to have no effect, or different parts of the code see different layer states. In practice, since the consumer is a single binary that links all these `.c` files together, the risk is low -- but the pattern should be correct.

## Code Examples

Verified patterns from official sources:

### Layer Create
```c
// Source: ncurses panel.3x man page
TUI_Layer* tui_layer_create(const char* name, int x, int y, int w, int h) {
    if (g_layer_count >= TUI_LAYER_MAX) return NULL;

    TUI_Layer* layer = &g_layers[g_layer_count];
    strncpy(layer->name, name, sizeof(layer->name) - 1);
    layer->name[sizeof(layer->name) - 1] = '\0';

    layer->win = newwin(h, w, y, x);    // newwin(lines, cols, begin_y, begin_x)
    if (!layer->win) return NULL;

    layer->panel = new_panel(layer->win); // Places at top of stack
    if (!layer->panel) {
        delwin(layer->win);
        return NULL;
    }

    set_panel_userptr(layer->panel, layer);
    layer->x = x;
    layer->y = y;
    layer->width = w;
    layer->height = h;
    layer->visible = true;

    g_layer_count++;
    return layer;
}
```

### Layer Destroy
```c
// Source: ncurses panel.3x man page
void tui_layer_destroy(TUI_Layer* layer) {
    if (!layer || !layer->panel) return;

    del_panel(layer->panel);   // Remove from panel stack (does NOT free window)
    delwin(layer->win);        // Free the window
    layer->panel = NULL;
    layer->win = NULL;

    // Compact the array (swap with last, decrement count)
    int idx = (int)(layer - g_layers);
    if (idx < g_layer_count - 1) {
        g_layers[idx] = g_layers[g_layer_count - 1];
        // Update the moved layer's panel userptr to point to new location
        set_panel_userptr(g_layers[idx].panel, &g_layers[idx]);
    }
    g_layer_count--;
}
```

### Layer Show/Hide
```c
// Source: ncurses panel.3x man page
void tui_layer_show(TUI_Layer* layer) {
    show_panel(layer->panel);  // Places at top of stack + makes visible
    layer->visible = true;
}

void tui_layer_hide(TUI_Layer* layer) {
    hide_panel(layer->panel);  // Removes from stack (PANEL struct preserved)
    layer->visible = false;
}
```

### Layer Z-Order
```c
// Source: ncurses panel.3x man page
void tui_layer_raise(TUI_Layer* layer) {
    top_panel(layer->panel);   // Move to top of stack
}

void tui_layer_lower(TUI_Layer* layer) {
    bottom_panel(layer->panel); // Move to bottom of stack
}
```

### Layer Move
```c
// Source: ncurses panel.3x man page - "use this function, not mvwin"
void tui_layer_move(TUI_Layer* layer, int x, int y) {
    move_panel(layer->panel, y, x);  // Note: move_panel takes (panel, starty, startx)
    layer->x = x;
    layer->y = y;
}
```

### Layer Resize
```c
// Source: ncurses wresize.3x + panel.3x man pages
void tui_layer_resize(TUI_Layer* layer, int w, int h) {
    wresize(layer->win, h, w);              // wresize(win, lines, cols)
    replace_panel(layer->panel, layer->win); // Update panel's internal bookkeeping
    layer->width = w;
    layer->height = h;
}
```

### Layer-to-DrawContext Bridge
```c
// Source: existing tui_draw_context.h
TUI_DrawContext tui_layer_get_draw_context(TUI_Layer* layer) {
    // Coordinates are LOCAL to the layer window (0,0 is top-left of layer)
    return tui_draw_context_create(layer->win, 0, 0, layer->width, layer->height);
}
```

### Frame Loop with Panels (replacing current pattern)
```c
// Source: ncurses-intro.html + existing tui_window.c
static void tui_window_frame_loop(void) {
    // ... existing setup ...

    while (g_running) {
        Engine_Progress(delta);
        update_panels();    // Replaces individual wnoutrefresh calls
        doupdate();         // Flush to physical screen (already exists)
        usleep((unsigned int)(delta * 1000000));
    }

    // ... existing cleanup ...
}
```

### Terminal Resize Handler (in input system)
```c
// Source: ncurses resizeterm.3x man page
case KEY_RESIZE:
    // ncurses has already called resizeterm() internally,
    // which resized stdscr to new LINES x COLS.
    // Application must resize its own windows/panels:
    tui_layer_resize_all(COLS, LINES);  // Application-defined resize policy

    // Update window state
    TUI_WindowState.width = COLS;
    TUI_WindowState.height = LINES;
    TUI_WindowState.state = WINDOW_STATE_RESIZING;
    cels_state_notify_change(TUI_WindowStateID);
    // State will return to READY on next frame or after observer processing
    break;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Manual wnoutrefresh ordering | update_panels() + doupdate() | ncurses panel library (stable since 1995+) | Eliminates manual refresh ordering bugs |
| mvwin for window movement | move_panel for panel windows | ncurses panel library | Proper panel stack integration |
| delwin + newwin for resize | wresize + replace_panel | wresize available since ncurses 5.x | Simpler, preserves content |

**Deprecated/outdated:**
- Using `refresh()` or `wrefresh()` with panel-managed windows: must use `update_panels()` + `doupdate()`
- Using `mvwin()` on panel windows: must use `move_panel()`
- The existing `wnoutrefresh(stdscr)` call in `tui_renderer.c` (line 216): must be removed once panels are active

## Open Questions

Things that could not be fully resolved:

1. **Resize policy: who decides new layer dimensions?**
   - What we know: On KEY_RESIZE, all layers need new dimensions. Some layers should be full-screen, others may be fixed-size, others proportional.
   - What's unclear: Whether the layer manager should have a built-in resize policy (e.g., callback per layer) or if the application handles all resize logic via the RSZE observer system.
   - Recommendation: Keep it simple for Phase 3 -- provide `tui_layer_resize()` as a primitive, let the observer notification system (RSZE-03) inform application code which then calls resize on each layer. No built-in resize policy in the layer manager.

2. **stdscr as background layer: erase or leave alone?**
   - What we know: stdscr is implicitly below all panels. The panel library handles it as background.
   - What's unclear: Whether we need to `erase()` stdscr each frame (current code does this), or if panels cover it completely and it doesn't matter.
   - Recommendation: Continue calling `erase()` on stdscr at frame start to prevent stale content from showing through if layers don't cover the entire screen. This is safe and cheap.

3. **Layer capacity (TUI_LAYER_MAX)**
   - What we know: A fixed-capacity array is simplest and avoids malloc. 32 layers seems more than sufficient for a TUI.
   - What's unclear: Whether Clay UI layouts could generate more than 32 overlapping regions.
   - Recommendation: Start with 32. This can be bumped trivially if needed. Clay's scissor regions map to the scissor stack (already implemented), not to layers.

4. **Existing renderer migration timing**
   - What we know: `tui_renderer.c` currently draws to stdscr with `erase()` + `wnoutrefresh(stdscr)`. This must change to draw into a panel-backed layer instead.
   - What's unclear: Whether this migration happens in Phase 3 or is deferred to Phase 4/5.
   - Recommendation: Phase 3 should make the frame loop use `update_panels()` + `doupdate()` and provide the layer API. The existing renderer can be migrated incrementally -- it could draw into a full-screen layer as its first step.

## Sources

### Primary (HIGH confidence)
- [ncurses panel.3x man page (invisible-island.net, 2024-12-28)](https://invisible-island.net/ncurses/man/panel.3x.html) - Complete API reference for all panel functions
- [ncurses resizeterm.3x man page (invisible-island.net, 2025-07-05)](https://invisible-island.net/ncurses/man/resizeterm.3x.html) - Terminal resize handling, KEY_RESIZE mechanism
- [Writing Programs with NCURSES (invisible-island.net)](https://invisible-island.net/ncurses/ncurses-intro.html) - stdscr/panel relationship, refresh cycle, mixing warnings
- [ncurses wresize documentation](https://www.mkssoftware.com/docs/man3/wresize.3.asp) - wresize behavior and constraints
- System verification: `pkg-config --libs panelw` confirms `-lpanelw`, library at `/usr/lib/libpanelw.so`, header at `/usr/include/panel.h`

### Secondary (MEDIUM confidence)
- [NCURSES Programming HOWTO - Panel Library (tldp.org)](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/panels.html) - Usage patterns, examples, lifecycle management
- [CMake FindCurses documentation](https://cmake.org/cmake/help/latest/module/FindCurses.html) - Confirms CURSES_LIBRARIES does not include panel library

### Tertiary (LOW confidence)
- None -- all findings verified against official documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - ncurses panel library is well-documented and stable since 1995+; system verification confirms availability
- Architecture: HIGH - patterns derived from official man pages and verified against existing codebase conventions
- Pitfalls: HIGH - panel/stdscr mixing warnings come directly from official ncurses documentation; CMake linking verified on target system
- Resize handling: HIGH - KEY_RESIZE mechanism documented in official resizeterm man page; wresize + replace_panel pattern confirmed by official panel docs

**Research date:** 2026-02-08
**Valid until:** Indefinite -- ncurses panel API is stable and has not changed in decades
