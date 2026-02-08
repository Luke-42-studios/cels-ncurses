# Phase 5: Integration and Migration - Research

**Researched:** 2026-02-08
**Domain:** Code extraction, API migration, module boundary enforcement
**Confidence:** HIGH

## Summary

Phase 5 is a pure refactoring phase with no new library dependencies or technology to learn. The work is extracting app-specific code (widgets, renderer provider, color constants) from the cels-ncurses module into the example app, then migrating all drawing from legacy stdscr/mvprintw calls to the Phase 1-4 graphics API (DrawContext, TUI_Style, tui_draw_* primitives).

The codebase is well-understood from having built Phases 1-4. Every file that needs modification has been read and analyzed. The primary risk is ordering -- some removals depend on replacements being in place first, and the app must remain functional after each plan.

**Primary recommendation:** Structure plans around functional milestones: (1) create example app with widgets rewritten to use DrawContext, (2) rewire TUI_Engine to stop calling tui_renderer_init, (3) remove module files. Each plan produces a compilable, runnable app.

## Standard Stack

No new libraries. This phase uses only what Phases 1-4 established:

### Core (already in project)
| Library | Version | Purpose | Status |
|---------|---------|---------|--------|
| ncurses (wide) | system | Terminal drawing backend | Established |
| panel | system | Layer z-order compositing | Established |
| CELS framework | 0.1.0 | ECS, Feature/Provides pattern | Established |
| flecs | 4.1.4 | ECS engine | Established |

### Graphics API (Phases 1-4, used for migration)
| Component | Header | Purpose |
|-----------|--------|---------|
| TUI_Style | tui_color.h | fg/bg color + attrs (replaces CP_* + attron/attroff) |
| TUI_DrawContext | tui_draw_context.h | Target for all draw calls (replaces stdscr) |
| tui_draw_text() | tui_draw.h | Positioned text with style (replaces mvprintw + attron) |
| tui_draw_fill_rect() | tui_draw.h | Filled rectangles (replaces manual mvhline loops) |
| tui_draw_border_rect() | tui_draw.h | Box-drawing borders (replaces manual +---+ strings) |
| tui_draw_hline() | tui_draw.h | Horizontal lines |
| tui_layer_get_draw_context() | tui_layer.h | Get DrawContext from a layer (auto-marks dirty) |
| tui_frame_get_background() | tui_frame.h | Get the background layer |
| TUI_Layer | tui_layer.h | Named panel-backed layers |

## Architecture Patterns

### Pattern 1: Widget Extraction File Structure

Per the CONTEXT.md decision, each widget is a self-contained header file in the example app.

```
examples/
  app.c                    # Entry point, compositions, state, input systems
  components.h             # ECS component definitions (Text, Canvas, etc.)
  widgets/
    theme.h                # Shared TUI_Style constants with semantic names
    canvas.h               # Canvas header renderer (border + centered title)
    button.h               # Button renderer (selected/unselected states)
    slider.h               # Cycle slider + toggle slider renderers
    hint.h                 # Controls hint text renderer
    info_box.h             # Info box renderer (title + version)
  render_provider.h        # App-level _CEL_DefineFeature + _CEL_Provides + render callback
```

**Why header-only for widgets:** The existing tui_components.h is already header-only (all static inline). The widget files are small (each ~20-50 lines of rendering code). Static inline avoids cross-TU linkage issues inherent in the INTERFACE library pattern. Since the example app is a single translation unit (app.c), there is zero code duplication cost.

### Pattern 2: Widget Render Function Signature

Each widget render function takes explicit coordinates and a DrawContext pointer. No global row tracking (the old `g_render_row` pattern is replaced by caller-managed layout).

```c
// Source: CONTEXT.md decision -- explicit (x, y) coordinates
static inline void widget_button_render(TUI_DrawContext* ctx,
                                         int x, int y,
                                         const char* label,
                                         bool selected) {
    if (selected) {
        TUI_Style style = theme_highlight;
        style.attrs |= TUI_ATTR_BOLD;
        tui_draw_text(ctx, x, y, "> ", style);
        tui_draw_text(ctx, x + 2, y, label, style);
        tui_draw_text(ctx, x + 2 + (int)strlen(label) + 1, y, "<", style);
    } else {
        tui_draw_text(ctx, x + 2, y, label, theme_muted);
    }
}
```

### Pattern 3: Theme Constants

Shared theme.h with semantic style names. Uses tui_color_rgb() for colors (Phase 1 API) and TUI_Style structs.

```c
// Source: CONTEXT.md decision -- semantic style names
#ifndef THEME_H
#define THEME_H

#include <cels-ncurses/tui_color.h>

// Computed at first use (tui_color_rgb does eager xterm-256 mapping)
static inline TUI_Style theme_get_highlight(void) {
    return (TUI_Style){ .fg = tui_color_rgb(230, 200, 50), .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
}
// ... etc for each semantic name

// Alternatively, if called from a single TU, simple static const-like pattern works:
// static TUI_Style theme_highlight = { ... };
// BUT: tui_color_rgb() is NOT constexpr, so these must be initialized at runtime.

#endif
```

**Important detail:** TUI_Style values containing TUI_Color fields cannot be compile-time constants because `tui_color_rgb()` performs runtime computation (RGB-to-xterm-256 nearest-color mapping). Theme styles must be initialized at runtime, either via:
- Inline getter functions (static inline TUI_Style theme_get_X())
- A `theme_init()` function that populates static variables
- Lazy initialization with a static bool flag

**Recommendation:** Use a `theme_init()` function called once from the render callback setup. This is the simplest pattern and matches how color initialization works (after ncurses init, before first draw).

### Pattern 4: App-Level Render Provider

The app registers its own CELS render provider using the same _CEL_DefineFeature / _CEL_Provides / _CEL_ProviderConsumes macros. This is the same pattern currently in tui_renderer.c, just moved to the app.

```c
// In render_provider.h (app-level)
#include <cels/cels.h>
#include <cels-ncurses/tui_frame.h>
#include <cels-ncurses/tui_layer.h>
#include <cels-ncurses/tui_draw.h>
#include "components.h"
#include "widgets/theme.h"
#include "widgets/canvas.h"
#include "widgets/button.h"
// ... etc

_CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnStore, .priority = 0);

static void app_render_screen(CELS_Iter* it) {
    // Get background layer and DrawContext
    TUI_Layer* bg = tui_frame_get_background();
    if (!bg) return;
    TUI_DrawContext ctx = tui_layer_get_draw_context(bg);

    // Use widget render functions with DrawContext
    // ... all drawing via tui_draw_* through ctx
}

static inline void app_renderer_init(void) {
    _CEL_Feature(Canvas, Renderable);
    _CEL_Provides(TUI, Renderable, Canvas, app_render_screen);
    _CEL_ProviderConsumes(Text, ClickArea, Selectable, Range, GraphicsSettings);
}
```

### Pattern 5: TUI_Engine Module Modification

TUI_Engine_use() currently calls `tui_renderer_init()`. After Phase 5, this call is removed. The app calls `app_renderer_init()` itself.

```c
// tui_engine.c AFTER Phase 5 -- tui_renderer_init() line REMOVED
_CEL_DefineModule(TUI_Engine) {
    CEL_Use(TUI_Window, ...);
    CEL_Use(TUI_Input);
    // tui_renderer_init(); -- REMOVED
    tui_frame_init();
    tui_frame_register_systems();
    // ... root function call
}
```

The app's Build block calls its own renderer init:

```c
CEL_Build(App) {
    TUI_Engine_use((TUI_EngineConfig){
        .title = "CELS Demo",
        .version = "1.0.0",
        .fps = 60,
        .root = AppUI
    });
    app_renderer_init();  // App-level provider registration
    InitConfig();
}
```

### Pattern 6: stdscr Debug Guard

Per CONTEXT.md, a runtime assert in debug builds should catch stdscr writes after initialization. Since `mvprintw()`, `printw()`, `mvaddstr()`, and similar functions are ncurses library functions (not macros that can be easily overridden), the practical approach is:

**Recommended: Flag-based guard in frame pipeline.**

```c
// In tui_frame.c or a new tui_debug.h
#ifndef NDEBUG
static bool g_stdscr_writes_forbidden = false;

static inline void tui_debug_forbid_stdscr(void) {
    g_stdscr_writes_forbidden = true;
}

// Called at end of tui_window_startup(), after ncurses init
// but before frame loop starts
#endif
```

The guard cannot intercept actual ncurses calls to stdscr without LD_PRELOAD or link-time wrapping, which is heavyweight. Instead, the practical approach is:

1. Remove all stdscr write calls from the codebase during migration
2. Add a code-level assertion that documents the invariant
3. Use grep/static analysis as the enforcement mechanism

**Rationale:** The codebase is small enough (13 source files) that static analysis (grep for `mvprintw\|printw\|mvaddstr\|addstr\|mvaddch\|addch` without a WINDOW* argument) is sufficient. A runtime interception mechanism via LD_PRELOAD or `--wrap` linker flag would be overkill for a project this size.

**Simpler alternative:** Add a comment-based contract at the top of relevant files:

```c
/* INVARIANT: No code in this module calls mvprintw(), printw(), addstr(),
 * addch(), or any ncurses function that implicitly targets stdscr.
 * All drawing goes through TUI_DrawContext via tui_draw_* functions.
 * Violation: grep -rn 'mvprintw\|printw\|addstr\|addch' src/ */
```

### Anti-Patterns to Avoid

- **Incremental migration within a single file:** Do not try to have half-migrated widgets (some using mvprintw, some using DrawContext) coexisting in the same render callback. Migrate all widgets at once in a single plan.
- **Removing module files before app has replacements:** Do not delete tui_renderer.c/tui_components.h until the app compiles and runs with its own render provider.
- **Using attron/attroff in new widget code:** The Phase 1 decision is to use tui_style_apply() / TUI_Style exclusively. Never use the old ncurses attron/attroff/COLOR_PAIR pattern in migrated widgets.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Color pair allocation | Manual init_pair() with hardcoded indices | tui_color_rgb() + tui_style_apply() (uses alloc_pair internally) | alloc_pair handles LRU eviction; init_pair requires manual index management |
| Text positioning with color | mvprintw + attron/attroff sequences | tui_draw_text(ctx, x, y, text, style) | Atomic style application, no state leaks, clipping built in |
| Box borders | Manual "+---+" string concatenation | tui_draw_border_rect(ctx, rect, border_style, style) | Proper Unicode box-drawing chars, per-side control, clipping |
| Screen clearing | werase(stdscr) | tui_frame_begin() handles dirty layer clearing automatically | Retained-mode; only dirty layers are cleared |
| Screen compositing | wnoutrefresh + doupdate | tui_frame_end() handles all compositing | Panel library handles overlap resolution |

## Common Pitfalls

### Pitfall 1: TUI_Style Runtime Initialization
**What goes wrong:** Trying to use compile-time initialization for TUI_Style containing tui_color_rgb() calls.
**Why it happens:** tui_color_rgb() is a function call, not a compile-time constant. C99 does not support non-constant initializers for file-scope variables.
**How to avoid:** Use a theme_init() function called once after ncurses initialization, or use inline getter functions.
**Warning signs:** Compiler errors about "initializer element is not constant".

### Pitfall 2: Forgetting to Remove overwrite() Bridge
**What goes wrong:** If overwrite(stdscr, bg->win) is left in place but widgets no longer write to stdscr, the bridge copies an empty/stale stdscr over the background layer's content, erasing all DrawContext-based drawing.
**Why it happens:** The bridge was a Phase 4 compatibility shim. If it stays, it actively conflicts with the new drawing path.
**How to avoid:** Remove the overwrite() call and the werase(stdscr) call in the same commit as migrating widgets to DrawContext.
**Warning signs:** Blank screen despite correct DrawContext drawing code.

### Pitfall 3: init_pair() Conflicts with alloc_pair()
**What goes wrong:** The hardcoded init_pair() calls in tui_window.c (lines 95-100) pre-allocate pair indices 1-6. The Phase 1 color system uses alloc_pair() for dynamic allocation. If init_pair pairs overlap with alloc_pair allocations, color corruption occurs.
**Why it happens:** init_pair() and alloc_pair() share the same pair number space. init_pair() with explicit indices can collide.
**How to avoid:** Remove all init_pair() calls from tui_window.c when migrating to the Phase 1 color system. The alloc_pair() system manages its own pairs.
**Warning signs:** Wrong colors on some widgets.

### Pitfall 4: TUI_Engine Header Still Including tui_renderer.h
**What goes wrong:** tui_engine.h includes tui_renderer.h. If tui_renderer.h is removed from the module but tui_engine.h still references it, compilation fails.
**Why it happens:** Include chain not updated when files are removed.
**How to avoid:** Remove the #include from tui_engine.h in the same plan that removes the renderer files.
**Warning signs:** Compilation error: "tui_renderer.h: No such file or directory".

### Pitfall 5: CELS Feature/Provider Registration Order
**What goes wrong:** If the app calls app_renderer_init() before TUI_Engine_use(), the CELS context may not be ready, causing NULL pointer access in _CEL_DefineFeature internals.
**Why it happens:** The CELS macros require the ECS world to be initialized, which happens during TUI_Engine_use().
**How to avoid:** Call app_renderer_init() AFTER TUI_Engine_use() in the CEL_Build block.
**Warning signs:** Segfault during startup at provider registration.

### Pitfall 6: CMakeLists.txt Source List Not Updated
**What goes wrong:** Removing tui_renderer.c from the source tree but forgetting to remove it from CMakeLists.txt target_sources() causes build failure.
**Why it happens:** CMake source lists are manual.
**How to avoid:** Update CMakeLists.txt in the same commit that removes source files.
**Warning signs:** "No such file or directory" during cmake build.

## Code Examples

### Migrating a Button Widget from mvprintw to DrawContext

**Before (tui_components.h -- legacy):**
```c
static inline void tui_render_button(const char* label, bool selected) {
    if (selected) {
        attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvprintw(g_render_row, 2, "> %-20s <", label);
        attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
    } else {
        mvprintw(g_render_row, 4, "%-20s", label);
    }
    g_render_row++;
}
```

**After (widgets/button.h -- new):**
```c
#include <cels-ncurses/tui_draw.h>
#include "theme.h"

static inline void widget_button_render(TUI_DrawContext* ctx,
                                         int x, int y,
                                         const char* label,
                                         bool selected) {
    if (selected) {
        char buf[64];
        snprintf(buf, sizeof(buf), "> %-20s <", label);
        tui_draw_text(ctx, x, y, buf, theme_highlight);
    } else {
        tui_draw_text(ctx, x + 2, y, label, theme_normal);
    }
}
```

### Migrating Canvas Header from mvprintw to DrawContext

**Before:**
```c
mvprintw(g_render_row++, 0, "+-----------------------------------------+");
attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
// ... manual character-by-character
```

**After:**
```c
static inline void widget_canvas_render(TUI_DrawContext* ctx,
                                         int x, int y,
                                         const char* title) {
    int width = 43;
    TUI_CellRect box = { x, y, width, 3 };
    tui_draw_border_rect(ctx, box, TUI_BORDER_SINGLE, theme_normal);

    // Center title within box
    int title_len = (int)strlen(title);
    int title_x = x + 1 + (width - 2 - title_len) / 2;
    tui_draw_text(ctx, title_x, y + 1, title, theme_accent);
}
```

### App-Level Render Provider Registration

```c
// In render_provider.h
_CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnStore, .priority = 0);

static void app_render_screen(CELS_Iter* it) {
    (void)it;

    TUI_Layer* bg = tui_frame_get_background();
    if (!bg) return;
    TUI_DrawContext ctx = tui_layer_get_draw_context(bg);

    CELS_Context* cels_ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(cels_ctx);
    if (!world) return;

    // Query entities and render widgets using DrawContext
    // ... widget_canvas_render(&ctx, 0, 1, title);
    // ... widget_button_render(&ctx, 0, row, label, selected);
}

static inline void app_renderer_init(void) {
    _CEL_Feature(Canvas, Renderable);
    _CEL_Provides(TUI, Renderable, Canvas, app_render_screen);
    _CEL_ProviderConsumes(Text, ClickArea, Selectable, Range, GraphicsSettings);
}
```

### Theme Initialization Pattern

```c
// theme.h
#ifndef THEME_H
#define THEME_H

#include <cels-ncurses/tui_color.h>

// Module-scope styles initialized by theme_init()
static TUI_Style theme_highlight;
static TUI_Style theme_active;
static TUI_Style theme_inactive;
static TUI_Style theme_muted;
static TUI_Style theme_accent;
static TUI_Style theme_normal;

static inline void theme_init(void) {
    theme_highlight = (TUI_Style){ .fg = tui_color_rgb(230, 200, 50), .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
    theme_active    = (TUI_Style){ .fg = tui_color_rgb(50, 200, 80),  .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
    theme_inactive  = (TUI_Style){ .fg = tui_color_rgb(220, 50, 50),  .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
    theme_muted     = (TUI_Style){ .fg = TUI_COLOR_DEFAULT,           .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_DIM };
    theme_accent    = (TUI_Style){ .fg = tui_color_rgb(80, 220, 220), .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
    theme_normal    = (TUI_Style){ .fg = tui_color_rgb(255, 255, 255),.bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_NORMAL };
}

#endif
```

## Inventory of Changes

Complete list of what changes, file by file:

### Files REMOVED from module
| File | What It Contains | Where It Goes |
|------|-----------------|---------------|
| `src/renderer/tui_renderer.c` | Render provider + callback | `examples/render_provider.h` (rewritten) |
| `include/cels-ncurses/tui_renderer.h` | CP_* constants + tui_renderer_init() decl | Constants replaced by theme.h; init replaced by app_renderer_init() |
| `include/cels-ncurses/tui_components.h` | Widget render functions (static inline) | Split into `examples/widgets/*.h` (rewritten) |

### Files MODIFIED in module
| File | Change |
|------|--------|
| `src/tui_engine.c` | Remove `#include "cels-ncurses/tui_renderer.h"` and `tui_renderer_init()` call |
| `include/cels-ncurses/tui_engine.h` | Remove `#include <cels-ncurses/tui_renderer.h>` |
| `src/window/tui_window.c` | Remove init_pair() calls (lines 95-100) |
| `CMakeLists.txt` | Remove `tui_renderer.c` from target_sources |

### Files CREATED in example app
| File | Purpose |
|------|---------|
| `examples/widgets/theme.h` | Semantic TUI_Style constants |
| `examples/widgets/canvas.h` | Canvas header widget (DrawContext-based) |
| `examples/widgets/button.h` | Button widget (DrawContext-based) |
| `examples/widgets/slider.h` | Cycle + toggle slider widgets (DrawContext-based) |
| `examples/widgets/hint.h` | Hint text widget (DrawContext-based) |
| `examples/widgets/info_box.h` | Info box widget (DrawContext-based) |
| `examples/render_provider.h` | App-level CELS render provider |

### Files MODIFIED in example app
| File | Change |
|------|--------|
| `examples/app.c` | Include render_provider.h, call app_renderer_init() after TUI_Engine_use(), include widget headers |

## State of the Art

| Old Approach (Phases 1-4 bridge) | New Approach (Phase 5) | Impact |
|----------------------------------|------------------------|--------|
| Widgets use mvprintw(row, col, ...) to stdscr | Widgets use tui_draw_text(ctx, x, y, ...) to layer WINDOW | No stdscr dependency for drawing |
| overwrite(stdscr, bg->win) bridge copies content | Direct DrawContext drawing to layer window | Eliminates unnecessary copy step |
| attron(COLOR_PAIR(CP_X) | A_BOLD) state management | tui_style_apply(win, style) atomic application | No style leaks between draw calls |
| init_pair(N, FG, BG) manual pair indices | tui_color_rgb(r,g,b) + alloc_pair() automatic | No pair index management |
| Renderer in module, app-specific | Renderer in app, module is pure graphics backend | Clean module boundary |
| g_render_row global sequential layout | Explicit (x, y) coordinates per widget | Supports future layout engines (Clay) |

## Open Questions

None. This phase operates entirely within the existing codebase with well-understood APIs. All decisions have been made in CONTEXT.md.

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis of all source files in cels-ncurses module
- CONTEXT.md decisions from /gsd:discuss-phase
- STATE.md accumulated context from Phases 1-4
- Phase 1-4 API headers (tui_draw.h, tui_color.h, tui_layer.h, tui_frame.h)
- CELS framework macros (cels.h: _CEL_DefineFeature, _CEL_Provides, _CEL_DefineModule)

### Secondary (MEDIUM confidence)
- Header-only vs header+source pattern analysis for C widget files

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, all existing code
- Architecture: HIGH -- patterns derived from existing codebase and locked CONTEXT.md decisions
- Pitfalls: HIGH -- identified from direct code analysis of actual dependency chains
- Code examples: HIGH -- derived from existing working code (tui_components.h, tui_renderer.c) mapped to existing working API (tui_draw.h, tui_color.h)

**Research date:** 2026-02-08
**Valid until:** Indefinite (internal refactoring, no external dependency freshness concern)
