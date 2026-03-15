# Phase 5: Frame Pipeline - Research

**Researched:** 2026-03-14
**Domain:** NCurses frame lifecycle as CELS pipeline phase systems; legacy code removal
**Confidence:** HIGH

## Summary

This phase absorbs two legacy files (`src/frame/tui_frame.c` and `src/layer/tui_layer.c`) into ECS systems, deletes the `draw_test` example, and cleans up the public header. The working tree already contains partial Phase 4 refactoring (tui_layer_entity.c deleted, layer lifecycle/system/composition moved into ncurses_module.c, tui_internal.h updated) which simplifies this phase significantly.

The frame pipeline currently consists of two ECS systems in `tui_frame.c` (TUI_FrameBeginSystem at PreRender, TUI_FrameEndSystem at PostRender) that delegate to `tui_frame_begin()` and `tui_frame_end()`. These functions contain logic that must be redistributed: timing goes away (WindowState already handles it), legacy g_layers[] clearing goes away (layers deleted), SIGWINCH blocking inlines into ECS system bodies, and update_panels()/doupdate() inlines into ECS system bodies. FPS throttling moves from `tui_hook_frame_end()` in `tui_window.c` to `NCurses_WindowUpdateSystem`.

**Primary recommendation:** Rewrite TUI_FrameBeginSystem and TUI_FrameEndSystem inline in ncurses_module.c (alongside TUI_LayerSystem), delete tui_frame.c, tui_layer.c, draw_test.c, and strip the public header of all legacy layer/frame API surface.

## Standard Stack

No new libraries or dependencies required. This phase is purely a refactoring and deletion exercise within existing codebase patterns.

### Core (Already Present)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses (wide) | System | Terminal rendering, panel compositing | Already the rendering backend |
| CELS (cels) | v0.5.3 | ECS framework for systems/components/phases | Already the architectural foundation |
| ncurses panel library | System | update_panels(), doupdate(), z-order stacking | Already used for layer compositing |

### POSIX APIs Used
| API | Purpose | Where Used |
|-----|---------|-----------|
| `sigprocmask(SIG_BLOCK/SIG_UNBLOCK, SIGWINCH)` | Block SIGWINCH during ncurses panel compositing | Currently in tui_frame_begin/end, will move to ECS system bodies |
| `clock_gettime(CLOCK_MONOTONIC)` | Frame timing | Already in NCurses_WindowUpdateSystem; duplicate in tui_frame.c to be removed |
| `usleep()` | FPS throttle | Currently in tui_hook_frame_end(), moves to NCurses_WindowUpdateSystem |

## Architecture Patterns

### Current System Registration Pattern (from ncurses_module.c)

All ECS declarations live in `ncurses_module.c`. Systems are defined inline using the `CEL_System` macro:

```c
// Source: src/ncurses_module.c (current working tree)
CEL_System(TUI_LayerSystem, .phase = PreRender) {
    // ... body with cel_query/cel_each ...
}

CEL_Module(NCurses, init) {
    cels_register(NCurses_WindowState, NCurses_InputState,
                  NCursesWindowLC,
                  TUI_Renderable, TUI_LayerConfig, TUI_DrawContext_Component,
                  TUI_LayerLC, TUI_LayerSystem);
    ncurses_register_frame_systems();
}
```

**Key pattern:** Systems defined in the same TU as the module init can be registered directly via `cels_register()` -- no need for a separate `_register()` call or forward declaration.

### Current Frame System Pattern (in tui_frame.c -- TO BE REPLACED)

```c
// Source: src/frame/tui_frame.c (current, to be deleted)
CEL_System(TUI_FrameBeginSystem, .phase = PreRender) {
    cel_run {
        tui_frame_begin();  // delegates to function with legacy g_layers[]
    }
}
CEL_System(TUI_FrameEndSystem, .phase = PostRender) {
    cel_run {
        tui_frame_end();
        tui_hook_frame_end();  // FPS throttle from tui_window.c
    }
}
void ncurses_register_frame_systems(void) {
    tui_frame_init();
    TUI_FrameBeginSystem_register();
    TUI_FrameEndSystem_register();
}
```

### Target Architecture: All Systems in ncurses_module.c

After this phase, ALL systems live in `ncurses_module.c`:

```
Pipeline Phase Order (CELS):
  OnLoad       -> NCurses_InputSystem (defined in tui_input.c, registered via tui_internal.h forward decl)
  LifecycleEval -> [CELS internal]
  OnUpdate     -> [developer systems]
  StateSync    -> [CELS internal]
  Recompose    -> [CELS internal]
  PreRender    -> NCurses_WindowUpdateSystem (defined in tui_window.c)
                  TUI_LayerSystem (defined in ncurses_module.c) -- sync+clear+resize
                  TUI_FrameBeginSystem (NEW in ncurses_module.c) -- block SIGWINCH
  OnRender     -> [developer render systems]
  PostRender   -> TUI_FrameEndSystem (NEW in ncurses_module.c) -- update_panels, doupdate, unblock SIGWINCH
```

**Note:** NCurses_InputSystem and NCurses_WindowUpdateSystem remain defined in their respective source files (tui_input.c, tui_window.c) because they own static state (CEL_State singletons) that must stay in those TUs. They are forward-declared via CEL_Define_System in tui_internal.h.

### Recommended Project Structure After Phase 5

```
src/
  ncurses_module.c       # All CELS declarations: lifecycles, observers, systems, compositions
                         # Systems: TUI_LayerSystem, TUI_FrameBeginSystem, TUI_FrameEndSystem
  tui_internal.h         # Internal forward declarations
  tui_window.h           # WindowState enum + tui_hook_frame_end decl (TO BE CLEANED)
  window/
    tui_window.c         # Terminal init/shutdown, NCurses_WindowUpdateSystem, FPS throttle
    tui_spawn.c          # PTY terminal spawning
  input/
    tui_input.c          # NCurses_InputSystem, input config
  graphics/
    tui_color.c          # Color system
    tui_draw.c           # Drawing primitives
    tui_scissor.c        # Scissor stack
    tui_subcell.c        # Sub-cell rendering
  layer/
    tui_layer_panel.c    # Pure ncurses panel helpers (create, destroy, resize, clear, sort)
    [tui_layer.c DELETED]
  [frame/ DELETED]       # Entire directory removed
include/
  cels_ncurses.h         # Public API (unchanged)
  cels_ncurses_draw.h    # Draw API (legacy sections stripped)
examples/
  minimal.c              # Reference example (serves as layers_demo too)
  [draw_test.c DELETED]
```

### Anti-Patterns to Avoid

- **Leaving tui_frame_begin/end as functions:** The whole point is inlining their logic into ECS system bodies. No function call layer.
- **Keeping duplicate timing:** WindowState already computes delta_time/fps. Do not compute it again in a frame begin system. The TUI_FrameState global should be removed entirely.
- **Clearing layers in frame_begin:** Clearing moves to TUI_LayerSystem at PreRender (which already runs before frame begin). The system already has the cel_query/cel_each loop over layers.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Layer clearing | Custom clearing loop in frame_begin | `ncurses_layer_clear_window()` in TUI_LayerSystem | Already exists in tui_layer_panel.c, already handles werase + wattr_set + subcell clear |
| Panel compositing | Custom window refresh logic | `update_panels()` + `doupdate()` | NCurses panel library handles the compositing correctly |
| Z-order rebuilding | Manual panel stacking | `ncurses_layer_sort_and_stack()` | Already exists in tui_layer_panel.c |
| FPS throttle calculation | New timing code | Existing `tui_hook_frame_end()` pattern | Just move the body inline into NCurses_WindowUpdateSystem |

## Common Pitfalls

### Pitfall 1: SIGWINCH Blocking Placement
**What goes wrong:** SIGWINCH arrives during update_panels()/doupdate() and corrupts ncurses internal state.
**Why it happens:** The frame begin/end pair currently blocks SIGWINCH before rendering and unblocks after doupdate(). If the new ECS systems don't maintain this bracket, terminal resize can crash the app.
**How to avoid:** Block SIGWINCH in TUI_FrameBeginSystem (PreRender, after TUI_LayerSystem), unblock in TUI_FrameEndSystem (PostRender, after doupdate()). This ensures SIGWINCH is blocked during all of OnRender (developer drawing) and the compositing in PostRender.
**Warning signs:** Intermittent crashes or visual corruption on terminal resize.

### Pitfall 2: System Ordering Within PreRender
**What goes wrong:** TUI_FrameBeginSystem runs before TUI_LayerSystem, so layers haven't been cleared yet when SIGWINCH is blocked.
**Why it happens:** Multiple systems at the same phase -- ordering not guaranteed unless explicitly managed.
**How to avoid:** TUI_LayerSystem must run first (sync visibility, clear, resize), then TUI_FrameBeginSystem blocks SIGWINCH. CELS registers systems in declaration order, so define TUI_LayerSystem before TUI_FrameBeginSystem in ncurses_module.c and register them in that order. Alternatively, clearing could be folded into a single system.
**Warning signs:** Layers still dirty when developer draws; visual artifacts.

### Pitfall 3: Removing g_layers[] References From tui_input.c
**What goes wrong:** tui_input.c calls `tui_layer_resize_all()` and `tui_frame_invalidate_all()` on KEY_RESIZE. After tui_layer.c is deleted, these are unresolved symbols.
**Why it happens:** KEY_RESIZE handling in the input system still references the old imperative layer API.
**How to avoid:** Remove the KEY_RESIZE branch from the input system entirely. Resize is already handled by NCurses_WindowUpdateSystem (PTY mode polls TIOCGWINSZ, direct mode handles SIGWINCH) and TUI_LayerSystem (detects COLS/LINES changes and resizes entity layers). The input system should just drop KEY_RESIZE events.
**Warning signs:** Link errors for tui_layer_resize_all / tui_frame_invalidate_all.

### Pitfall 4: TUI_FrameState Global Removal
**What goes wrong:** draw_test.c references g_frame_state extensively. If TUI_FrameState is removed from the header while draw_test still exists, compilation fails.
**Why it happens:** draw_test.c is a standalone non-ECS target that uses the imperative API.
**How to avoid:** Delete draw_test.c AND its CMake target in the same step as removing the TUI_FrameState type and g_frame_state extern from the header. Do not remove the type before deleting the consumer.
**Warning signs:** Compilation errors in draw_test target.

### Pitfall 5: FPS Throttle Timing Source
**What goes wrong:** FPS throttle in `tui_hook_frame_end()` uses `g_frame_start` from tui_window.c. When moved to NCurses_WindowUpdateSystem, the timing reference is the same -- but the call site moves from PostRender to... when?
**Why it happens:** The throttle must run AFTER doupdate() (PostRender), not during PreRender when NCurses_WindowUpdateSystem runs.
**How to avoid:** Per the CONTEXT.md decision, FPS throttle moves to NCurses_WindowUpdateSystem. However, it should be called at the END of the frame, not the beginning. Two options: (1) keep the throttle call in TUI_FrameEndSystem (PostRender) which calls the existing function from tui_window.c, or (2) restructure so the window system tracks when to throttle. Option 1 is simpler and matches the decision that "window system owns all timing" -- the implementation lives in tui_window.c, the call site is PostRender.
**Warning signs:** Frame rate not capped; CPU spinning at 100%.

### Pitfall 6: Clearing Only Visible Layers
**What goes wrong:** Clearing all entity layers including hidden ones wastes CPU on layers that won't be composited.
**Why it happens:** The new clearing code in TUI_LayerSystem must check both visibility and that the layer needs clearing.
**How to avoid:** In the cel_each loop, only clear if `TUI_LayerConfig->visible` is true. The CONTEXT.md says "Framework auto-clears all visible entity layers at PreRender." Hidden layers skip clearing.
**Warning signs:** Unnecessary CPU usage; drawing into hidden layers unexpectedly appears when they become visible.

## Code Examples

### TUI_LayerSystem With Clearing Added (PreRender)

```c
// Target: src/ncurses_module.c
// Add clearing to the existing TUI_LayerSystem cel_each loop
CEL_System(TUI_LayerSystem, .phase = PreRender) {
    static int prev_cols = 0;
    static int prev_lines = 0;

    bool resized = (COLS != prev_cols || LINES != prev_lines) &&
                   (prev_cols > 0 || prev_lines > 0);
    prev_cols = COLS;
    prev_lines = LINES;

    PANEL* panels[TUI_LAYER_MAX];
    int z_orders[TUI_LAYER_MAX];
    int count = 0;

    cel_query(TUI_LayerConfig, TUI_DrawContext_Component);
    cel_each(TUI_LayerConfig, TUI_DrawContext_Component) {
        /* Resize fullscreen entity layers when terminal dimensions changed */
        if (resized &&
            (TUI_LayerConfig->width == 0 || TUI_LayerConfig->height == 0)) {
            int new_w = TUI_LayerConfig->width == 0 ? COLS : TUI_LayerConfig->width;
            int new_h = TUI_LayerConfig->height == 0 ? LINES : TUI_LayerConfig->height;
            cel_update(TUI_DrawContext_Component) {
                ncurses_layer_panel_resize(TUI_DrawContext_Component, new_w, new_h);
            }
        }

        ncurses_layer_sync_visibility(
            TUI_LayerConfig->visible, TUI_DrawContext_Component->panel);

        /* NEW: Clear visible layers (developer gets blank canvas) */
        if (TUI_LayerConfig->visible) {
            ncurses_layer_clear_window(
                TUI_DrawContext_Component->win,
                TUI_DrawContext_Component->subcell_buf);
        }

        if (count < TUI_LAYER_MAX && TUI_DrawContext_Component->panel) {
            panels[count] = TUI_DrawContext_Component->panel;
            z_orders[count] = TUI_LayerConfig->z_order;
            count++;
        }
    }

    ncurses_layer_sort_and_stack(panels, z_orders, count);
}
```

### TUI_FrameBeginSystem (PreRender, After TUI_LayerSystem)

```c
// Target: src/ncurses_module.c
// Minimal system -- just block SIGWINCH
CEL_System(TUI_FrameBeginSystem, .phase = PreRender) {
    cel_run {
        sigset_t winch_set;
        sigemptyset(&winch_set);
        sigaddset(&winch_set, SIGWINCH);
        sigprocmask(SIG_BLOCK, &winch_set, NULL);
    }
}
```

### TUI_FrameEndSystem (PostRender)

```c
// Target: src/ncurses_module.c
// Composite panels and flush to terminal, then unblock SIGWINCH
CEL_System(TUI_FrameEndSystem, .phase = PostRender) {
    cel_run {
        update_panels();
        doupdate();

        /* Unblock SIGWINCH so getch() can receive KEY_RESIZE */
        sigset_t winch_set;
        sigemptyset(&winch_set);
        sigaddset(&winch_set, SIGWINCH);
        sigprocmask(SIG_UNBLOCK, &winch_set, NULL);

        /* FPS throttle -- window system owns timing */
        tui_hook_frame_end();
    }
}
```

### Module Init (Revised)

```c
// Target: src/ncurses_module.c
CEL_Module(NCurses, init) {
    cels_register(NCurses_WindowState, NCurses_InputState,
                  NCursesWindowLC,
                  TUI_Renderable, TUI_LayerConfig, TUI_DrawContext_Component,
                  TUI_LayerLC, TUI_LayerSystem,
                  TUI_FrameBeginSystem, TUI_FrameEndSystem);
}
```

### Input System KEY_RESIZE Cleanup

```c
// Target: src/input/tui_input.c
// Remove the KEY_RESIZE handler entirely -- resize handled by
// NCurses_WindowUpdateSystem + TUI_LayerSystem
if (ch == KEY_RESIZE) {
    int next;
    while ((next = wgetch(stdscr)) == KEY_RESIZE) { /* consume */ }
    if (next != ERR) ungetch(next);
    // NOTE: removed tui_layer_resize_all and tui_frame_invalidate_all calls
    // Resize is handled by NCurses_WindowUpdateSystem (detects COLS/LINES)
    // and TUI_LayerSystem (resizes entity layers)
    continue;
}
```

## Current State Analysis

### Files to DELETE

| File | Lines | Why Delete | Consumers |
|------|-------|-----------|-----------|
| `src/frame/tui_frame.c` | 241 | Logic absorbed into ECS systems in ncurses_module.c | ncurses_module.c calls ncurses_register_frame_systems() |
| `src/layer/tui_layer.c` | 223 | Legacy imperative layer API replaced by entity layers | draw_test.c (also deleted), tui_input.c (KEY_RESIZE), tui_frame.c |
| `examples/draw_test.c` | ~1200 | Standalone non-ECS demo; minimal.c covers visual testing | CMakeLists.txt draw_test target |
| `src/layer/tui_layer_entity.c` | ~deleted | Already deleted in working tree (content moved to ncurses_module.c + tui_layer_panel.c) | Was registered in CMakeLists, already removed |

### Files to MODIFY

| File | Changes Needed |
|------|---------------|
| `src/ncurses_module.c` | Add TUI_FrameBeginSystem + TUI_FrameEndSystem; add clearing to TUI_LayerSystem; remove ncurses_register_frame_systems() call; add signal.h + panel.h includes |
| `src/tui_internal.h` | Remove ncurses_register_frame_systems declaration; remove tui_window.h include reference |
| `src/tui_window.h` | Remove tui_hook_frame_end declaration (if call moves inline) OR keep if tui_hook_frame_end remains a function |
| `src/input/tui_input.c` | Remove KEY_RESIZE calls to tui_layer_resize_all/tui_frame_invalidate_all; remove cels_ncurses_draw.h include if no longer needed |
| `include/cels_ncurses_draw.h` | Remove entire "Layer System" section (TUI_Layer typedef, g_layers/g_layer_count externs, all tui_layer_* function decls); remove entire "Frame Pipeline" section (TUI_FrameState typedef, g_frame_state extern, all tui_frame_* function decls) |
| `CMakeLists.txt` | Remove tui_layer.c + tui_frame.c from INTERFACE sources; remove draw_test target entirely |

### Dependency Map

```
tui_frame.c depends on:
  -> g_layers[], g_layer_count (from tui_layer.c) -- for clearing loop
  -> tui_layer_create/lower (from tui_layer.c) -- for background layer
  -> tui_hook_frame_end (from tui_window.c) -- for FPS throttle
  -> tui_internal.h -- for ncurses_layer_entity_clear_dirty (already removed from tree)

tui_layer.c depended on by:
  -> tui_frame.c -- g_layers/g_layer_count clearing loop, background layer
  -> tui_input.c -- tui_layer_resize_all on KEY_RESIZE
  -> draw_test.c -- entire imperative API (create, destroy, show, hide, etc.)
  -> cels_ncurses_draw.h -- extern declarations

tui_input.c references to remove:
  -> tui_layer_resize_all(COLS, LINES) -- line 98
  -> tui_frame_invalidate_all() -- line 99

draw_test.c references (all removed by file deletion):
  -> g_frame_state, tui_frame_init/begin/end/get_background/invalidate_all
  -> g_layers, g_layer_count, tui_layer_create/destroy/show/hide/raise/lower/move/resize/get_draw_context
```

### SIGWINCH Flow Analysis

Current flow:
1. `tui_frame_begin()` blocks SIGWINCH (sigprocmask SIG_BLOCK)
2. Developer draws (between begin/end)
3. `tui_frame_end()` calls update_panels() + doupdate(), then unblocks SIGWINCH

Target flow (same semantics, different location):
1. TUI_FrameBeginSystem (PreRender) blocks SIGWINCH
2. Developer draws at OnRender
3. TUI_FrameEndSystem (PostRender) calls update_panels() + doupdate(), then unblocks SIGWINCH

The bracket is maintained. SIGWINCH is blocked for the entire OnRender and PostRender span.

### FPS Throttle Flow Analysis

Current flow:
1. `tui_hook_frame_end()` defined in `tui_window.c` (lines 322-334)
2. Called from `TUI_FrameEndSystem` in `tui_frame.c` (after `tui_frame_end()`)
3. Uses `g_frame_start` and `g_target_fps` statics from `tui_window.c`

Target flow (per CONTEXT.md decision "FPS throttle moves to NCurses_WindowUpdateSystem"):
- Option A: Move the usleep logic into NCurses_WindowUpdateSystem at PreRender -- but this runs at the START of the frame, so it would sleep before rendering, not after. This is actually fine for frame capping (sleep at top of loop == sleep at bottom, just shifted by one frame).
- Option B: Keep tui_hook_frame_end() call in TUI_FrameEndSystem (PostRender) -- simpler, same effect.
- **Recommendation:** Option B is simpler. The function body stays in tui_window.c (owns timing statics), the call stays at PostRender. The CONTEXT.md decision says the window system owns all timing -- the function IS in tui_window.c, so this satisfies the decision.

### TUI_FrameState Global Decision

CONTEXT.md says this is at Claude's discretion. Analysis:

- `TUI_FrameState` contains: `frame_count`, `delta_time`, `fps`, `in_frame`
- `delta_time` and `fps` (as `actual_fps`) are already in `NCurses_WindowState`
- `frame_count` is not exposed in WindowState but could be added
- `in_frame` is a debug assertion aid, not needed with ECS systems

**Recommendation:** Remove TUI_FrameState entirely. It is fully redundant with NCurses_WindowState. The `in_frame` guard is unnecessary since ECS phase ordering guarantees correct begin/end bracketing. If `frame_count` is needed later, add it to NCurses_WindowState.

### TUI_Layer Backward Compatibility Decision

CONTEXT.md says this is at Claude's discretion.

- `TUI_Layer` struct is defined in `cels_ncurses_draw.h` (typedef struct TUI_Layer)
- Only consumers: `draw_test.c` (being deleted) and `tui_layer.c` (being deleted)
- No external consumers of the v1.0 API exist (this is pre-v0.2.0 refactor)
- `TUI_LayerConfig` is the entity-driven replacement (different type, different name)

**Recommendation:** No backward-compat period needed. Delete the TUI_Layer typedef and all associated function declarations from the header cleanly. There are no external consumers.

## Open Questions

1. **System ordering within PreRender**
   - What we know: TUI_LayerSystem and TUI_FrameBeginSystem both target PreRender. NCurses_WindowUpdateSystem also targets PreRender.
   - What's unclear: Does CELS guarantee intra-phase ordering based on registration order? The working assumption (from all prior phases) is yes -- systems run in registration order within a phase.
   - Recommendation: Define and register in the correct order: NCurses_WindowUpdateSystem (forward-declared, registered via cels_register), TUI_LayerSystem, TUI_FrameBeginSystem. If ordering issues arise, the TUI_FrameBeginSystem SIGWINCH block could be folded into TUI_LayerSystem as a final step.

2. **Whether tui_hook_frame_end needs renaming or inlining**
   - What we know: The function body is 12 lines of simple timing code in tui_window.c.
   - What's unclear: Whether to keep it as a function call or inline the body into TUI_FrameEndSystem.
   - Recommendation: Keep as a function call. It accesses static variables (`g_frame_start`, `g_target_fps`) in tui_window.c that shouldn't move.

3. **clearok(curscr, TRUE) on resize**
   - What we know: tui_input.c calls `clearok(curscr, TRUE)` after KEY_RESIZE. NCurses_WindowUpdateSystem calls it in PTY mode. The TUI_LayerSystem does not call it.
   - What's unclear: Whether clearok is needed after entity layer resize (TUI_LayerSystem already calls werase on each layer).
   - Recommendation: Keep clearok in NCurses_WindowUpdateSystem for both resize paths (PTY and SIGWINCH). Remove it from input system since that handler is being stripped.

## Sources

### Primary (HIGH confidence)
- `src/frame/tui_frame.c` -- read in full (241 lines), all logic mapped
- `src/layer/tui_layer.c` -- read in full (223 lines), all functions inventoried
- `src/layer/tui_layer_panel.c` -- read in full (135 lines), helper functions confirmed
- `src/ncurses_module.c` -- read in full (167 lines), current system/lifecycle pattern confirmed
- `src/window/tui_window.c` -- read in full (335 lines), timing/throttle/resize logic mapped
- `src/input/tui_input.c` -- read in full (194 lines), KEY_RESIZE handler identified
- `src/tui_internal.h` -- read in full (66 lines), forward declarations confirmed
- `src/tui_window.h` -- read in full (49 lines), tui_hook_frame_end declaration confirmed
- `include/cels_ncurses.h` -- read in full (205 lines), public API surface audited
- `include/cels_ncurses_draw.h` -- read in full (609 lines), legacy sections identified
- `examples/minimal.c` -- read in full (117 lines), reference pattern confirmed
- `examples/draw_test.c` -- read first 150 lines + grep for all legacy API usage (50+ references)
- `CMakeLists.txt` -- read in full (117 lines), both targets and source lists mapped
- `cels/include/cels/cels.h` -- CELS phase enum confirmed: OnLoad, LifecycleEval, OnUpdate, StateSync, Recompose, PreRender, OnRender, PostRender
- `git diff` -- all unstaged changes from Phase 4 refactoring reviewed

### Secondary (MEDIUM confidence)
- Working tree state analysis (git status/diff) -- Phase 4 refactoring partially applied but uncommitted

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, purely internal refactoring
- Architecture: HIGH -- all source files read, all system patterns observed, all dependencies mapped
- Pitfalls: HIGH -- each pitfall derived from concrete code references with line numbers

**Research date:** 2026-03-14
**Valid until:** indefinite (internal codebase research, no external dependencies to go stale)
