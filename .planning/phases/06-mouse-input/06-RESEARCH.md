# Phase 6: Mouse Input - Research

**Researched:** 2026-02-20
**Domain:** ncurses mouse API, input system integration
**Confidence:** HIGH

## Summary

Phase 6 adds raw mouse primitives to the existing TUI_Input system. The scope has been explicitly narrowed by CONTEXT.md: pollable mouse position (cell coordinates), plus button press/release events for left/middle/right buttons. No scroll wheel, no modifier keys, no higher-level logic (hit testing, drag detection, hover enter/leave).

The ncurses mouse API is mature and well-documented. Mouse events arrive through the same `wgetch()` path as keyboard input -- when a mouse event occurs, `wgetch()` returns the sentinel value `KEY_MOUSE`, and the application calls `getmouse()` to retrieve the MEVENT struct containing coordinates and button state. This maps cleanly to the existing input loop in `tui_input.c`.

The key design challenge is implementing "pollable position" when ncurses only provides event-driven mouse position updates. The solution is to enable `REPORT_MOUSE_POSITION` in the mouse mask and maintain a static `(mouse_x, mouse_y)` pair updated on every mouse event. Consumers read the stored coordinates on demand -- no move events flood the input struct.

**Primary recommendation:** Add a `case KEY_MOUSE:` branch to the existing `tui_read_input_ncurses()` function. Call `mousemask()` during `TUI_Input_use()` initialization. Store mouse position in static globals. Expose button events through new fields on the input struct.

## Standard Stack

No new libraries required. This phase uses only ncurses built-in mouse support.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses | 6.5 (system) | Mouse event API | Already the foundation of cels-ncurses |

### Key ncurses Mouse Functions
| Function | Signature | Purpose |
|----------|-----------|---------|
| `mousemask()` | `mmask_t mousemask(mmask_t newmask, mmask_t *oldmask)` | Enable desired mouse event types |
| `getmouse()` | `int getmouse(MEVENT *event)` | Retrieve mouse event from queue (returns OK/ERR) |
| `mouseinterval()` | `int mouseinterval(int erval)` | Set click detection timeout (0 = disable clicks) |
| `wenclose()` | `bool wenclose(const WINDOW*, int y, int x)` | Test if screen coords are inside a window (future use) |
| `wmouse_trafo()` | `bool wmouse_trafo(const WINDOW*, int *y, int *x, bool to_screen)` | Convert screen coords to window-local (future use) |

### MEVENT Struct (from `/usr/include/ncurses.h`)
```c
typedef struct {
    short id;       /* ID to distinguish multiple devices */
    int x, y, z;    /* event coordinates (character-cell, SCREEN-relative) */
    mmask_t bstate; /* button state bits */
} MEVENT;
```

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| ncurses mouse API | Raw xterm escape sequences | Would break ncurses internal state; unsupported |
| REPORT_MOUSE_POSITION for tracking | No position tracking | Simpler but user can't poll mouse position |

## Architecture Patterns

### Pattern 1: Mouse Mask Configuration

**What:** Call `mousemask()` during `TUI_Input_use()` to enable the desired mouse event types.

**When to use:** Once during initialization, after ncurses is initialized.

**Details:**

The mask for the reduced scope (press + release for buttons 1-3, plus position reporting):

```c
// Source: ncurses curs_mouse(3x) man page + /usr/include/ncurses.h
mmask_t desired = BUTTON1_PRESSED | BUTTON1_RELEASED
                | BUTTON2_PRESSED | BUTTON2_RELEASED
                | BUTTON3_PRESSED | BUTTON3_RELEASED
                | REPORT_MOUSE_POSITION;

mmask_t actual = mousemask(desired, NULL);
```

**CRITICAL:** `mouseinterval(0)` must be called to disable click detection. Without this, ncurses internally delays press/release events to detect clicks/double-clicks. Since the user wants raw press + release as separate events, click detection must be disabled.

```c
mouseinterval(0);  // Disable click resolution -- raw press/release only
```

**CRITICAL:** `ALL_MOUSE_EVENTS` does NOT include `REPORT_MOUSE_POSITION`. They are distinct. If position tracking is desired, `REPORT_MOUSE_POSITION` must be explicitly OR'd into the mask.

### Pattern 2: Mouse Event Handling in Input Loop

**What:** Add a `case KEY_MOUSE:` branch to the existing `tui_read_input_ncurses()` switch statement.

**When to use:** Every frame when `wgetch()` returns `KEY_MOUSE`.

**Example:**
```c
// Source: ncurses curs_mouse(3x) man page
case KEY_MOUSE: {
    MEVENT event;
    if (getmouse(&event) == OK) {
        // Always update stored position (screen-relative cell coords)
        g_mouse_x = event.x;
        g_mouse_y = event.y;

        // Check button state bits
        if (event.bstate & BUTTON1_PRESSED) {
            input.mouse_button = TUI_MOUSE_LEFT;
            input.mouse_pressed = true;
        } else if (event.bstate & BUTTON1_RELEASED) {
            input.mouse_button = TUI_MOUSE_LEFT;
            input.mouse_released = true;
        }
        // ... repeat for BUTTON2 (middle) and BUTTON3 (right)
    }
    break;
}
```

### Pattern 3: Pollable Mouse Position via Static Globals

**What:** Maintain `g_mouse_x` and `g_mouse_y` as static variables updated on every mouse event. Expose them through a getter function or through the input struct.

**Why:** ncurses does not provide a "get current mouse position" API. Position is only reported through events. By enabling `REPORT_MOUSE_POSITION`, every mouse motion generates a `KEY_MOUSE` event with updated coordinates. The input loop stores the latest coordinates, and consumers read them whenever they want.

**Implementation options:**
1. **Static globals with getter function** (recommended): `tui_mouse_get_position(int *x, int *y)` reads from `g_mouse_x`/`g_mouse_y`.
2. **Fields on input struct**: Add `mouse_x`, `mouse_y` to the input struct, populated every frame.

Option 1 is cleaner because position is always valid (not just when there's a button event), and it separates the "what is the mouse at right now" query from "what happened this frame" events.

### Pattern 4: Terminal Mouse Reporting Escape Sequence

**What:** Some terminals require explicit escape sequences to enable mouse motion tracking beyond what ncurses' `mousemask()` provides.

**When to use:** When `REPORT_MOUSE_POSITION` via `mousemask()` alone does not produce motion events on the target terminal.

```c
// Enable "any event" mouse tracking (mode 1003)
printf("\033[?1003h\n");
// ... at shutdown:
printf("\033[?1003l\n");
```

**WARNING:** ncurses may handle this internally via the XM terminfo capability. If `mousemask(REPORT_MOUSE_POSITION, NULL)` returns the bit set, the terminal supports it natively through ncurses. The escape sequence is a fallback for terminals where ncurses' detection fails.

**The cleanup (`\033[?1003l`) MUST happen on exit** or the terminal will continue sending mouse escape sequences after the program ends. Use `atexit()` or ensure it runs in the shutdown hook.

### Pattern 5: Multiple Mouse Events Per Frame

**What:** Multiple mouse events can queue between frames. The input loop currently reads only one `wgetch()` per frame.

**When to use:** Decide whether to drain all mouse events or process one per frame.

**Recommendation:** For the reduced scope (pollable position + button events):
- Drain all `KEY_MOUSE` events in a single frame to get the final mouse position
- Only report the LAST button event (if multiple, last wins -- prevents double-press artifacts)
- This matches the user's "position is a pollable field" intent -- always have the latest position

```c
// Drain all queued mouse events, keep last button event + final position
while (ch == KEY_MOUSE) {
    MEVENT event;
    if (getmouse(&event) == OK) {
        g_mouse_x = event.x;
        g_mouse_y = event.y;
        // Update button state from this event...
    }
    ch = wgetch(stdscr);
}
if (ch != ERR) ungetch(ch);  // Put back non-mouse key
```

### Anti-Patterns to Avoid

- **Requesting BUTTON1_CLICKED instead of PRESSED+RELEASED:** Click detection introduces latency via `mouseinterval`. Since consumers will build their own click/drag logic, raw press/release is correct.
- **Not calling mouseinterval(0):** Default mouseinterval is ~166ms. Without disabling it, button events will be delayed or coalesced into clicks.
- **Using ALL_MOUSE_EVENTS alone:** This does NOT include REPORT_MOUSE_POSITION. You will get button events but NO position tracking.
- **Not draining queued mouse events:** At 60fps with mouse motion tracking enabled, many motion events will queue. Reading only one per frame causes position lag.

### Recommended Project Structure Changes

```
include/cels-ncurses/
    tui_input.h          # Add mouse getter; no structural change
src/input/
    tui_input.c          # Add KEY_MOUSE handling + mouse state globals
```

No new files needed. All changes are additions to existing files.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Mouse event parsing | Custom xterm escape sequence parser | `getmouse()` + `mousemask()` | ncurses handles all terminal-specific encoding |
| Screen-to-window coordinate transform | Manual subtraction from layer x/y | `wmouse_trafo()` or `wenclose()` | Correct for all panel configurations (deferred) |
| Click/double-click detection | Custom timer-based detection | `mouseinterval()` + CLICKED constants | Already built into ncurses (but we disable it for raw mode) |

**Key insight:** ncurses wraps all terminal-specific mouse protocols (xterm, SGR, etc.) behind a uniform MEVENT interface. Never bypass this by writing raw escape sequences.

## Common Pitfalls

### Pitfall 1: mouseinterval Default Causes Delayed Events

**What goes wrong:** Button press events arrive ~166ms late because ncurses waits to see if a release follows quickly (to report CLICKED instead of PRESSED+RELEASED).
**Why it happens:** Default mouseinterval is 1/6th of a second.
**How to avoid:** Call `mouseinterval(0)` immediately after `mousemask()`.
**Warning signs:** Button events feel sluggish; BUTTON1_CLICKED events appear instead of BUTTON1_PRESSED.

### Pitfall 2: keypad() Must Be Enabled

**What goes wrong:** Mouse events are not detected at all. `wgetch()` never returns `KEY_MOUSE`.
**Why it happens:** Mouse event reports from xterm are encoded like function keys. Without `keypad(stdscr, TRUE)`, ncurses doesn't decode them.
**How to avoid:** Verify `keypad(stdscr, TRUE)` is already called (it is -- in `tui_hook_startup()`).
**Warning signs:** No mouse events received despite correct `mousemask()` call.

### Pitfall 3: MEVENT Coordinates Are Screen-Relative

**What goes wrong:** Mouse coordinates don't match expected widget/layer positions.
**Why it happens:** `MEVENT.x` and `MEVENT.y` are screen-relative (stdscr coordinates), not window-relative. When layers are positioned at non-zero offsets, raw coordinates need adjustment.
**How to avoid:** For this phase (raw primitives), expose screen coordinates as-is. Document that coordinates are screen-relative. Layer-local translation is deferred.
**Warning signs:** Click positions are offset from visual elements.

### Pitfall 4: Mouse Motion Event Flood

**What goes wrong:** Performance degradation from processing hundreds of mouse motion events per second.
**Why it happens:** `REPORT_MOUSE_POSITION` generates a KEY_MOUSE for every cell the mouse crosses.
**How to avoid:** Drain all queued mouse events per frame (see Pattern 5). Only the final position matters.
**Warning signs:** Input processing takes disproportionate CPU time; position updates lag behind actual mouse position.

### Pitfall 5: Terminal Mouse Reporting Not Disabled on Exit

**What goes wrong:** After program crash or abnormal exit, the terminal continues printing mouse escape sequences for every mouse movement.
**Why it happens:** If `\033[?1003h` was sent to enable tracking and the program exits without sending `\033[?1003l`, the terminal stays in tracking mode. Even ncurses' `endwin()` may not always clean this up.
**How to avoid:** Register cleanup via `atexit()`. The existing `cleanup_endwin()` handler calls `endwin()` which should restore terminal state. Test that `endwin()` correctly disables mouse reporting on the target terminal. If it doesn't, add an explicit `printf("\033[?1003l")` in the cleanup.
**Warning signs:** Garbage characters in terminal after program exit when moving the mouse.

### Pitfall 6: CELS_Input Struct Is Currently Removed From Core

**What goes wrong:** The `CELS_Input` struct, `cels_input_set()`, and `cels_input_get()` were removed from `<cels/backend.h>` in the Phase 26 refactoring of the parent cels repo. The project currently does not compile.
**Why it happens:** The core cels library gutted backend.h (commit 052aabf). The struct definition, cels_input_set, and cels_input_get no longer exist anywhere in the cels codebase.
**How to avoid:** Before adding mouse fields to CELS_Input, the input struct definition must be restored somewhere. Two options:
  1. Restore it in the cels core (undo part of the Phase 26 refactoring)
  2. Define a local input struct within cels-ncurses and use module-local storage instead of cels_input_set/get
**Warning signs:** Build failures referencing unknown type CELS_Input; linker errors for cels_input_set/cels_input_get.

## Code Examples

### Complete Mouse Initialization

```c
// Source: ncurses curs_mouse(3x) man page, verified against /usr/include/ncurses.h

// In TUI_Input_use(), after other initialization:

// Enable mouse events: buttons 1-3 press/release + position tracking
mmask_t desired = BUTTON1_PRESSED | BUTTON1_RELEASED
                | BUTTON2_PRESSED | BUTTON2_RELEASED
                | BUTTON3_PRESSED | BUTTON3_RELEASED
                | REPORT_MOUSE_POSITION;
mmask_t actual = mousemask(desired, NULL);

// Disable click detection -- we want raw press + release
mouseinterval(0);

// Optional: enable xterm "any event" mouse tracking if ncurses didn't
// Note: Only needed if REPORT_MOUSE_POSITION was not returned in actual
if (!(actual & REPORT_MOUSE_POSITION)) {
    printf("\033[?1003h");
    fflush(stdout);
    // Register cleanup for abnormal exit
}
```

### Complete Mouse Event Processing

```c
// In tui_read_input_ncurses(), inside the switch(ch):

case KEY_MOUSE: {
    MEVENT event;
    if (getmouse(&event) != OK) break;

    // Update stored position (always, for polling)
    g_mouse_x = event.x;
    g_mouse_y = event.y;

    // Decode button events
    if (event.bstate & BUTTON1_PRESSED) {
        input.mouse_button = 1;  // Left
        input.mouse_pressed = true;
    } else if (event.bstate & BUTTON1_RELEASED) {
        input.mouse_button = 1;
        input.mouse_released = true;
    } else if (event.bstate & BUTTON2_PRESSED) {
        input.mouse_button = 2;  // Middle
        input.mouse_pressed = true;
    } else if (event.bstate & BUTTON2_RELEASED) {
        input.mouse_button = 2;
        input.mouse_released = true;
    } else if (event.bstate & BUTTON3_PRESSED) {
        input.mouse_button = 3;  // Right
        input.mouse_pressed = true;
    } else if (event.bstate & BUTTON3_RELEASED) {
        input.mouse_button = 3;
        input.mouse_released = true;
    }
    // else: motion-only event (no button state change)
    break;
}
```

### Mouse Position Getter

```c
// Static state in tui_input.c
static int g_mouse_x = -1;  // -1 = no position yet
static int g_mouse_y = -1;

// Public API
void tui_mouse_get_position(int *x, int *y) {
    if (x) *x = g_mouse_x;
    if (y) *y = g_mouse_y;
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| NCURSES_MOUSE_VERSION 1 (buttons 1-3 only) | NCURSES_MOUSE_VERSION 2 (buttons 1-5, scroll = button 4/5) | ncurses 6.x | System has v2, BUTTON4/5 available for future scroll |
| xterm basic mouse (mode 1000, 223 col limit) | SGR extended mouse (mode 1006, no coordinate limit) | ncurses 6.x | Large terminals (>223 cols) get correct coordinates |

**Current system:** ncurses 6.5 with NCURSES_MOUSE_VERSION=2 on Arch Linux. All modern mouse features are available.

**Deprecated/outdated:**
- BUTTON4 for "shift + button 1" (NCURSES_MOUSE_VERSION 1 mapping): Replaced by BUTTON4 = scroll up in v2
- Manual xterm escape sequence parsing: ncurses handles all terminal protocols internally

## Open Questions

1. **CELS_Input struct restoration**
   - What we know: The struct and its accessor functions were removed from the cels core in Phase 26 refactoring. The cels-ncurses module references them but the project does not compile.
   - What's unclear: Whether the core team plans to restore these types or replace them with a different pattern. The working tree has uncommitted changes that suggest work is in progress.
   - Recommendation: Treat this as a prerequisite. Before adding mouse fields, either restore CELS_Input in core or define a module-local input struct. The planner should make this Task 1.

2. **Whether to expose button-held state**
   - What we know: The CONTEXT.md says "Claude's discretion: whether to expose button state (currently held) alongside events."
   - What's unclear: Whether consumers need "is button currently held down" in addition to press/release events.
   - Recommendation: YES -- track held state for each button. It's trivial (set on press, clear on release) and enables consumers to check `is_left_held` without maintaining their own state machine. This is the difference between "button 1 was pressed this frame" and "button 1 is currently down."

3. **printf escape sequence for mouse tracking**
   - What we know: Some implementations use `printf("\033[?1003h")` to enable xterm "any event" tracking. ncurses may or may not do this automatically via the XM terminfo capability.
   - What's unclear: Whether `mousemask(REPORT_MOUSE_POSITION, NULL)` alone is sufficient on Kitty (the user's terminal, per the F1 pause mode code referencing it).
   - Recommendation: Start with `mousemask()` only. If testing shows position events aren't received, add the escape sequence with proper cleanup. This should be verified during implementation.

## Sources

### Primary (HIGH confidence)
- ncurses curs_mouse(3x) man page - [invisible-island.net/ncurses/man/curs_mouse.3x.html](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) - Full API reference, event constants, behavior
- `/usr/include/ncurses.h` on target system - MEVENT struct, NCURSES_MOUSE_VERSION=2, all button constants verified
- Existing cels-ncurses source code - `tui_input.c`, `tui_window.c`, `tui_layer.h` - Current architecture and patterns

### Secondary (MEDIUM confidence)
- TLDP ncurses mouse tutorial - [tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/mouse.html) - Practical usage patterns
- Mouse movement ncurses gist - [gist.github.com/sylt/93d3f7b77e7f3a881603](https://gist.github.com/sylt/93d3f7b77e7f3a881603) - Working example of REPORT_MOUSE_POSITION with escape sequence
- Debian ncurses man pages - [manpages.debian.org/testing/ncurses-doc/mouse.3ncurses.en.html](https://manpages.debian.org/testing/ncurses-doc/mouse.3ncurses.en.html) - Additional documentation

### Tertiary (LOW confidence)
- Arch Linux forum threads on ncurses mouse issues - Known issues with specific terminal configurations

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - ncurses mouse API is well-documented and verified against local system headers
- Architecture: HIGH - Patterns verified against existing input loop code and ncurses documentation
- Pitfalls: HIGH - Common issues well-documented across multiple sources and verified
- CELS_Input migration: MEDIUM - Structural dependency on core cels refactoring; exact resolution path unclear

**Research date:** 2026-02-20
**Valid until:** Indefinite for ncurses API; 7 days for CELS_Input struct status (actively changing)
