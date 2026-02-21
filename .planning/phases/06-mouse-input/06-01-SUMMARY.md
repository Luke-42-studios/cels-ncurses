---
phase: 06-mouse-input
plan: 01
subsystem: input
tags: [ncurses, mouse, mousemask, input-state, extern-global]

# Dependency graph
requires:
  - phase: 05-stdscr-removal
    provides: Clean input system without stdscr drawing
provides:
  - TUI_InputState struct with keyboard + mouse fields
  - TUI_MouseButton enum (NONE, LEFT, MIDDLE, RIGHT)
  - g_input_state extern global (replaces removed CELS_Input)
  - mousemask() initialization for button press/release + position tracking
  - KEY_MOUSE drain loop with persistent held state
  - Backward compat CELS_Input typedef and cels_input_get() shim
  - tui_input_get_state() and tui_input_get_mouse_position() getters
affects: [06-02 consumer migration, future mouse hit-testing, cels-clay mouse integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Module-local input state via extern global (same pattern as g_frame_state)"
    - "Mouse drain loop: read all KEY_MOUSE events per frame, final position + last button event win"
    - "Persistent held state: set on press, cleared on release, survives frame reset"

key-files:
  created: []
  modified:
    - include/cels-ncurses/tui_input.h
    - src/input/tui_input.c

key-decisions:
  - "Module-local TUI_InputState replaces removed CELS_Input -- no dependency on cels/backend.h for input types"
  - "Mouse position persists across frames; button events are per-frame (reset each frame)"
  - "mouseinterval(0) disables click detection -- raw press/release only"
  - "No printf escape sequences -- mousemask() only for initial implementation"
  - "Backward compat typedef CELS_Input = TUI_InputState for gradual consumer migration"

patterns-established:
  - "Module-local extern global for state: TUI_InputState g_input_state follows TUI_FrameState g_frame_state pattern"
  - "Per-frame vs persistent fields: per-frame fields reset at start of tui_read_input_ncurses(), persistent fields survive"

# Metrics
duration: 4min
completed: 2026-02-21
---

# Phase 6 Plan 1: Mouse Input Foundation Summary

**Module-local TUI_InputState with ncurses mouse support via mousemask() drain loop, replacing removed CELS_Input dependency**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-21T07:28:26Z
- **Completed:** 2026-02-21T07:32:13Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Defined TUI_InputState struct with all v1.0 keyboard fields plus mouse position, button events, and persistent held state
- Implemented mouse-enabled input system with KEY_MOUSE drain loop (handles multiple events per frame)
- Removed dependency on cels/backend.h (CELS_Input, cels_input_set, cels_input_get all gone from .c)
- Added backward compatibility typedef and inline shim for gradual consumer migration

## Task Commits

Each task was committed atomically:

1. **Task 1: Define TUI_InputState struct and mouse types in tui_input.h** - `d49c223` (feat)
2. **Task 2: Implement mouse-enabled input system in tui_input.c** - `a09469d` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_input.h` - TUI_InputState struct, TUI_MouseButton enum, extern g_input_state, getter declarations, backward compat shims
- `src/input/tui_input.c` - Mouse-enabled input system: g_input_state definition, mousemask() init, KEY_MOUSE drain loop, getter implementations

## Decisions Made
- Module-local TUI_InputState replaces removed CELS_Input -- breaks dependency on cels core for input types
- Mouse position (-1,-1 initial) and held state persist across frames; button events reset each frame
- mouseinterval(0) disables click detection for raw press/release events
- No printf escape sequences for mouse tracking -- mousemask() only (escape sequences are a fallback if testing reveals issues)
- Backward compat CELS_Input typedef + cels_input_get() inline shim for consumer migration

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

Pre-existing compile error in tui_window.h (`CELS_WindowState` type removed from cels core in Phase 26 refactoring) prevents full project build. This is outside scope -- tui_input.h and tui_input.c compile correctly in isolation. Consumer migration (Plan 02) will address remaining build issues.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Input system foundation complete with mouse primitives
- Plan 02 needed to update consumers (space_invaders, examples) to use new TUI_InputState API
- Pre-existing tui_window.h CELS_WindowState issue may need resolution for full build

---
*Phase: 06-mouse-input*
*Completed: 2026-02-21*
