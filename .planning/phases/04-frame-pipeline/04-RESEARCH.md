# Phase 4: Frame Pipeline - Research

**Researched:** 2026-02-08
**Domain:** ncurses frame lifecycle, retained-mode dirty tracking, ECS system registration
**Confidence:** HIGH

## Summary

Phase 4 implements a begin/end frame lifecycle that orchestrates layer compositing with dirty tracking. The core task is straightforward: `tui_frame_begin()` erases dirty layers and resets drawing state, `tui_frame_end()` calls `update_panels()` + `doupdate()`, and the existing frame loop in `tui_window_frame_loop()` simplifies to just `Engine_Progress(delta)` + `usleep()`.

The user has locked the design as retained-mode with dirty tracking (not immediate-mode erase-all). Layers keep their content between frames; only layers marked dirty (via any draw call into their DrawContext) get cleared at frame_begin. A `tui_frame_invalidate_all()` function forces all layers dirty for full redraws (resize, mode changes). The frame pipeline also owns frame timing data (frame count, delta time, FPS), moving it out of TUI_WindowState.

Integration with the existing ECS uses two system registrations: frame_begin at PreStore (runs before renderer) and frame_end at PostFrame (runs after renderer). The existing renderer at OnStore draws between the two -- no changes needed to renderer registration. The current `update_panels()` + `doupdate()` in `tui_window_frame_loop()` must be removed since frame_end handles it. A default fullscreen background layer (z=0) is created at init as an immediate drawing surface, replacing stdscr usage.

**Primary recommendation:** Add a `bool dirty` flag to `TUI_Layer`, auto-set it from a wrapper around `tui_layer_get_draw_context()`, use `werase()` (not `wclear()`) for clearing dirty layers, and register frame_begin/frame_end as standalone ECS systems via `ecs_system_init()` following the exact pattern from `tui_input.c`.

## Standard Stack

### Core

No new libraries are needed. This phase uses only existing dependencies.

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses (wide) | 6.x | `werase()`, `wattr_set()`, `update_panels()`, `doupdate()` | Already linked, panel compositing |
| panel library | 6.x | `update_panels()` for z-ordered compositing | Already linked via `-lpanelw` |
| POSIX time | N/A | `clock_gettime(CLOCK_MONOTONIC)` for frame timing | Standard C/POSIX, no new dependency |
| flecs | (via cels) | ECS system registration at PreStore/PostFrame | Already linked via cels |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `<time.h>` | POSIX | `struct timespec`, `clock_gettime` | Frame timing measurement |
| `<assert.h>` | C99 | `assert()` for misuse detection | Drawing outside frame, nested frame_begin |

**Installation:** No new packages needed. All dependencies are already in CMakeLists.txt.

## Architecture Patterns

### Recommended Project Structure

New files for Phase 4:

```
include/cels-ncurses/
    tui_frame.h          # Frame pipeline public API (new)
src/
    frame/
        tui_frame.c      # Frame pipeline implementation (new)
```

Modifications to existing files:

```
include/cels-ncurses/
    tui_layer.h          # Add dirty flag to TUI_Layer struct
    tui_window.h         # Remove frame timing from TUI_WindowState (or keep for compat)
src/
    window/tui_window.c  # Remove update_panels()/doupdate() from frame loop
    layer/tui_layer.c    # Update tui_layer_get_draw_context to mark dirty
    tui_engine.c         # Register frame_begin/frame_end ECS systems
CMakeLists.txt           # Add src/frame/tui_frame.c to INTERFACE sources
```

### Pattern 1: Dirty Flag Per Layer

**What:** Add a `bool dirty` field to `TUI_Layer`. Auto-set to `true` whenever `tui_layer_get_draw_context()` is called. `tui_frame_begin()` iterates all layers, calls `werase()` + style reset only on dirty ones, then clears the dirty flag.

**When to use:** Every frame. The dirty flag is the core of retained-mode rendering.

**Why this way:** The user explicitly decided "retained-mode with dirty tracking (NOT immediate-mode erase-all)". This means layers that were not drawn into between frames keep their content -- only modified layers get cleared. This is performance-optimal for ncurses because fewer changed cells means fewer bytes sent to the terminal.

**Example:**
```c
// In tui_layer.h -- add dirty flag to TUI_Layer struct
typedef struct TUI_Layer {
    char name[64];
    PANEL* panel;
    WINDOW* win;
    int x, y;
    int width, height;
    bool visible;
    bool dirty;           /* NEW: auto-set by get_draw_context */
} TUI_Layer;

// In tui_layer.c -- mark dirty when draw context is obtained
TUI_DrawContext tui_layer_get_draw_context(TUI_Layer* layer) {
    layer->dirty = true;  /* Any drawing into this layer marks it dirty */
    return tui_draw_context_create(layer->win, 0, 0, layer->width, layer->height);
}
```

### Pattern 2: Frame State as Global with Extern Linkage

**What:** Frame pipeline state (frame count, in_frame flag, timing) stored as a global struct with extern linkage, following the same INTERFACE library pattern as `g_layers[]` and `TUI_WindowState`.

**When to use:** All frame pipeline state that must be shared across translation units.

**Example:**
```c
// In tui_frame.h
typedef struct TUI_FrameState {
    uint64_t frame_count;    /* Total frames rendered */
    float delta_time;        /* Seconds since last frame */
    float fps;               /* Current FPS (1.0 / delta_time) */
    bool in_frame;           /* True between frame_begin and frame_end */
} TUI_FrameState;

extern TUI_FrameState g_frame_state;

// In tui_frame.c
TUI_FrameState g_frame_state = {0};
```

### Pattern 3: ECS System Registration (Direct ecs_system_init)

**What:** Register frame_begin and frame_end as standalone ECS systems using `ecs_system_init()` with the exact pattern from `tui_input.c` line 149-161. No query terms (standalone system), phase dependency via `ecs_pair(EcsDependsOn, phase_id)`.

**When to use:** For registering systems that don't iterate entities -- they just run once per frame at a specific phase.

**Why not CEL_System macro:** The `CEL_System` macro is designed for the consumer's context and generates static state. For module-internal systems, direct `ecs_system_init()` is cleaner and matches the existing pattern in `tui_input.c`.

**Example:**
```c
// Exact pattern from tui_input.c (proven working)
static void tui_frame_begin_callback(ecs_iter_t* it) {
    (void)it;
    tui_frame_begin();
}

static void tui_frame_end_callback(ecs_iter_t* it) {
    (void)it;
    tui_frame_end();
}

void tui_frame_register_systems(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    // frame_begin at PreStore (before renderer at OnStore)
    {
        ecs_system_desc_t sys_desc = {0};
        ecs_entity_desc_t entity_desc = {0};
        entity_desc.name = "TUI_FrameBeginSystem";
        ecs_id_t phase_ids[3] = {
            ecs_pair(EcsDependsOn, EcsPreStore),
            EcsPreStore,
            0
        };
        entity_desc.add = phase_ids;
        sys_desc.entity = ecs_entity_init(world, &entity_desc);
        sys_desc.callback = tui_frame_begin_callback;
        ecs_system_init(world, &sys_desc);
    }

    // frame_end at PostFrame (after renderer at OnStore)
    {
        ecs_system_desc_t sys_desc = {0};
        ecs_entity_desc_t entity_desc = {0};
        entity_desc.name = "TUI_FrameEndSystem";
        ecs_id_t phase_ids[3] = {
            ecs_pair(EcsDependsOn, EcsPostFrame),
            EcsPostFrame,
            0
        };
        entity_desc.add = phase_ids;
        sys_desc.entity = ecs_entity_init(world, &entity_desc);
        sys_desc.callback = tui_frame_end_callback;
        ecs_system_init(world, &sys_desc);
    }
}
```

### Pattern 4: Default Background Layer

**What:** Create a fullscreen layer at z=0 during frame pipeline initialization. This gives the renderer an immediate drawing surface, replacing stdscr. The layer is named "background" and is the implicit bottom of the z-order stack.

**When to use:** Created once at init time, before the first frame_begin.

**Example:**
```c
// During tui_frame_init() or equivalent
static TUI_Layer* g_background_layer = NULL;

void tui_frame_init(void) {
    g_background_layer = tui_layer_create("background", 0, 0, COLS, LINES);
    if (g_background_layer) {
        tui_layer_lower(g_background_layer);  /* Ensure z=0 (bottom) */
    }
}
```

### Pattern 5: Frame Timing via clock_gettime

**What:** Measure actual frame duration using `clock_gettime(CLOCK_MONOTONIC)` for accurate delta_time and FPS. This replaces the current naive approach where `delta_time = 1.0f / fps` is a constant.

**When to use:** Called in frame_begin (record start) and frame_end (compute delta from previous frame's start).

**Example:**
```c
#include <time.h>

static struct timespec g_frame_start = {0};
static struct timespec g_prev_frame_start = {0};

static float timespec_diff_sec(struct timespec end, struct timespec start) {
    return (float)(end.tv_sec - start.tv_sec) +
           (float)(end.tv_nsec - start.tv_nsec) / 1e9f;
}

// In frame_begin:
g_prev_frame_start = g_frame_start;
clock_gettime(CLOCK_MONOTONIC, &g_frame_start);
if (g_prev_frame_start.tv_sec != 0) {
    g_frame_state.delta_time = timespec_diff_sec(g_frame_start, g_prev_frame_start);
    g_frame_state.fps = 1.0f / g_frame_state.delta_time;
}
```

### Anti-Patterns to Avoid

- **Using `wclear()` instead of `werase()`:** `wclear()` calls `clearok(TRUE)` which forces a complete terminal repaint on the next refresh, causing visible flicker. Always use `werase()` which just fills blanks into the window buffer. (Pitfall #8 from research)

- **Calling `wrefresh()` or `wnoutrefresh()` on panel-managed windows:** The `update_panels()` function handles all necessary `wnoutrefresh()` calls internally. Direct calls bypass panel compositing and cause visual corruption.

- **Leaving `update_panels()` + `doupdate()` in the window frame loop:** After Phase 4, the frame loop must NOT contain these calls. They move into `tui_frame_end()`. Having them in both places causes duplicate compositing.

- **Drawing outside a frame (before frame_begin or after frame_end):** The `in_frame` flag should be checked with `assert(g_frame_state.in_frame)` in debug builds. Drawing outside a frame means the draw won't be properly composited.

- **Using `erase()` (stdscr-targeted) instead of `werase(win)`:** `erase()` targets stdscr which is not part of the panel stack. Must use `werase(layer->win)` for panel-backed layers.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Layer compositing | Manual wnoutrefresh ordering per window | `update_panels()` + `doupdate()` | Panel library handles overlap resolution, visibility, and correct refresh ordering automatically |
| Style reset per layer | Manual attribute tracking/restoration | `wattr_set(win, A_NORMAL, 0, NULL)` | Atomic reset, no leaked attribute state |
| Window clearing | Character-by-character fill loop | `werase(win)` | ncurses optimized internal clear, handles wide chars correctly |
| Frame timing | `usleep(full_budget)` constant | `clock_gettime(CLOCK_MONOTONIC)` delta | Accurate elapsed time accounts for work duration |
| Scissor reset | Manual stack pointer manipulation | `tui_scissor_reset(&ctx)` | Already implemented, resets to full drawable area |

**Key insight:** The ncurses panel library already does the hard part (z-ordered compositing with overlap resolution). Phase 4 just needs to orchestrate when clearing happens and when compositing happens, wrapping the existing panel infrastructure in a frame lifecycle.

## Common Pitfalls

### Pitfall 1: Duplicate update_panels/doupdate

**What goes wrong:** If `update_panels()` + `doupdate()` remain in `tui_window_frame_loop()` AND are also called in `tui_frame_end()`, compositing happens twice per frame. This wastes terminal I/O bandwidth and can cause subtle timing issues.

**Why it happens:** STATE.md explicitly warns: "tui_frame_begin/end must avoid duplicate update_panels/doupdate calls since frame loop already has them." The current frame loop (tui_window.c line 133-134) has these calls.

**How to avoid:** Remove `update_panels()` and `doupdate()` from `tui_window_frame_loop()` when frame_end takes over. The frame loop simplifies to `Engine_Progress(delta)` + `usleep()`.

**Warning signs:** Rendering works but FPS drops to half expected, or terminal output doubles.

### Pitfall 2: Scissor State Leaking Between Frames

**What goes wrong:** If a frame exits early (error, early return) without popping all pushed scissor rects, the next frame starts with a narrowed clip region and draws are clipped incorrectly.

**Why it happens:** The scissor stack (`g_scissor_sp` in `tui_scissor.c`) is static state that persists between frames.

**How to avoid:** `tui_frame_begin()` must call `tui_scissor_reset()` for each layer's draw context. The user explicitly decided "Scissor stack auto-reset at frame_begin (prevents leaked clips)."

**Warning signs:** Drawing that worked on one frame suddenly clips to a small region on the next.

### Pitfall 3: Drawing to stdscr After Panel Migration

**What goes wrong:** The current renderer (`tui_renderer.c` line 144: `erase()`, line 216: `wnoutrefresh(stdscr)`) still draws to stdscr. After Phase 4 creates a background layer, drawing to stdscr causes visual corruption because stdscr content bleeds through/under panels unpredictably.

**Why it happens:** Pitfall #1 from PITFALLS.md: "Writing to stdscr after panels are introduced causes flickering, ghost characters, and z-order violations."

**How to avoid:** The renderer must be migrated to draw into the background layer's DrawContext instead of stdscr. The user decided "Migrate renderer off stdscr in Phase 4 -- replace wnoutrefresh(stdscr) with layer-based drawing."

**Warning signs:** Ghost characters, content appearing behind panels that should be opaque.

### Pitfall 4: frame_begin Clearing Non-Dirty Layers Destroys Retained Content

**What goes wrong:** If frame_begin calls `werase()` on ALL layers (not just dirty ones), retained content in non-dirty layers is lost. This defeats the purpose of retained-mode rendering.

**Why it happens:** The user explicitly decided "Retained-mode with dirty tracking (NOT immediate-mode erase-all). Layers keep content between frames; only dirty layers get erased."

**How to avoid:** Only `werase()` layers where `layer->dirty == true`. Then clear the dirty flag.

**Warning signs:** Static content (borders, labels) that should persist between frames disappears and reappears with flicker.

### Pitfall 5: Assert on Misuse Can Crash Production

**What goes wrong:** `assert(g_frame_state.in_frame)` will crash the application in debug builds if a draw call happens outside the frame lifecycle.

**Why it happens:** The user decided "Assert on misuse: drawing outside a frame, nested frame_begin calls." This is intentional for development -- misuse should be caught loudly.

**How to avoid:** This is the desired behavior. Ensure asserts are used correctly -- they fire in debug builds but are compiled out with `-DNDEBUG` in release builds.

**Warning signs:** Crashes during development when draw calls are made at wrong times (which is the intended behavior -- the crash tells you to fix the ordering).

### Pitfall 6: Frame Timing Drift (Existing Bug)

**What goes wrong:** The current frame loop uses `usleep(delta * 1000000)` where delta is a constant `1.0f / fps`. This sleeps for the full frame budget without subtracting work time, so actual FPS is always lower than target.

**Why it happens:** CONCERNS.md documents this: "usleep(full_budget) sleeps for the full frame duration without accounting for time spent in Engine_Progress() and doupdate()."

**How to avoid:** Since frame timing data is moving into the frame pipeline, the frame loop should measure elapsed time and sleep only the remaining budget. However, fixing the sleep pattern is a frame loop concern (plan 04-02), not a frame pipeline concern (plan 04-01).

**Warning signs:** Actual FPS is consistently lower than target FPS.

## Code Examples

### tui_frame_begin Implementation Pattern

```c
// Source: Derived from CONTEXT.md decisions + codebase patterns
void tui_frame_begin(void) {
    assert(!g_frame_state.in_frame && "Nested tui_frame_begin() calls");

    // Frame timing
    g_prev_frame_start = g_frame_start;
    clock_gettime(CLOCK_MONOTONIC, &g_frame_start);
    if (g_prev_frame_start.tv_sec != 0) {
        g_frame_state.delta_time = timespec_diff_sec(g_frame_start, g_prev_frame_start);
        g_frame_state.fps = (g_frame_state.delta_time > 0.0f)
            ? 1.0f / g_frame_state.delta_time : 0.0f;
    }
    g_frame_state.frame_count++;

    // Clear dirty layers only (retained-mode)
    for (int i = 0; i < g_layer_count; i++) {
        if (g_layers[i].dirty && g_layers[i].visible) {
            werase(g_layers[i].win);
            // Reset style to default on dirty layers
            wattr_set(g_layers[i].win, A_NORMAL, 0, NULL);
            g_layers[i].dirty = false;
        }
    }

    // Scissor stack reset is per-DrawContext, happens when draw context is obtained
    // (frame_begin does not hold DrawContexts -- individual systems do)

    g_frame_state.in_frame = true;
}
```

### tui_frame_end Implementation Pattern

```c
// Source: Derived from CONTEXT.md decisions
void tui_frame_end(void) {
    assert(g_frame_state.in_frame && "tui_frame_end() without tui_frame_begin()");

    g_frame_state.in_frame = false;

    // Composite all visible layers via panel library
    update_panels();
    doupdate();
}
```

### tui_frame_invalidate_all Implementation Pattern

```c
// Source: CONTEXT.md decision -- forces every layer dirty
void tui_frame_invalidate_all(void) {
    for (int i = 0; i < g_layer_count; i++) {
        g_layers[i].dirty = true;
    }
}
```

### Frame Loop Simplification Pattern

```c
// Source: tui_window.c current frame loop, modified per CONTEXT.md
static void tui_window_frame_loop(void) {
    // ... (existing initialization unchanged) ...

    while (g_running) {
        Engine_Progress(delta);
        // update_panels() and doupdate() REMOVED -- handled by frame_end ECS system
        usleep((unsigned int)(delta * 1000000));
    }

    // ... (existing shutdown unchanged) ...
}
```

### Renderer Migration Pattern (stdscr to background layer)

```c
// Source: tui_renderer.c current pattern -> migrated pattern
// BEFORE (current):
erase();                    // clears stdscr
// ... draw calls using mvprintw (implicit stdscr) ...
wnoutrefresh(stdscr);      // mark stdscr for refresh

// AFTER (Phase 4):
// Get draw context from background layer
extern TUI_Layer* tui_frame_get_background(void);
TUI_Layer* bg = tui_frame_get_background();
TUI_DrawContext ctx = tui_layer_get_draw_context(bg);
// ... draw calls using tui_draw_text(&ctx, ...) etc ...
// No wnoutrefresh needed -- frame_end handles it via update_panels()
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `erase()` + `wnoutrefresh(stdscr)` per frame | `werase(layer->win)` only on dirty layers + `update_panels()` + `doupdate()` | Phase 3 -> Phase 4 | Retained content, reduced terminal I/O |
| `delta_time = 1.0f / fps` constant | `clock_gettime(CLOCK_MONOTONIC)` measured delta | Phase 4 | Accurate frame timing |
| `update_panels()` + `doupdate()` in window frame loop | Moved to `tui_frame_end()` ECS system at PostFrame | Phase 4 | Decoupled compositing from window provider |
| Renderer draws to stdscr via `mvprintw()` | Renderer draws to background layer via DrawContext | Phase 4 | Eliminates stdscr corruption with panels |

**Deprecated/outdated after Phase 4:**
- `wnoutrefresh(stdscr)` in renderer: replaced by frame_end's `update_panels()`
- `erase()` in renderer: replaced by frame_begin's per-layer `werase()`
- Frame timing in `TUI_WindowState.delta_time`: replaced by `g_frame_state.delta_time`

## ECS Phase Execution Order

Critical for understanding when frame_begin and frame_end run relative to other systems.

The flecs execution order within `Engine_Progress()` is:

```
1. EcsOnStart     (CELS_Phase_OnStart)      -- one-time startup
2. EcsPreFrame    (CELS_Phase_PreFrame)      -- frame setup
3. EcsOnLoad      (CELS_Phase_OnLoad)        -- input reading (TUI_Input lives here)
4. EcsPostLoad    (CELS_Phase_PostLoad)
5. EcsPreUpdate   (CELS_Phase_PreUpdate)
6. EcsOnUpdate    (CELS_Phase_OnUpdate)      -- game logic
7. EcsOnValidate  (CELS_Phase_OnValidate)
8. EcsPostUpdate  (CELS_Phase_PostUpdate)
9. EcsPreStore    (CELS_Phase_PreStore)      -- FRAME_BEGIN goes here
10. EcsOnStore    (CELS_Phase_OnStore)       -- renderer runs here (Renderable feature)
11. EcsPostFrame  (CELS_Phase_PostFrame)     -- FRAME_END goes here
```

**Key insight:** CELS_Phase_OnRender maps to EcsOnStore (see cels.cpp line 401). The Renderable feature is defined with `.phase = CELS_Phase_OnStore`. So frame_begin at PreStore runs before rendering, and frame_end at PostFrame runs after rendering. This ordering is correct and validated by the codebase.

## Scissor Reset Strategy

The user decided "Scissor stack auto-reset at frame_begin (prevents leaked clips)." However, the scissor stack is static state in `tui_scissor.c` (per-TU in INTERFACE library) and `tui_scissor_reset()` takes a `TUI_DrawContext*` parameter.

**Problem:** frame_begin does not hold DrawContext references -- individual systems obtain their own DrawContexts from layers. The scissor stack is per-DrawContext.

**Resolution:** The scissor reset should happen when a DrawContext is obtained from a dirty layer (in `tui_layer_get_draw_context()`), or at the beginning of each system's render pass. Since `tui_scissor_reset()` reinitializes `g_scissor_sp = 0` and sets the base clip, calling it immediately after getting a draw context from a layer handles the reset cleanly.

**Alternative:** Since the scissor stack is per-TU (INTERFACE library static), and the draw context is typically obtained once per render system per frame, the reset naturally happens as part of each system's setup.

## Open Questions

1. **Frame timing ownership vs TUI_WindowState compatibility**
   - What we know: The user decided "Frame pipeline owns frame timing data (frame count, delta time, fps) -- moved out of TUI_WindowState." The current `TUI_WindowState.delta_time` is set in `tui_window_frame_loop()`.
   - What's unclear: Should `TUI_WindowState.delta_time` be removed, or should it remain as a backward-compatible alias that reads from `g_frame_state.delta_time`?
   - Recommendation: Keep `TUI_WindowState.delta_time` for now but update it from the frame pipeline. Remove in Phase 5 if needed. Avoids breaking any code that reads from WindowState.

2. **Background layer resize behavior**
   - What we know: `tui_layer_resize_all()` already resizes all layers to full terminal size on KEY_RESIZE. The background layer will be part of g_layers and resize automatically.
   - What's unclear: Should KEY_RESIZE also call `tui_frame_invalidate_all()` to force a full redraw?
   - Recommendation: Yes -- after resize, all layers should be invalidated since their content is stale at the new dimensions. Add `tui_frame_invalidate_all()` call in the KEY_RESIZE handler.

3. **Hardcoded window dimensions bug**
   - What we know: CONCERNS.md documents `TUI_WindowState.width = 600` and `.height = 800` as hardcoded with correct COLS/LINES logic commented out.
   - What's unclear: Should this be fixed in Phase 4 or left for Phase 5?
   - Recommendation: Fix it in Phase 4 plan 04-02 (frame loop integration) since it directly affects the background layer creation (`COLS`/`LINES` are used). Uncomment the correct logic.

## Sources

### Primary (HIGH confidence)
- **Codebase inspection** -- All source files read directly: tui_window.c, tui_layer.c, tui_renderer.c, tui_input.c, tui_frame_pipeline (CONTEXT.md), tui_engine.c, tui_draw.c, tui_scissor.c
- **cels.cpp** -- ECS phase mapping (lines 385-405), system registration pattern (lines 470-505)
- **STATE.md, ROADMAP.md, REQUIREMENTS.md** -- Project decisions and constraints
- **PITFALLS.md** -- Verified pitfalls #1 (stdscr+panels), #8 (werase not wclear), #11 (frame timing), #15 (single doupdate)
- **CONCERNS.md** -- Hardcoded dimensions bug, frame timing drift, renderer coupling

### Secondary (MEDIUM confidence)
- **ncurses man pages** (werase, wclear, update_panels, doupdate) -- via web search, consistent with training data
- **clock_gettime(3)** -- POSIX standard, well-documented

### Tertiary (LOW confidence)
- None -- all findings verified with primary sources.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, all existing tools
- Architecture: HIGH -- patterns derived from existing codebase + user decisions in CONTEXT.md
- Pitfalls: HIGH -- documented in PITFALLS.md, verified against current code
- ECS integration: HIGH -- exact system registration pattern from tui_input.c, phase mapping from cels.cpp

**Research date:** 2026-02-08
**Valid until:** 2026-03-08 (stable domain, no external dependencies changing)
