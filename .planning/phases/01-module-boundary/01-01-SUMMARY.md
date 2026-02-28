---
phase: 01-module-boundary
plan: 01
subsystem: module-api
tags: [cels, ncurses, cel-module, cel-define, cel-compose, ecs, components, interface-library]

# Dependency graph
requires:
  - phase: 00-cels-module-registration
    provides: CEL_Module macro with Name_register() generation
provides:
  - NCurses module skeleton with CEL_Module(NCurses)
  - Public component types (NCurses_WindowConfig, NCurses_WindowState)
  - CEL_Define(NCursesWindow) composition with call macro
  - Umbrella header (ncurses.h) replacing backend.h
  - Internal header for observer/system registration function decls
  - CMake updated with ncurses_module.c, cels-widgets dependency removed
affects: [01-02-PLAN (observer/system wiring), 01-03-PLAN (legacy cleanup/example rebuild)]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "CEL_Module + extern decls in header + per-TU static inline register pattern"
    - "Weak symbol stubs for incremental module assembly"
    - "CEL_Define/CEL_Compose for cross-TU composition export"

key-files:
  created:
    - include/cels-ncurses/ncurses.h
    - include/cels-ncurses/tui_ncurses.h
    - include/cels-ncurses/tui_internal.h
    - src/ncurses_module.c
  modified:
    - CMakeLists.txt

key-decisions:
  - "Per-TU NCurses_register() static inline delegates to extern NCurses_init() -- mirrors CEL_Component pattern for INTERFACE library"
  - "Weak symbol stubs allow module to compile before Plan 02 wires real observers/systems"
  - "cels-widgets dependency removed from INTERFACE library -- consumers link it themselves if needed"

patterns-established:
  - "Module public header pattern: extern entity + extern init + static inline register"
  - "Internal header pattern: forward decls for registration functions, included only by module .c"
  - "Umbrella header pattern: ncurses.h includes tui_ncurses.h + all drawing/layer/frame headers"

# Metrics
duration: 3min
completed: 2026-02-28
---

# Phase 1 Plan 1: Module Boundary API Surface Summary

**CEL_Module(NCurses) skeleton with NCurses_WindowConfig/WindowState components, CEL_Define(NCursesWindow) composition, and weak stubs for incremental wiring**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-28T18:21:08Z
- **Completed:** 2026-02-28T18:24:13Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Created the new public API surface: umbrella header (ncurses.h), component types header (tui_ncurses.h), and internal function declarations header (tui_internal.h)
- Implemented CEL_Module(NCurses) with component registration and CEL_Compose(NCursesWindow) forwarding props to cel_has(NCurses_WindowConfig)
- Updated CMake to include ncurses_module.c and drop cels-widgets dependency
- Weak stubs allow compilation before Plan 02 wires real observer/system implementations

## Task Commits

Each task was committed atomically:

1. **Task 1: Create public headers (ncurses.h, tui_ncurses.h, tui_internal.h)** - `921085d` (feat)
2. **Task 2: Create module skeleton and update CMake** - `8c6c507` (feat)

## Files Created/Modified
- `include/cels-ncurses/ncurses.h` - Umbrella header replacing backend.h, includes all public NCurses headers
- `include/cels-ncurses/tui_ncurses.h` - Public component types (NCurses_WindowConfig, NCurses_WindowState), module extern decls, per-TU NCurses_register(), CEL_Define(NCursesWindow), call macro
- `include/cels-ncurses/tui_internal.h` - Internal forward declarations for 4 registration functions (window observer, window remove, input system, frame systems)
- `src/ncurses_module.c` - CEL_Module(NCurses) definition with component registration, CEL_Compose(NCursesWindow) body, and 4 weak stub functions
- `CMakeLists.txt` - Added ncurses_module.c to target_sources, removed cels-widgets from target_link_libraries

## Decisions Made
- Per-TU NCurses_register() as static inline delegating to extern NCurses_init() -- required because CEL_Module generates a static inline register in the module .c file (not visible to other TUs in INTERFACE library pattern)
- Used __attribute__((weak)) for all 4 registration stubs so the linker picks real implementations when available (Plan 02) without multiple definition errors
- Removed cels-widgets from link dependencies -- NCurses module has no widget dependency; consumers link it themselves if needed

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Module skeleton compiles with stubs -- Plan 02 can wire real observer/system implementations by providing strong definitions that override the weak stubs
- Component types and composition macro are ready for use in observer callbacks
- tui_engine.c still present (Plan 03 handles legacy deletion)
- draw_test target unchanged (Plan 03 rebuilds it)

---
*Phase: 01-module-boundary*
*Completed: 2026-02-28*
