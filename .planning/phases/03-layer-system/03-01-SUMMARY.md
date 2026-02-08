---
phase: 03-layer-system
plan: 01
subsystem: ui
tags: [ncurses, panel, layer, z-order, newwin, new_panel]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: "TUI_Types (TUI_CellRect), TUI_Color, TUI_DrawContext, extern pattern"
  - phase: 02-drawing-primitives
    provides: "Draw functions that will target layer windows in later plans"
provides:
  - "TUI_Layer struct with panel-backed WINDOW lifecycle"
  - "tui_layer_create / tui_layer_destroy API"
  - "Global layer registry (g_layers, g_layer_count) with extern linkage"
  - "Panel library (panelw) linked in CMakeLists.txt"
affects: [03-02, 03-03, 03-04, 04-integration, 05-cels-clay-bridge]

# Tech tracking
tech-stack:
  added: [panelw]
  patterns: [panel-backed-layer, swap-remove-compaction, userptr-reverse-lookup]

key-files:
  created:
    - include/cels-ncurses/tui_layer.h
    - src/layer/tui_layer.c
  modified:
    - CMakeLists.txt

key-decisions:
  - "TUI_Layer struct stores name[64], PANEL*, WINDOW*, x, y, width, height, visible"
  - "Fixed-capacity g_layers[32] array with swap-remove compaction on destroy"
  - "set_panel_userptr for PANEL* to TUI_Layer* reverse lookup"
  - "del_panel before delwin (correct cleanup order per ncurses docs)"

patterns-established:
  - "Panel-backed layer: each layer owns WINDOW + PANEL pair, created together, destroyed together"
  - "Swap-remove compaction: destroyed slot filled by last element, userptr updated for moved element"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 3 Plan 1: Layer Types Summary

**TUI_Layer struct with panel-backed create/destroy, global layer registry, and panelw library linkage**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T08:12:09Z
- **Completed:** 2026-02-08T08:14:10Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- TUI_Layer struct defined with name, panel, win, position, dimensions, and visibility fields
- tui_layer_create allocates newwin + new_panel with userptr reverse lookup
- tui_layer_destroy frees del_panel then delwin with swap-remove array compaction
- Panel library (panelw) found and linked in CMakeLists.txt for both cels-ncurses and draw_test targets

## Task Commits

Each task was committed atomically:

1. **Task 1: Add panel library to CMake and create tui_layer.h** - `1752313` (feat)
2. **Task 2: Implement tui_layer_create and tui_layer_destroy** - `b120a5f` (feat)

**Plan metadata:** (pending)

## Files Created/Modified
- `include/cels-ncurses/tui_layer.h` - TUI_Layer struct, TUI_LAYER_MAX constant, extern declarations, create/destroy API
- `src/layer/tui_layer.c` - Global layer array definition, create/destroy implementation with panel lifecycle
- `CMakeLists.txt` - find_library(panelw), target_link_libraries, target_sources for tui_layer.c

## Decisions Made
None - followed plan as specified.

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Layer types and lifecycle ready for 03-02 (layer operations: show/hide, move, resize, z-order)
- Global layer registry available for layer-to-DrawContext bridge in 03-03
- Panel library linked and verified, ready for update_panels() integration in 03-04

---
*Phase: 03-layer-system*
*Completed: 2026-02-08*
