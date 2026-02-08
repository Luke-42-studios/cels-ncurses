---
phase: 03-layer-system
plan: 03
subsystem: ui
tags: [ncurses, panels, resize, SIGWINCH, KEY_RESIZE, update_panels]

# Dependency graph
requires:
  - phase: 03-layer-system (03-02)
    provides: "Layer lifecycle, operations, and single-layer resize via tui_layer_resize"
provides:
  - "tui_layer_resize_all() for bulk resize on terminal resize"
  - "KEY_RESIZE handler with layer resize -> state update -> observer notification ordering"
  - "Frame loop with update_panels() + doupdate() for panel compositing"
affects: [04-event-system, 05-migration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Resize-all before notify: layers resized before observers run, preventing stale-dimension draws"
    - "update_panels() + doupdate() frame loop: panel library handles overlap resolution and refresh ordering"

key-files:
  created: []
  modified:
    - "include/cels-ncurses/tui_layer.h"
    - "src/layer/tui_layer.c"
    - "src/window/tui_window.c"
    - "src/input/tui_input.c"

key-decisions:
  - "tui_layer_resize_all resizes every layer to full terminal size (simplest policy); apps needing per-layer policies iterate g_layers directly"
  - "Resize ordering: resize all layers FIRST, then update WindowState, then notify observers"

patterns-established:
  - "Bulk-then-notify: mutate all affected state before triggering observer callbacks"
  - "Panel frame loop: update_panels() immediately before doupdate() in every frame"

# Metrics
duration: 1min
completed: 2026-02-08
---

# Phase 3 Plan 3: Resize Handling Summary

**KEY_RESIZE handler resizes all layers via tui_layer_resize_all, updates WindowState to RESIZING, and notifies observers; frame loop migrated to update_panels() + doupdate()**

## Performance

- **Duration:** 1 min
- **Started:** 2026-02-08T08:18:35Z
- **Completed:** 2026-02-08T08:19:54Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Added tui_layer_resize_all() that iterates all layers and calls tui_layer_resize on each
- Added KEY_RESIZE case to input switch block with correct ordering: resize layers -> update state -> notify
- Migrated frame loop from bare doupdate() to update_panels() + doupdate() for panel compositing

## Task Commits

Each task was committed atomically:

1. **Task 1: Add tui_layer_resize_all helper and update frame loop** - `d74509a` (feat)
2. **Task 2: Add KEY_RESIZE handler to input system** - `1424e26` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_layer.h` - Added tui_layer_resize_all() declaration
- `src/layer/tui_layer.c` - Added tui_layer_resize_all() implementation (iterates g_layers, calls tui_layer_resize)
- `src/window/tui_window.c` - Added #include <panel.h>, inserted update_panels() before doupdate() in frame loop
- `src/input/tui_input.c` - Added #include tui_layer.h, added KEY_RESIZE case with resize-all + state update + notify

## Decisions Made
- tui_layer_resize_all resizes every layer to full terminal size as simplest resize policy; applications needing per-layer sizing should iterate g_layers and call tui_layer_resize individually in their resize observer
- Resize ordering is resize-all THEN notify: prevents observer code from drawing into layers with stale dimensions

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Layer system is now complete: create, destroy, show, hide, raise, lower, move, resize, resize-all, DrawContext bridge, and panel-based frame loop
- Phase 3 Plan 4 (integration test / wiring) is the final plan to validate end-to-end layer usage
- No blockers or concerns

---
*Phase: 03-layer-system*
*Completed: 2026-02-08*
