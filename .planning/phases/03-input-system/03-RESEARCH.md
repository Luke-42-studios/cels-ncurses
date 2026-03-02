# Phase 3: Input System - Research

**Researched:** 2026-03-01
**Domain:** NCurses input handling as a CELS ECS component/system, ncurses getch/mouse API, ECS pipeline phase ordering
**Confidence:** HIGH

## Summary

This phase transforms the existing input system from a global-state-with-function-getter pattern (`g_input_state` + `tui_input_get_state()`) into a CELS component-based pattern where input state lives on the window entity as an `NCurses_InputState` component, written by a system running at the `OnLoad` phase. The existing `tui_input.c` has a complete, battle-tested ncurses input implementation covering keyboard mapping (arrow keys, WASD, action buttons, navigation keys, function keys, raw passthrough) and mouse handling (3-button press/release, position tracking, held state, drain loops). This implementation should be preserved and adapted, not rewritten.

The key architectural shift is: instead of `extern TUI_InputState g_input_state` accessed via `tui_input_get_state()`, the input state becomes an ECS component on the window entity that the developer reads via `cels_entity_get_component()` or `cel_watch()`. The system still runs at `OnLoad` (before user systems at `OnUpdate`), drains the ncurses input queue via `wgetch(stdscr)`, and writes the result to the entity via `cels_entity_set_component()` + `cels_component_notify_change()`. The quit-on-'q' behavior transitions fully to `cels_request_quit()`, removing the `g_running` pointer bridge.

The existing code already has the correct ncurses input patterns: `nodelay(stdscr, TRUE)` for non-blocking reads, `keypad(stdscr, TRUE)` for function key decoding, `ESCDELAY = 25` for fast escape processing, `mousemask()` for button events, and a drain loop for KEY_MOUSE to prevent position lag. The Phase 3 work is about changing the storage and access pattern (global to component), not the input reading logic itself.

**Primary recommendation:** Adapt the existing `tui_input.c` to write `NCurses_InputState` onto the window entity each frame via `cels_entity_set_component()` + `cels_component_notify_change()`. Define `NCurses_InputState` as a `CEL_Component` in `tui_ncurses.h`. Remove the global extern `g_input_state`, the getter functions, and the `g_running` pointer bridge. Keep the `tui_read_input_ncurses()` logic nearly unchanged.

## Standard Stack

### Core (no new dependencies)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses (wide) | System | getch/wgetch, getmouse, mousemask, KEY_* constants | Already used -- input reading is pure ncurses |
| CELS | v0.5.2+ | CEL_Component, CEL_System, cels_entity_set_component | Already used -- component/system patterns established in P1.2 |

### Internal (files being modified)

| File | Current Purpose | Phase 3 Action |
|------|----------------|----------------|
| `src/input/tui_input.c` | Input system with global TUI_InputState | **Rewrite** to write NCurses_InputState to entity |
| `include/cels-ncurses/tui_input.h` | TUI_InputState struct, getters, backward compat | **Replace** with thin internal header or merge into tui_ncurses.h |
| `include/cels-ncurses/tui_ncurses.h` | Public component types + module declaration | **Add** NCurses_InputState component + extern ID |
| `src/ncurses_module.c` | Module init body | **Add** NCurses_InputState_id global definition |
| `src/window/tui_window.c` | Window with g_running pointer bridge | **Remove** tui_window_get_running_ptr() |
| `include/cels-ncurses/tui_window.h` | Internal window header | **Remove** tui_window_get_running_ptr() declaration |
| `include/cels-ncurses/ncurses.h` | Umbrella header | **No change** (tui_input.h not currently included) |
| `examples/minimal.c` | Consumer example | **Update** to read NCurses_InputState via cel_watch |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Component on window entity | Separate input entity | Extra entity adds complexity with no benefit -- input is 1:1 with the window entity. Window entity pattern is established. |
| OnLoad phase | Custom input phase via cel_phase() | OnLoad is already the correct built-in phase for input (runs before OnUpdate where user systems execute). No custom phase needed. |
| cels_entity_set_component each frame | Direct component mutation in system query | Input system is queryless (cel_run), writes to a known entity. set_component + notify_change matches P1.2 established pattern. |

## Architecture Patterns

### Component Design: NCurses_InputState

The new component replaces the global `TUI_InputState`. The field layout should follow the existing struct closely but with v0.2.0 naming conventions:

```c
// Source: existing tui_input.h TUI_InputState, adapted to CEL_Component pattern
CEL_Component(NCurses_InputState) {
    /* Keyboard: directional input (-1.0/0.0/1.0 for x/y) */
    float axis_x;
    float axis_y;

    /* Keyboard: action buttons (per-frame, reset each frame) */
    bool accept;        /* Enter/Return */
    bool cancel;        /* Escape */

    /* Keyboard: navigation (per-frame) */
    bool tab;
    bool shift_tab;
    bool home;
    bool end;
    bool page_up;
    bool page_down;
    bool backspace;
    bool delete_key;

    /* Keyboard: number keys (per-frame) */
    int  number;
    bool has_number;

    /* Keyboard: function keys (per-frame) */
    int  function_key;
    bool has_function;

    /* Keyboard: raw key passthrough (per-frame) */
    int  raw_key;
    bool has_raw_key;

    /* Mouse: position (persistent across frames) */
    int mouse_x;
    int mouse_y;

    /* Mouse: button event (per-frame) */
    int  mouse_button;   /* 0=none, 1=left, 2=middle, 3=right */
    bool mouse_pressed;
    bool mouse_released;

    /* Mouse: held state (persistent across frames) */
    bool mouse_left_held;
    bool mouse_middle_held;
    bool mouse_right_held;
};
```

### Pattern 1: Input System as Queryless CEL_System at OnLoad

The system uses `cel_run` (no query) because it writes to a known entity rather than iterating matched entities. This matches the existing pattern from Phase 1.2.

```c
// Source: established pattern in tui_input.c and tui_frame.c
CEL_System(NCurses_InputSystem, .phase = OnLoad) {
    cel_run {
        ncurses_read_input();  // internal function, drains getch queue
    }
}
```

### Pattern 2: Component Mutation via set_component + notify_change

Every frame, the input system writes fresh state to the window entity. This follows the pattern established in `tui_window.c` for `NCurses_WindowState`.

```c
// Source: established pattern in tui_window.c ncurses_window_frame_update()
NCurses_InputState state = { /* ... filled from getch reads ... */ };
cels_entity_set_component(window_entity, NCurses_InputState_id,
                           &state, sizeof(NCurses_InputState));
cels_component_notify_change(NCurses_InputState_id);
```

### Pattern 3: Extern Component ID Override for INTERFACE Library

The component ID must be shared across translation units. This follows the exact pattern used for `NCurses_WindowConfig_id` and `NCurses_WindowState_id`.

```c
// In tui_ncurses.h:
extern cels_entity_t _ncurses_InputState_id;
#define NCurses_InputState_id _ncurses_InputState_id

// In ncurses_module.c:
cels_entity_t _ncurses_InputState_id = 0;
```

### Pattern 4: Developer Reads Input via cel_watch or cels_entity_get_component

```c
// Source: established pattern in minimal.c for NCurses_WindowState
CEL_Composition(World) {
    cels_entity_t win = 0;
    NCursesWindow(.title = "App", .fps = 60) {
        win = cels_get_current_entity();
    }

    // Reactive: recomposes when input changes each frame
    const NCurses_InputState* input = cel_watch(win, NCurses_InputState);
    if (input && input->accept) {
        // Handle enter key
    }

    // OR: polling (non-reactive read):
    const NCurses_InputState* input =
        (const NCurses_InputState*)cels_entity_get_component(win, NCurses_InputState_id);
}
```

### Pattern 5: Fresh State Each Frame

Input state must not leak between frames. The pattern is: build a fresh struct on the stack each frame, copy persistent fields (mouse position, held buttons) from the previous component state, then drain getch into it.

```c
static void ncurses_read_input(void) {
    cels_entity_t win = ncurses_window_get_entity();
    if (win == 0) return;

    // Read previous state for persistent fields (mouse pos, held buttons)
    const NCurses_InputState* prev =
        (const NCurses_InputState*)cels_entity_get_component(win, NCurses_InputState_id);

    // Build fresh state (all per-frame fields zeroed)
    NCurses_InputState state = {0};

    // Copy persistent fields from previous frame
    if (prev) {
        state.mouse_x = prev->mouse_x;
        state.mouse_y = prev->mouse_y;
        state.mouse_left_held = prev->mouse_left_held;
        state.mouse_middle_held = prev->mouse_middle_held;
        state.mouse_right_held = prev->mouse_right_held;
    }

    // Drain getch queue into state...
    int ch = wgetch(stdscr);
    // ... (existing switch/case logic preserved)

    // Write to entity
    cels_entity_set_component(win, NCurses_InputState_id,
                               &state, sizeof(NCurses_InputState));
    cels_component_notify_change(NCurses_InputState_id);
}
```

### Anti-Patterns to Avoid

- **Global extern state:** Do NOT keep `extern TUI_InputState g_input_state`. Input state belongs on the entity. The global prevents multiple windows and breaks ECS data locality.
- **Querying input from systems:** Do NOT use `tui_input_get_state()` getter functions. Use `cels_entity_get_component()` or `cel_watch()` instead.
- **Resetting in place:** Do NOT memset the component pointer. Build a fresh struct on the stack and `cels_entity_set_component()` the whole thing.
- **Forgetting notify_change:** Every `cels_entity_set_component()` MUST be paired with `cels_component_notify_change()` or `cel_watch` reactivity breaks.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Escape sequence parsing | Custom terminal parser | ncurses keypad() + define_key() | ncurses handles hundreds of terminal escape sequences correctly |
| Mouse event decoding | Custom mouse protocol handler | ncurses mousemask() + getmouse() | Handles xterm, gpm, sysmouse mouse protocols |
| Non-blocking input | select()/poll() on stdin | ncurses nodelay(stdscr, TRUE) + wgetch() | Integrates with ncurses input buffer, no raw fd manipulation |
| Input queue drain | Custom ring buffer | wgetch() loop until ERR | ncurses manages its own input buffer; just drain it |
| Key code mapping | Custom scancode table | ncurses KEY_* constants + keypad() | Cross-terminal key code abstraction |

**Key insight:** The ncurses input API is complete and battle-tested. The Phase 3 work is about storage and access patterns (global -> component), not input reading logic.

## Common Pitfalls

### Pitfall 1: Notify Without Change Detection

**What goes wrong:** Calling `cels_component_notify_change()` every frame causes `cel_watch()` to trigger recomposition every frame, even when no input occurred.
**Why it happens:** The window state pattern always notifies, but window state rarely changes. Input changes every frame when keys are pressed but is zero-state most frames.
**How to avoid:** Always call notify_change. The composition engine and cel_watch should handle this efficiently. The alternative (diffing state) adds complexity for marginal gain. If performance becomes an issue later, add change detection.
**Warning signs:** Excessive recomposition in idle state, high CPU when no input.

### Pitfall 2: Mouse Position Reset on No-Input Frames

**What goes wrong:** Mouse position (mouse_x, mouse_y) resets to 0 or -1 on frames with no mouse event, causing UI elements to lose track of cursor.
**Why it happens:** Building fresh state with `{0}` zeros mouse position.
**How to avoid:** Copy mouse_x, mouse_y, and held button states from the previous frame's component data before draining getch. These are persistent fields.
**Warning signs:** Mouse cursor jumps to (0,0) when no mouse movement occurs.

### Pitfall 3: Stale KEY_MOUSE Events Causing Input Lag

**What goes wrong:** Mouse position lags behind actual cursor, especially during fast movement.
**Why it happens:** ncurses queues multiple KEY_MOUSE events per frame during rapid movement. Processing only one per frame leaves stale events in the queue.
**How to avoid:** Drain ALL KEY_MOUSE events in a loop (existing pattern: `do { getmouse(); } while ((ch = wgetch(stdscr)) == KEY_MOUSE)`). Keep last position.
**Warning signs:** Mouse cursor follows movement with 2-3 frame delay.

### Pitfall 4: g_running Pointer Bridge Removal

**What goes wrong:** Removing `tui_window_get_running_ptr()` without updating quit signaling breaks 'q' key quit behavior.
**Why it happens:** The old input system sets `*g_tui_running_ptr = 0` to signal the window to shut down. The new pattern uses `cels_request_quit()` exclusively.
**How to avoid:** The existing code already calls `cels_request_quit()` on 'q' press (added in P1-02). Simply remove the redundant `g_running_ptr` write. Verify quit still works via the CELS session loop.
**Warning signs:** 'q' key no longer exits the application, or exits twice.

### Pitfall 5: define_key and mousemask Before initscr

**What goes wrong:** Custom key sequences and mouse events don't work.
**Why it happens:** `ncurses_register_input_system()` is called during module init, before the window lifecycle observer fires `initscr()`. Key definitions registered before initscr are silently ignored in some ncurses versions.
**How to avoid:** Defer `define_key()` and `mousemask()` calls to the first frame when the system actually runs (guard with `ncurses_window_is_active()` check), or call them from the on_create observer after initscr.
**Warning signs:** Ctrl+Arrow keys don't work, mouse events not received. The existing code notes this issue in comments but claims it works in practice.

### Pitfall 6: NCurses_InputState Component Not Declared in Composition

**What goes wrong:** `cels_entity_get_component()` returns NULL for input state.
**Why it happens:** The component must be declared on the entity via `cel_has(NCurses_InputState, ...)` in the NCursesWindow composition before the system can `cels_entity_set_component()` on it.
**How to avoid:** Add `cel_has(NCurses_InputState, ...)` with zeroed defaults to the `CEL_Compose(NCursesWindow)` block, following the `NCurses_WindowState` pattern. This ensures the component ID is registered and the entity has the component from creation.
**Warning signs:** Input system writes fail silently, developer gets NULL from cel_watch.

## Code Examples

### NCurses Input Queue Drain (existing pattern to preserve)

```c
// Source: tui_input.c tui_read_input_ncurses() (current implementation)
int ch = wgetch(stdscr);
if (ch == ERR) {
    return;  // No input this frame
}

switch (ch) {
    case KEY_UP:    state.axis_y = -1.0f; break;
    case KEY_DOWN:  state.axis_y =  1.0f; break;
    case KEY_LEFT:  state.axis_x = -1.0f; break;
    case KEY_RIGHT: state.axis_x =  1.0f; break;
    // ... (full switch/case preserved from existing code)

    case KEY_MOUSE: {
        MEVENT event;
        do {
            if (getmouse(&event) != OK) break;
            state.mouse_x = event.x;
            state.mouse_y = event.y;
            // ... decode button events from event.bstate
        } while ((ch = wgetch(stdscr)) == KEY_MOUSE);
        if (ch != ERR) ungetch(ch);
        break;
    }
}
```

### Component Registration in Module Init

```c
// Source: pattern from ncurses_module.c CEL_Module(NCurses)
CEL_Module(NCurses) {
    cels_register(NCursesWindowLC);
    ncurses_register_input_system();   // already called (strong symbol override)
    ncurses_register_frame_systems();
}
```

### Consumer Reading Input State

```c
// Source: pattern from minimal.c adapted for input
CEL_Composition(World) {
    cels_entity_t win = 0;
    NCursesWindow(.title = "Input Demo", .fps = 60) {
        win = cels_get_current_entity();
    }

    const NCurses_InputState* input = cel_watch(win, NCurses_InputState);
    const NCurses_WindowState* state = cel_watch(win, NCurses_WindowState);
    if (input && state) {
        mvprintw(0, 0, "Axis: %.0f, %.0f  Mouse: %d,%d",
                 input->axis_x, input->axis_y,
                 input->mouse_x, input->mouse_y);
        if (input->accept)
            mvprintw(1, 0, "ENTER pressed!");
    }
}
```

## State of the Art

| Old Approach (v1.0/v1.1) | Current Approach (v0.2.0) | When Changed | Impact |
|--------------------------|--------------------------|--------------|--------|
| `extern TUI_InputState g_input_state` | `NCurses_InputState` component on window entity | Phase 3 | Enables ECS queries, cel_watch reactivity, multi-window |
| `tui_input_get_state()` | `cels_entity_get_component(win, NCurses_InputState_id)` | Phase 3 | Consistent with NCurses_WindowState access pattern |
| `tui_window_get_running_ptr()` bridge | `cels_request_quit()` only | Phase 3 | Removes coupling between input and window statics |
| `TUI_InputState` typedef | `NCurses_InputState` CEL_Component | Phase 3 | Follows NCurses_ naming convention for module components |
| `CELS_Input` backward compat shim | Removed | Phase 3 | Dead code cleanup |

**Deprecated/outdated:**
- `TUI_InputState`: Replaced by `NCurses_InputState` CEL_Component
- `tui_input_get_state()`: Replaced by `cels_entity_get_component()` / `cel_watch()`
- `tui_input_get_mouse_position()`: Replaced by reading `mouse_x`/`mouse_y` from component
- `tui_input_set_quit_guard()`: Needs evaluation -- may be kept as internal API or moved to component config
- `g_input_state` extern: Eliminated -- state lives on entity
- `CELS_Input` / `cels_input_get()`: Legacy shims removed
- `tui_window_get_running_ptr()`: Eliminated -- use `cels_request_quit()`

## Open Questions

1. **Quit guard mechanism**
   - What we know: `tui_input_set_quit_guard()` lets cels-widgets suppress 'q' quit when text input is active. This is a callback function pointer stored in the input system.
   - What's unclear: Should this become a component field (e.g., `NCurses_InputConfig` with a quit_guard flag), or remain as an internal function pointer? Component-based approach is more ECS-pure but adds complexity.
   - Recommendation: Keep as internal function pointer for now. It is an internal API used by cels-widgets, not developer-facing. Can be componentized in a future phase if needed.

2. **KEY_RESIZE handling location**
   - What we know: The current tui_input.c handles KEY_RESIZE by calling `tui_layer_resize_all()` and `tui_frame_invalidate_all()`. But resize detection also happens in `ncurses_window_frame_update()` by checking COLS/LINES.
   - What's unclear: Should KEY_RESIZE handling stay in the input system (where it drains the event queue) or move to the frame pipeline? Both fire per-frame.
   - Recommendation: Keep KEY_RESIZE in the input system since it naturally appears in the getch drain loop. The input system runs at OnLoad (before PreRender), so resize invalidation happens before frame_begin clears layers. The COLS/LINES dimension update stays in window frame_update.

3. **cel_watch triggering every frame**
   - What we know: `cels_component_notify_change()` is called every frame. `cel_watch()` detects change and triggers recomposition.
   - What's unclear: Does cel_watch trigger on every notify, or does it diff the component data? If it triggers on every notify, any composition watching input will recompose 60 times per second.
   - Recommendation: Assume it triggers on every notify. This matches the existing WindowState behavior. If this becomes a performance problem, the solution is in the CELS framework (change detection), not in cels-ncurses. For now, the developer should use `cels_entity_get_component()` for polling and `cel_watch()` only for state changes.

4. **Initial component state before first frame**
   - What we know: `cel_has(NCurses_InputState, ...)` in the composition sets initial zeros. The input system writes real state starting from the first frame.
   - What's unclear: Is there a gap between entity creation and first system tick where a consumer reads the zero state?
   - Recommendation: The zero state is a valid "no input" state. This is the same as NCurses_WindowState where initial `.width = 0, .height = 0` is set in composition and real values arrive on first frame. Not a problem.

## Sources

### Primary (HIGH confidence)
- **Codebase analysis** -- Direct reading of all source files in the project
  - `src/input/tui_input.c` -- Complete existing input implementation (374 lines)
  - `include/cels-ncurses/tui_input.h` -- Current input type definitions (159 lines)
  - `src/ncurses_module.c` -- Module registration pattern with weak stubs
  - `src/window/tui_window.c` -- Window frame update and g_running bridge
  - `include/cels-ncurses/tui_ncurses.h` -- Public component types and extern ID pattern
  - `src/frame/tui_frame.c` -- CEL_System pattern for frame pipeline systems
  - `examples/minimal.c` -- Consumer pattern for cel_watch
- **CELS API** -- `include/cels/cels.h` and `include/cels/private/cels_system_impl.h`
  - Pipeline phases: OnLoad, OnUpdate, PreRender, OnRender, PostRender
  - CEL_System macro, cel_run, cel_phase patterns
  - cels_entity_set_component, cels_component_notify_change
  - Custom phase registration via cel_phase + cels_phase_register

### Secondary (MEDIUM confidence)
- [ncurses mouse man page (curs_mouse 3x)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- Authoritative documentation for MEVENT, getmouse, mousemask
- [NCURSES Programming HOWTO - Mouse](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html) -- Reference for mouse event handling patterns

### Tertiary (LOW confidence)
- None -- all findings based on direct codebase analysis and official ncurses documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- No new dependencies, all libraries already in use
- Architecture: HIGH -- Follows exact patterns established in Phase 1.2 (window component mutation)
- Pitfalls: HIGH -- Based on direct analysis of existing code and known behavior
- Component design: HIGH -- Direct adaptation of existing TUI_InputState struct

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable -- internal refactoring, no external dependency changes)
