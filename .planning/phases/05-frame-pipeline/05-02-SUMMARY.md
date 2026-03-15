---
phase: 05-frame-pipeline
plan: 02
subsystem: rendering
tags: [ncurses, ecs, cmake, legacy-deletion, frame-pipeline]

# Dependency graph
requires:
  - phase: 05-frame-pipeline/01
    provides: TUI_FrameBeginSystem, TUI_FrameEndSystem inline in ncurses_module.c
provides:
  - Zero legacy frame/layer code remaining in codebase
  - Clean public draw API (cels_ncurses_draw.h) with only entity-driven types
  - Build-verified minimal target with no dead file references
affects: [06-demo]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Split cels_register calls to stay within 8-arg macro limit"

key-files:
  created: []
  modified:
    - include/cels_ncurses_draw.h
    - src/tui_internal.h
    - src/ncurses_module.c
    - CMakeLists.txt
  deleted:
    - src/frame/tui_frame.c
    - src/layer/tui_layer.c
    - examples/draw_test.c

key-decisions:
  - "cels_register split into two calls (8+3) to fit _CELS_REG_ macro 8-arg limit"
  - "Subcell buffer comments mentioning tui_frame_begin/tui_layer_destroy kept (documentation, not API)"

patterns-established:
  - "cels_register max 8 args per call -- split into multiple calls when exceeding"

# Metrics
duration: 4min
completed: 2026-03-14
---

# Phase 5 Plan 2: Legacy Deletion Summary

**Deleted 3 legacy files (~1660 lines), stripped layer/frame API from public header, fixed cels_register 11-arg overflow, build verified clean**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-15T05:54:15Z
- **Completed:** 2026-03-15T05:58:02Z
- **Tasks:** 2
- **Files modified:** 4 modified, 3 deleted

## Accomplishments
- Deleted tui_frame.c (241 lines), tui_layer.c (223 lines), draw_test.c (~1200 lines) and src/frame/ directory
- Stripped Layer System section (TUI_Layer typedef, g_layers/g_layer_count externs, all tui_layer_* declarations) and Frame Pipeline section (TUI_FrameState, g_frame_state, tui_frame_* declarations) from cels_ncurses_draw.h
- Fixed cels_register overflow: 11 args exceeded 8-arg macro limit, split into two calls
- Build verified: minimal target compiles and links cleanly with zero errors

## Task Commits

Each task was committed atomically:

1. **Task 1: Delete legacy files and clean headers** - `b747ef0` (refactor)
2. **Task 2: Update CMakeLists.txt and verify build** - `5ed51d0` (fix)

## Files Created/Modified
- `include/cels_ncurses_draw.h` - Removed Layer System and Frame Pipeline sections (~70 lines), updated header doc comment
- `src/tui_internal.h` - Removed ncurses_register_frame_systems() declaration
- `src/ncurses_module.c` - Split cels_register(11 args) into two calls (8+3) to fix macro overflow
- `CMakeLists.txt` - Removed tui_layer.c and tui_frame.c from target_sources, deleted draw_test target block

## Files Deleted
- `src/frame/tui_frame.c` - Legacy imperative frame pipeline (absorbed into ncurses_module.c systems)
- `src/layer/tui_layer.c` - Legacy imperative layer API (replaced by entity-driven TUI_LayerLC)
- `examples/draw_test.c` - Standalone non-ECS visual test (no longer relevant)
- `src/frame/` - Empty directory after tui_frame.c deletion

## Decisions Made
- Split cels_register into two calls (8+3 args) instead of one (11 args) because the _CELS_REG_ macros only support up to 8 arguments
- Kept subcell buffer comments referencing tui_frame_begin/tui_layer_destroy as documentation -- they describe lifecycle context, not API surface

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed cels_register macro overflow with 11 arguments**
- **Found during:** Task 2 (build verification)
- **Issue:** cels_register(11 args) exceeded _CEL_SYSTEM_ARG_COUNT 8-arg limit, causing _CELS_REG_ dispatch to concatenate incorrectly and produce "implicit declaration of function _CELS_REG_TUI_LayerSystem" errors
- **Fix:** Split single cels_register(11 args) into cels_register(8 args) + cels_register(3 args)
- **Files modified:** src/ncurses_module.c
- **Verification:** make minimal completes with zero errors
- **Committed in:** `5ed51d0` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Pre-existing bug from Phase 05-01 where 11 items were merged into a single cels_register call. Fix is minimal (one line split into two). No scope creep.

## Issues Encountered
None beyond the auto-fixed bug above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Frame pipeline is fully entity-driven with zero legacy remnants
- All headers expose only the entity-driven API surface
- Build verified: minimal target compiles and links cleanly
- Ready for Phase 6 (Demo) which will exercise the complete entity-driven pipeline

---
*Phase: 05-frame-pipeline*
*Completed: 2026-03-14*
