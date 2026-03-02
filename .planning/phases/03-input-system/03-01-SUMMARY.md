---
phase: 03-input-system
plan: 01
subsystem: input
tags: [ncurses, getch, mouse, ecs-component, entity-component]

# Dependency graph
requires:
  - phase: 01.2-window-lifecycle-rewrite
    provides: cels_entity_set_component/get_component API, CEL_Compose pattern, lifecycle-driven window
provides:
  - NCurses_InputState CEL_Component with keyboard and mouse fields
  - Input system writing per-frame state to window entity
  - Quit via cels_request_quit() only (no g_running pointer bridge)
affects: [03-02-input-developer-api, 04-layer-entities, 06-demo]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Input state as CEL_Component on window entity (matches WindowState pattern)"
    - "Per-frame state reset with persistent field carry-forward from previous component"
    - "Mouse drain loop falls through to entity write instead of early return"

key-files:
  created: []
  modified:
    - include/cels-ncurses/tui_ncurses.h
    - src/ncurses_module.c
    - src/input/tui_input.c
    - include/cels-ncurses/tui_input.h

key-decisions:
  - "NCurses_InputState component on window entity replaces global g_input_state"
  - "Mouse button values as plain integers (0/1/2/3) instead of TUI_MouseButton enum"
  - "Quit guard kept as internal function pointer (not componentized)"
  - "Mouse x/y initialized to -1 in composition (no position yet)"
  - "ERR from getch still writes zeroed-but-persistent state to entity for consistency"

patterns-established:
  - "Input component pattern: build fresh state on stack, copy persistent fields from prev, drain getch, write to entity"
  - "Mouse drain loop: falls through to entity write (no early return after mouse processing)"

# Metrics
duration: 3min
completed: 2026-03-02
---

# Phase 3 Plan 01: Input System Component Summary

**NCurses_InputState CEL_Component replacing global g_input_state, written to window entity each frame via cels_entity_set_component with cels_request_quit()-only quit signaling**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-02T02:26:59Z
- **Completed:** 2026-03-02T02:30:19Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- NCurses_InputState CEL_Component defined in tui_ncurses.h with full keyboard and mouse field set
- Input system writes per-frame state to window entity via cels_entity_set_component + cels_component_notify_change
- Removed all global state: g_input_state, g_tui_running_ptr, g_mouse_initialized
- Stripped tui_input.h from 159-line public header to 17-line internal-only header (quit guard API only)
- Preserved all getch reading logic: switch/case, mouse drain loop, KEY_RESIZE, custom keys, pause mode

## Task Commits

Each task was committed atomically:

1. **Task 1: Add NCurses_InputState component to public header and module** - `59a5727` (feat)
2. **Task 2: Rewrite tui_input.c to entity-component pattern, strip tui_input.h** - `edae2db` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_ncurses.h` - Added NCurses_InputState CEL_Component definition + extern ID override
- `src/ncurses_module.c` - Added _ncurses_InputState_id global + cel_has in CEL_Compose(NCursesWindow)
- `src/input/tui_input.c` - Rewritten: entity-component writes, no globals, cels_request_quit() only
- `include/cels-ncurses/tui_input.h` - Stripped to internal-only header with quit guard API

## Decisions Made
- NCurses_InputState component lives on the window entity (matching NCurses_WindowState pattern)
- Mouse button values use plain integers (0=none, 1=left, 2=middle, 3=right) instead of TUI_MouseButton enum (enum removed with header)
- Quit guard (g_quit_guard_fn) kept as internal function pointer -- not componentized (internal API for cels-widgets)
- Mouse x/y initialized to -1 in composition cel_has (meaning "no position yet")
- On ERR from getch, still write zeroed-but-persistent state to entity so component is always fresh
- Mouse drain loop changed from early return to fall-through to entity write (consistent state writes)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- NCurses_InputState component and input system ready for Plan 02 (developer API: minimal.c update with cel_watch, tui_window.h cleanup)
- tui_window_get_running_ptr() still exists in tui_window.c/tui_window.h but is no longer called by tui_input.c -- Plan 02 or later should clean it up
- Runtime build/run confirmation recommended

---
*Phase: 03-input-system*
*Completed: 2026-03-02*
