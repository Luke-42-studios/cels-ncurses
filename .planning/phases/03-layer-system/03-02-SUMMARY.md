---
phase: 03-layer-system
plan: 02
subsystem: ui
tags: [ncurses, panel, layer, show-hide, z-order, move, resize, draw-context]

# Dependency graph
requires:
  - phase: 03-layer-system
    plan: 01
    provides: "TUI_Layer struct, tui_layer_create/destroy, global layer registry"
  - phase: 01-foundation
    provides: "TUI_DrawContext, tui_draw_context_create"
provides:
  - "tui_layer_show / tui_layer_hide - panel visibility control"
  - "tui_layer_raise / tui_layer_lower - z-order manipulation"
  - "tui_layer_move - panel-safe repositioning"
  - "tui_layer_resize - wresize + replace_panel size update"
  - "tui_layer_get_draw_context - bridge from layer to Phase 2 drawing"
affects: [03-03, 03-04, 04-integration, 05-cels-clay-bridge]

# Tech tracking
tech-stack:
  added: []
  patterns: [move_panel-not-mvwin, wresize-then-replace_panel, local-coordinate-draw-context]

key-files:
  created: []
  modified:
    - include/cels-ncurses/tui_layer.h
    - src/layer/tui_layer.c

key-decisions:
  - "move_panel (not mvwin) for layer repositioning -- mvwin bypasses panel tracking"
  - "wresize then replace_panel for resize -- replace_panel updates panel size bookkeeping"
  - "DrawContext uses local (0,0) origin -- each panel WINDOW has its own coordinate system"
  - "DrawContext borrows layer WINDOW -- layer manages WINDOW lifetime"

patterns-established:
  - "Layer-to-DrawContext bridge: tui_layer_get_draw_context returns local-origin context"
  - "Panel-safe movement: always move_panel, never mvwin on panel-managed windows"
  - "Panel-safe resize: wresize + replace_panel as mandatory pair"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 3 Plan 2: Layer Operations Summary

**Show/hide, raise/lower, move, resize operations and layer-to-DrawContext bridge for Phase 2 drawing**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T08:15:41Z
- **Completed:** 2026-02-08T08:17:16Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- tui_layer_show/hide toggle panel visibility via show_panel/hide_panel, tracking layer->visible
- tui_layer_raise/lower manipulate z-order via top_panel/bottom_panel
- tui_layer_move repositions via move_panel (not mvwin) and updates cached x/y
- tui_layer_resize calls wresize then replace_panel and updates cached width/height
- tui_layer_get_draw_context bridges layers to Phase 2 drawing with local (0,0) origin
- All functions have null-guard early returns
- No mvwin, wrefresh, or wnoutrefresh calls in tui_layer.c

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement show/hide, raise/lower, and move operations** - `793ed1b` (feat)
2. **Task 2: Implement resize and layer-to-DrawContext bridge** - `d048832` (feat)

## Files Modified
- `include/cels-ncurses/tui_layer.h` - Added tui_draw_context.h include; show/hide, raise/lower, move, resize, get_draw_context declarations
- `src/layer/tui_layer.c` - All 7 new function implementations (show, hide, raise, lower, move, resize, get_draw_context)

## Decisions Made
None new - followed plan as specified. Key API constraints (move_panel not mvwin, wresize+replace_panel pair, local origin DrawContext) were prescribed by the plan.

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Full layer operation API ready for 03-03 (resize handling / terminal resize events)
- Layer-to-DrawContext bridge enables drawing into layers with all Phase 2 primitives
- Ready for 03-04 (frame loop integration with update_panels + doupdate)

---
*Phase: 03-layer-system*
*Completed: 2026-02-08*
