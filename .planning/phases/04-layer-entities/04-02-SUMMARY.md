---
phase: 04-layer-entities
plan: 02
subsystem: ui
tags: [ncurses, panels, layer, ecs, module-registration, frame-pipeline, cmake, composition]

# Dependency graph
requires:
  - phase: 04-layer-entities/plan-01
    provides: TUI_LayerConfig, TUI_DrawContext_Component, TUI_LayerLC lifecycle, TUI_LayerSyncSystem, layer entity registry
  - phase: 03-input-system
    provides: CEL_System pattern with cel_read(NCurses_InputState) for input handling
provides:
  - Layer entity system fully wired into CEL_Module(NCurses) init
  - Entity-layer dirty clearing integrated into frame pipeline tui_frame_begin
  - tui_layer_entity.c added to CMake INTERFACE sources
  - Working minimal.c demo proving all three LAYR requirements
  - cels v0.5.3 macro migration (CEL_Define -> CEL_Define_Composition, CEL_Compose <-> CEL_Composition swap)
affects: [05-frame-pipeline, 06-demo]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "ncurses_layer_entity_clear_dirty() encapsulates entity layer clearing behind function call (not extern data)"
    - "CELS_HAS_ECS guard for entity layer clearing in frame pipeline alongside old g_layers[] clearing"

key-files:
  created: []
  modified:
    - src/ncurses_module.c
    - src/frame/tui_frame.c
    - src/layer/tui_layer_entity.c
    - src/tui_internal.h
    - CMakeLists.txt
    - examples/minimal.c
    - include/cels_ncurses.h

key-decisions:
  - "Approach (B) for frame integration: encapsulate entity layer clearing in ncurses_layer_entity_clear_dirty() function rather than exposing g_layer_entries extern"
  - "cels v0.5.3 macro migration: CEL_Define -> CEL_Define_Composition, CEL_Compose -> CEL_Composition (cross-TU), CEL_Composition -> CEL_Compose (single-file)"
  - "cels dependency targets v0.5.3 branch (has CEL_Define_State, renamed composition macros)"

patterns-established:
  - "Encapsulated clearing: entity layer state accessed only through function calls from frame pipeline, not extern globals"

# Metrics
duration: 10min
completed: 2026-03-14
---

# Phase 4 Plan 02: Module Registration, Frame Integration, and Layer Demo Summary

**Layer entity system wired into NCurses module init and frame pipeline, with minimal.c proving TUILayer composition + drawing at two z-orders, plus cels v0.5.3 macro migration**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-14T17:25:15Z
- **Completed:** 2026-03-14T17:35:03Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- TUI_LayerLC and TUI_LayerSyncSystem registered in CEL_Module(NCurses, init) via cels_register and ncurses_register_layer_systems()
- Entity-managed layers cleared at frame_begin alongside old g_layers[] loop, behind #ifdef CELS_HAS_ECS guard
- minimal.c demonstrates full LAYR developer workflow: two TUILayer entities at z=0 (fullscreen background) and z=10 (positioned overlay), with fill_rect, border_rect, and text drawing
- Both minimal and draw_test targets build cleanly with zero warnings
- Migrated all cels-ncurses source to cels v0.5.3 macro API (composition macro name swap)

## Task Commits

Each task was committed atomically:

1. **Task 1: Register layer systems in module, integrate entity layers with frame pipeline, update CMake** - `5a772f6` (feat)
2. **Task 2: Update minimal.c to demonstrate layer entities with drawing, verify build** - `aa699e2` (feat)

## Files Created/Modified
- `src/ncurses_module.c` - Added TUI_LayerLC, TUI_LayerSyncSystem to cels_register; added ncurses_register_layer_systems() call; migrated CEL_Compose -> CEL_Composition
- `src/frame/tui_frame.c` - Added ncurses_layer_entity_clear_dirty() call in tui_frame_begin behind CELS_HAS_ECS guard; added tui_internal.h include
- `src/layer/tui_layer_entity.c` - Added ncurses_layer_entity_clear_dirty() function for entity layer clearing; migrated CEL_Compose -> CEL_Composition
- `src/tui_internal.h` - Added extern declaration for ncurses_layer_entity_clear_dirty(); migrated CEL_Define -> CEL_Define_System
- `CMakeLists.txt` - Added tui_layer_entity.c to INTERFACE target sources
- `examples/minimal.c` - Replaced input demo with layer entity demo: two TUILayer entities with drawing; migrated CEL_Composition -> CEL_Compose
- `include/cels_ncurses.h` - Migrated CEL_Define -> CEL_Define_Composition for NCursesWindow and TUILayer

## Decisions Made
- Used approach (B) for frame integration: encapsulated entity layer dirty clearing in ncurses_layer_entity_clear_dirty() function rather than exposing g_layer_entries via extern. This keeps the entity layer registry private to tui_layer_entity.c.
- Migrated to cels v0.5.3 macro API instead of using CELS_COMPAT flag. The v0.5.3 API swapped CEL_Compose/CEL_Composition meanings and moved CEL_Define behind CELS_COMPAT. Direct migration is cleaner than depending on backward-compatibility shims.
- Confirmed cels dependency targets v0.5.3 branch (not v0.5.2 as previously stated in STATE.md). CEL_Define_State only exists in v0.5.3.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] cels v0.5.3 macro API migration**
- **Found during:** Task 2 (build verification)
- **Issue:** Build failed because cels v0.5.3 renamed composition macros: CEL_Define moved behind CELS_COMPAT, CEL_Compose and CEL_Composition meanings swapped. The cels-ncurses codebase used CEL_Define_State (v0.5.3 feature) but old CEL_Define/CEL_Compose names.
- **Fix:** Migrated all macro usage: CEL_Define -> CEL_Define_Composition (headers), CEL_Define -> CEL_Define_System (system forward declarations), CEL_Compose -> CEL_Composition (cross-TU source), CEL_Composition -> CEL_Compose (single-file). 7 files updated.
- **Files modified:** include/cels_ncurses.h, src/ncurses_module.c, src/layer/tui_layer_entity.c, src/tui_internal.h, examples/minimal.c
- **Verification:** Both minimal and draw_test build with zero errors and zero warnings
- **Committed in:** aa699e2 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Migration was necessary for build to succeed. No scope creep -- just macro renames to match the actual cels API version.

## Issues Encountered
- Initially attempted build against cels v0.5.2, which failed because cels-ncurses uses CEL_Define_State (a v0.5.3-only feature). Switched to v0.5.3, discovered additional macro renames needed. Resolved by migrating to v0.5.3 API throughout.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 4 (Layer Entities) is complete: component types, lifecycle, z-order sync, module registration, frame integration, and working demo
- Phase 5 (Frame Pipeline) is unblocked: entity layers are cleared at frame_begin, composed at frame_end via update_panels/doupdate
- The old tui_layer.c API is untouched -- draw_test backward compatibility preserved
- cels dependency must remain on v0.5.3 branch for builds

---
*Phase: 04-layer-entities*
*Completed: 2026-03-14*
