---
phase: 01-module-boundary
plan: 02
subsystem: module-wiring
tags: [flecs-observer, ecs-systems, ncurses-lifecycle, window-entity, frame-pipeline, input-system]

# Dependency graph
requires:
  - phase: 01-module-boundary
    plan: 01
    provides: Module skeleton with CEL_Module(NCurses), weak stubs, component types
provides:
  - Observer-based terminal lifecycle (ncurses_register_window_observer, ncurses_register_window_remove_observer)
  - NCurses_WindowState attached to window entity on terminal init
  - Module-pattern frame systems (ncurses_register_frame_systems)
  - Module-pattern input system (ncurses_register_input_system)
  - Per-frame resize detection and quit propagation (ncurses_window_frame_update)
affects: [01-03-PLAN (legacy cleanup/example rebuild)]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Flecs EcsOnSet/EcsOnRemove observer for reactive component lifecycle"
    - "Strong symbol override of weak stubs for incremental module assembly"
    - "ecs_set_id for attaching WindowState component to entity from observer callback"

key-files:
  created: []
  modified:
    - src/window/tui_window.c
    - include/cels-ncurses/tui_window.h
    - src/frame/tui_frame.c
    - src/input/tui_input.c

key-decisions:
  - "ncurses_terminal_init() extracted from tui_hook_startup() -- takes NCurses_WindowConfig* instead of reading global"
  - "g_window_entity tracks single window entity for resize updates and quit propagation"
  - "tui_hook_frame_end() kept as named function for FPS throttle -- called by frame_end_callback"
  - "tui_window_get_running_ptr() kept as bridge for input system quit signaling during transition"
  - "define_key/mousemask called at module init time (before initscr) -- ncurses tolerates this"

patterns-established:
  - "Observer-based terminal lifecycle: EcsOnSet triggers init, EcsOnRemove triggers shutdown"
  - "Module registration functions override weak stubs via strong symbol linkage"
  - "Frame pipeline owns window frame update call (ncurses_window_frame_update at frame begin)"

# Metrics
duration: 4min
completed: 2026-02-28
---

# Phase 1 Plan 2: Observer-Based Terminal Lifecycle Summary

**Flecs observer-based ncurses terminal init/shutdown with NCurses_WindowState entity attachment, module-pattern frame and input system registration**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-28T18:26:36Z
- **Completed:** 2026-02-28T18:30:52Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Replaced the entire Engine_WindowState / CELS_BackendDesc / cels_backend_register system with flecs observer-based lifecycle
- NCurses_WindowConfig EcsOnSet observer initializes terminal and attaches NCurses_WindowState with dimensions, running state, and fps
- NCurses_WindowConfig EcsOnRemove observer shuts down terminal cleanly
- Single window entity enforcement: second NCurses_WindowConfig logs warning and is ignored
- Frame pipeline wired to call ncurses_window_frame_update() each frame for resize detection and quit propagation
- Input system calls cels_request_quit() on 'q' key for proper session loop integration
- All 4 weak stubs from Plan 01 now have strong overrides in the respective .c files

## Task Commits

Each task was committed atomically:

1. **Task 1: Refactor tui_window.c to observer-based lifecycle** - `11cbbd5` (feat)
2. **Task 2: Wire frame and input systems into module pattern** - `e9d78d5` (feat)

## Files Created/Modified
- `src/window/tui_window.c` - Major refactor: removed Engine_WindowState, CELS_BackendDesc, backend hooks; added observer registration, ncurses_terminal_init(), ncurses_window_frame_update(), tui_hook_frame_end()
- `include/cels-ncurses/tui_window.h` - Stripped to internal header: WindowState enum, running_ptr bridge, frame update decl, frame_end decl; removed Engine_WindowState_t, TUI_Window config, backend.h include, all backward compat typedefs
- `src/frame/tui_frame.c` - Added ncurses_register_frame_systems() (inside CELS_HAS_ECS guard), frame_begin_callback calls ncurses_window_frame_update(), frame_end_callback calls tui_hook_frame_end()
- `src/input/tui_input.c` - Added ncurses_register_input_system() extracting registration from TUI_Input_use() (now no-op stub), 'q' handler calls cels_request_quit(), removed TUI_WindowState reference from resize handler

## Decisions Made
- Extracted ncurses_terminal_init() takes NCurses_WindowConfig* parameter instead of reading from global g_tui_config -- cleaner data flow from observer callback
- g_window_entity tracks the single window entity at file scope -- used by both observer callback and frame update function
- Frame end callback calls tui_hook_frame_end() for FPS throttle (function kept public for this purpose)
- tui_window_get_running_ptr() bridge kept -- input system still uses it alongside cels_request_quit() during transition
- TUI_Input_use() kept as no-op stub (Plan 03 deletes tui_engine.c which calls it)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Removed stale TUI_WindowState reference in resize handler**
- **Found during:** Task 2
- **Issue:** tui_input.c line 259 referenced `TUI_WindowState.width` and `TUI_WindowState.height` for debug fprintf, but TUI_WindowState macro pointed to Engine_WindowState which no longer exists
- **Fix:** Simplified fprintf to only show COLS/LINES without old dimensions
- **Files modified:** src/input/tui_input.c
- **Commit:** e9d78d5

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Module is functionally wired: CEL_Module(NCurses) registers real observer/system implementations (no more stubs)
- tui_engine.c still present with old Engine/CelsNcurses code -- Plan 03 handles legacy deletion
- draw_test target unchanged and safe (new ECS code is inside CELS_HAS_ECS guard)
- TUI_Input_use() is a no-op stub -- Plan 03 can safely delete it alongside tui_engine.c

---
*Phase: 01-module-boundary*
*Completed: 2026-02-28*
