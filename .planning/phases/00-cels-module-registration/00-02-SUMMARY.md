---
phase: 00-cels-module-registration
plan: 02
subsystem: api
tags: [c-macros, ecs-pipeline, cels, module-registration]

# Dependency graph
requires:
  - phase: none
    provides: existing cels codebase with CEL_Module macro and CELS_REGISTER phase
provides:
  - CEL_Module(Name) generates Name_register() alias for uniform cels_register() usage
  - CELS_REGISTER pipeline phase fully removed (7-phase pipeline)
  - cels_register(NCurses) expansion chain works end-to-end
affects: [01-module-boundary, cels-ncurses Phase 1]

# Tech tracking
tech-stack:
  added: []
  patterns: [static-inline-register-alias]

key-files:
  created: []
  modified:
    - include/cels/cels.h
    - include/cels/private/cels_system_impl.h
    - include/cels/private/cels_runtime.h
    - src/managers/phase_registry.c
    - src/managers/phase_registry.h
    - src/managers/system_registry.c
    - src/managers/system_registry.h
    - src/cels_api.c
    - src/backends/main.c

key-decisions:
  - "Name_register() is static inline alias calling Name_init() -- matches CEL_Component pattern"
  - "CELS_ERROR_LIMIT_EXCEEDED kept as general-purpose error code despite Register-specific usage being removed"
  - "7-phase pipeline: OnLoad, LifecycleEval, OnUpdate, StateSync, Recompose, PreRender, OnRender, PostRender"

patterns-established:
  - "static inline void Name##_register(void) { Name##_init(); } -- module registration alias pattern"

# Metrics
duration: 4min
completed: 2026-02-27
---

# Phase 0 Plan 2: CEL_Module Macro Fix and CELS_REGISTER Removal Summary

**CEL_Module generates Name_register() alias for uniform cels_register(Module), CELS_REGISTER pipeline phase fully removed across 9 files**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-27T23:39:09Z
- **Completed:** 2026-02-27T23:43:15Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- CEL_Module(Name) now generates static inline Name_register() alias, making cels_register(ModuleName) work uniformly for modules, components, systems, and phases
- CELS_REGISTER enum value, Register phase shorthand macro, CELS_Phase_Register legacy shim, CelsRegister phase entity creation, register callback storage, register-phase detection block, and session_enter register execution all removed
- All docstrings and comments updated from 8-phase to 7-phase pipeline references
- All 6 existing tests pass with zero failures, build compiles clean

## Task Commits

Each task was committed atomically:

1. **Task 1: Fix CEL_Module macro to generate Name_register()** - `ea3b5c0` (fix)
2. **Task 2: Remove CELS_REGISTER phase entirely** - `e6039d6` (refactor)

## Files Created/Modified
- `include/cels/cels.h` - Added Name_register alias to CEL_Module macro, removed CELS_REGISTER enum value, updated phase/error docstrings
- `include/cels/private/cels_system_impl.h` - Removed `#define Register` phase shorthand
- `include/cels/private/cels_runtime.h` - Removed CELS_Phase_Register legacy shim, updated session_enter docstring
- `src/managers/phase_registry.c` - Removed CelsRegister phase entity creation, updated 8->7 phase comments
- `src/managers/phase_registry.h` - Updated 8->7 phase comments
- `src/managers/system_registry.c` - Removed CELS_MAX_REGISTER_CALLBACKS, callback storage fields, add_register/run_register implementations
- `src/managers/system_registry.h` - Removed add_register/run_register declarations, updated file docstring
- `src/cels_api.c` - Removed register-phase call from _cels_session_enter, removed Register detection block from _cels_system_create_v2, updated docstring
- `src/backends/main.c` - Updated execution flow docstring to remove Register callbacks reference

## Decisions Made
- Name_register() implemented as `static inline` to match CEL_Component pattern (line 448) and avoid multiple definition errors in multi-TU inclusion
- CELS_ERROR_LIMIT_EXCEEDED retained as general-purpose error code (used by error_code_to_string mapping and test_error_handling.c test)
- _cels_session_enter() left as non-empty stub (NULL check only) since function signature is part of internal API

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed stale Register callbacks reference in backends/main.c docstring**
- **Found during:** Task 2 (CELS_REGISTER removal verification)
- **Issue:** src/backends/main.c line 29 still referenced "Run Register callbacks + main loop" in its execution flow docstring
- **Fix:** Updated to "Run main loop"
- **Files modified:** src/backends/main.c
- **Verification:** grep confirms no remaining "Register callback" or "Register phase" references in source
- **Committed in:** e6039d6 (part of Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug -- stale comment)
**Impact on plan:** Necessary for complete removal. No scope creep.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- cels_register(NCurses) expansion chain now works: cels_register(NCurses) -> NCurses_register() -> NCurses_init() -> module registration
- cels-ncurses Phase 1 (Module Boundary) is unblocked and can proceed with cels_register(NCurses)
- No blockers or concerns

---
*Phase: 00-cels-module-registration*
*Completed: 2026-02-27*
