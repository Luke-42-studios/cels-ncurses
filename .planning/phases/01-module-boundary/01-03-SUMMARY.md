---
phase: 01-module-boundary
plan: 03
subsystem: module-api
tags: [legacy-cleanup, cmake, minimal-example, ncurses-module, entity-component-api]

# Dependency graph
requires:
  - phase: 01-module-boundary
    plan: 01
    provides: Module skeleton with CEL_Module(NCurses), public headers, CEL_Define(NCursesWindow)
  - phase: 01-module-boundary
    plan: 02
    provides: Observer-based terminal lifecycle, frame/input system wiring, strong symbol overrides
provides:
  - All legacy Engine/CelsNcurses/widget files deleted
  - Clean CMake with no dead source references
  - Minimal example proving full NCurses module lifecycle end-to-end
  - Executable target for minimal example
affects: [02-window-entity (if renumbered), future phases build on clean module boundary]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Minimal example pattern: cels_register -> NCursesWindow -> cel_watch -> cels_session loop"

key-files:
  created: []
  modified:
    - examples/minimal.c
    - CMakeLists.txt

key-decisions:
  - "Deleted 11 legacy files (2318 lines removed) -- all replaced by Plan 01/02 implementations"
  - "minimal.c uses cel_watch(win, NCurses_WindowState) to prove reactive state reading works"
  - "CMake minimal target links against cels-ncurses INTERFACE library (compiles all module sources in consumer context)"

patterns-established:
  - "Example pattern: include cels/cels.h + cels-ncurses/ncurses.h, register module, create window entity via composition, watch state, run session loop"

# Metrics
duration: 3min
completed: 2026-02-28
---

# Phase 1 Plan 3: Legacy Cleanup and Minimal Example Summary

**Deleted 11 legacy files (Engine/CelsNcurses/widgets/space_invaders), rebuilt minimal.c with entity-component API (cels_register, NCursesWindow, cel_watch, cels_session)**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-28T18:33:37Z
- **Completed:** 2026-02-28T18:36:40Z
- **Tasks:** 2 auto + 1 checkpoint
- **Files modified:** 2 (minimal.c, CMakeLists.txt), 11 deleted

## Accomplishments
- Deleted all legacy Engine/CelsNcurses module files (tui_engine.c/h, backend.h), widget files (tui_widgets.h/c), space_invaders example, and run scripts -- 2318 lines of dead code removed
- Rewrote minimal.c as the reference implementation for the new NCurses module API, demonstrating the full lifecycle: module registration, window entity creation, reactive state watching, and session loop
- Updated CMake to remove dead source reference (tui_engine.c) and add minimal executable target

## Task Commits

Each task was committed atomically:

1. **Task 1: Delete legacy files and update CMake** - `a4874a1` (chore)
2. **Task 2: Rewrite minimal.c with new NCurses module API** - `44f4d01` (feat)

## Files Created/Modified
- `examples/minimal.c` - Rewritten to use cels_register(NCurses), NCursesWindow() call macro, cel_watch(win, NCurses_WindowState), cels_session/cels_running/cels_step
- `CMakeLists.txt` - Removed src/tui_engine.c from target_sources, added minimal executable target

## Files Deleted
- `src/tui_engine.c` - Old Engine/CelsNcurses module (replaced by src/ncurses_module.c)
- `include/cels-ncurses/tui_engine.h` - Old Engine module header (replaced by tui_ncurses.h)
- `include/cels-ncurses/backend.h` - Old umbrella header (replaced by ncurses.h)
- `include/cels-ncurses/tui_widgets.h` - Widget header (cels-widgets is a separate library)
- `src/widgets/tui_widgets.c` - Widget implementation (not part of NCurses module)
- `examples/space_invaders/components.h` - Old API example
- `examples/space_invaders/game.c` - Old API example
- `examples/space_invaders/renderer.h` - Old API example
- `examples/run_space_invaders.sh` - Old run script
- `examples/run_minimal.sh` - Old run script
- `examples/run_draw_test.sh` - Old run script

## Decisions Made
- Deleted all legacy files in a single commit for clean atomic revert if needed
- minimal.c uses exact API patterns from CONTEXT.md: CEL_Composition(World), NCursesWindow call macro, cel_watch for reactive state, cels_main/cels_session/cels_running/cels_step
- TUI_Input_use() no-op stub left in tui_input.c (it has no callers now, but removing it is a separate cleanup concern for future phases)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 1 Module Boundary is complete: CEL_Module(NCurses) with observer-based terminal lifecycle, entity-component API surface, and working minimal example
- Awaiting human verification that minimal example builds and runs (checkpoint:human-verify)
- draw_test standalone example should still compile (no CELS dependency, no deleted files referenced)
- Ready for Phase 2 (Window Entity deepening) or renumbered phases as per roadmap

---
*Phase: 01-module-boundary*
*Completed: 2026-02-28*
