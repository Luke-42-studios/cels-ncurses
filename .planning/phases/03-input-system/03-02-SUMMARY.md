---
phase: 03-input-system
plan: 02
subsystem: input
tags: [ncurses, input, cleanup, cel_watch, minimal-example]

# Dependency graph
requires:
  - phase: 03-input-system
    plan: 01
    provides: NCurses_InputState CEL_Component, entity-component input system, cels_request_quit() quit signaling
provides:
  - Clean window module with no running pointer bridge
  - Minimal example demonstrating cel_watch for both WindowState and InputState
  - Zero stale references to removed APIs across codebase
  - Build-verified project (both minimal and draw_test targets)
affects: [04-layer-entities, 05-frame-pipeline, 06-demo]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Dual cel_watch pattern: window state + input state read in same composition"
    - "Input display pattern: axis, mouse position, accept/cancel, raw key in mvprintw"

key-files:
  created: []
  modified:
    - src/window/tui_window.c
    - include/cels-ncurses/tui_window.h
    - examples/minimal.c

key-decisions:
  - "tui_window_get_running_ptr() fully removed -- g_running stays internal (SIGINT handler + frame update)"
  - "minimal.c updated from resize-tracking demo to input state demo"
  - "g_prev_w/g_prev_h resize tracking globals removed from minimal.c"

patterns-established:
  - "Consumer input reading: cel_watch(entity, NCurses_InputState) alongside cel_watch(entity, NCurses_WindowState)"

# Metrics
duration: 2min
completed: 2026-03-02
---

# Phase 3 Plan 02: Input Developer API Summary

**Running pointer bridge removed, minimal example updated with dual cel_watch for WindowState + InputState, both build targets verified clean**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-02T02:33:35Z
- **Completed:** 2026-03-02T02:35:18Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Deleted tui_window_get_running_ptr() from tui_window.c and its declaration from tui_window.h
- Updated minimal.c to demonstrate cel_watch for both NCurses_WindowState and NCurses_InputState
- Example now shows window dimensions, fps, axis values, mouse position, accept/cancel, and raw key events
- Verified zero stale references to g_input_state, tui_input_get_state, tui_window_get_running_ptr, CELS_Input, TUI_MOUSE_*
- Both minimal and draw_test targets build without errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove running pointer bridge and update minimal example** - `31f75d8` (feat)
2. **Task 2: Build verification and stale reference sweep** - verification only, no file changes

## Files Created/Modified
- `src/window/tui_window.c` - Removed tui_window_get_running_ptr() function and its comment block
- `include/cels-ncurses/tui_window.h` - Removed tui_window_get_running_ptr() declaration and comment
- `examples/minimal.c` - Updated to dual cel_watch demo (WindowState + InputState) with input display

## Decisions Made
- tui_window_get_running_ptr() fully removed -- the g_running static variable remains internal to tui_window.c for SIGINT handler and frame update quit detection
- minimal.c transformed from a resize-tracking demo (Phase 1.2) to an input state demo (Phase 3)
- Removed g_prev_w/g_prev_h globals and ncurses_console_log resize logging from minimal.c (no longer relevant)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Input system is fully entity-component based with zero global state remnants
- NCurses_InputState and NCurses_WindowState both readable via cel_watch in consumer compositions
- Ready for Phase 4 (Layer Entities) -- no blockers from input system
- Runtime confirmation still recommended (build verified, runtime not tested in this plan)

---
*Phase: 03-input-system*
*Completed: 2026-03-02*
