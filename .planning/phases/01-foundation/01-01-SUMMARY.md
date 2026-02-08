---
phase: 01-foundation
plan: 01
subsystem: types
tags: [c99, math, floorf, ceilf, coordinate-mapping, geometry, header-only]

# Dependency graph
requires:
  - phase: none
    provides: first plan in project
provides:
  - TUI_Rect (float x/y/w/h) -- Clay_BoundingBox compatible coordinate type
  - TUI_CellRect (int x/y/w/h) -- ncurses cell-grid coordinate type
  - tui_rect_to_cells -- float-to-cell conversion (floorf position, ceilf dimensions)
  - tui_cell_rect_intersect -- rectangle intersection for scissor/clip
  - tui_cell_rect_contains -- point-in-rect containment test
affects: [01-02, 01-03, 02-drawing-primitives, 03-layer-system]

# Tech tracking
tech-stack:
  added: [math.h (floorf, ceilf)]
  patterns: [header-only static inline, value-type structs with stack allocation]

key-files:
  created: [include/cels-ncurses/tui_types.h]
  modified: []

key-decisions:
  - "x/y/w/h representation (not x1/y1/x2/y2) -- matches Clay_BoundingBox and ncurses conventions"
  - "Zero-area rect for no-overlap intersection -- w=0,h=0 sentinel instead of error return"
  - "int (signed) for CellRect coordinates -- negative values valid for off-screen clipping"

patterns-established:
  - "Header-only static inline: geometry utilities have no .c file, no link-time dependency beyond math.h"
  - "Section separators with = dividers for grouping within header files"

# Metrics
duration: 1min
completed: 2026-02-08
---

# Phase 1 Plan 01: Coordinate Types Summary

**TUI_Rect and TUI_CellRect value types with floorf/ceilf conversion, rect intersection, and point containment -- header-only in tui_types.h**

## Performance

- **Duration:** 1 min
- **Started:** 2026-02-08T06:45:50Z
- **Completed:** 2026-02-08T06:47:12Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments
- Created the foundational coordinate type vocabulary for the graphics API
- TUI_Rect stores float coordinates matching Clay_BoundingBox layout output
- TUI_CellRect stores integer cell coordinates for ncurses drawing functions
- tui_rect_to_cells converts floats to cells using floorf (position) and ceilf (dimensions)
- tui_cell_rect_intersect computes rectangle intersection for scissor/clip operations
- tui_cell_rect_contains tests point-in-rect for hit detection
- All functions are static inline -- zero link-time cost beyond math.h

## Task Commits

Each task was committed atomically:

1. **Task 1: Create tui_types.h with TUI_Rect, TUI_CellRect, and geometry utilities** - `7d750de` (feat)

**Plan metadata:** [pending]

## Files Created/Modified
- `include/cels-ncurses/tui_types.h` - Coordinate types (TUI_Rect, TUI_CellRect) and geometry utilities (conversion, intersection, containment)

## Decisions Made
- Used `x/y/w/h` representation matching Clay_BoundingBox and ncurses conventions (not `x1/y1/x2/y2`)
- Zero-area rect (w=0, h=0) as intersection result when no overlap exists, rather than a separate error/status return
- Signed `int` for TUI_CellRect coordinates to support negative off-screen positions without unsigned truncation bugs

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed nested C block comment in header doc**
- **Found during:** Task 1 (compile verification)
- **Issue:** Usage example in header doc block contained `/* cells = ... */` which created an illegal nested C block comment, causing parse error with `-pedantic`
- **Fix:** Changed `/* cells = ... */` to `-- cells = ...` in the doc comment
- **Files modified:** include/cels-ncurses/tui_types.h
- **Verification:** Compiles cleanly with `-std=c99 -Wall -Wextra -pedantic`
- **Committed in:** 7d750de (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 bug)
**Impact on plan:** Minor comment syntax fix. No scope change.

## Issues Encountered
None beyond the nested comment fix documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- TUI_Rect and TUI_CellRect are available for all subsequent plans
- Plan 01-02 (color/style types) can proceed -- it will use TUI_CellRect for style application targets
- Plan 01-03 (draw context) can proceed -- TUI_DrawContext will embed TUI_CellRect for clipping

---
*Phase: 01-foundation*
*Completed: 2026-02-08*
