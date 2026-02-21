---
phase: 08-sub-cell-rendering
plan: 01
subsystem: ui
tags: [ncurses, unicode, half-block, sub-cell, shadow-buffer, wide-char]

# Dependency graph
requires:
  - phase: 07-true-color
    provides: "TUI_Color type, tui_color_rgb(), tui_style_apply() with alloc_pair"
provides:
  - "TUI_SubCellMode enum, TUI_SubCell struct, TUI_SubCellBuffer type"
  - "Buffer lifecycle API (create/clear/resize/destroy)"
  - "DrawContext subcell_buf field for layer-to-draw-function bridge"
  - "Layer/frame integration hooks (destroy, resize, frame_begin clear)"
  - "Half-block plot and fill_rect at 1x2 virtual pixel resolution"
affects: [08-02-quadrant-braille, 08-03-draw-test-integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Shadow buffer per layer: TUI_SubCellBuffer* on TUI_Layer, lazily allocated"
    - "Write-through: shadow buffer update + immediate ncurses cell write"
    - "Last-mode-wins: mode mixing resets cell to new mode"
    - "Double pointer bridge: TUI_SubCellBuffer** on DrawContext for lazy alloc from draw functions"

key-files:
  created:
    - include/cels-ncurses/tui_subcell.h
    - src/graphics/tui_subcell.c
  modified:
    - include/cels-ncurses/tui_draw_context.h
    - include/cels-ncurses/tui_layer.h
    - src/layer/tui_layer.c
    - src/frame/tui_frame.c
    - include/cels-ncurses/tui_draw.h
    - src/graphics/tui_draw.c
    - CMakeLists.txt

key-decisions:
  - "Lazy allocation: subcell_buf starts NULL, allocated on first sub-cell draw"
  - "Forward declaration (not include) of TUI_SubCellBuffer in tui_draw_context.h and tui_layer.h to avoid header coupling"
  - "Cell-coordinate iteration in fill_rect (not per-pixel) for efficiency"
  - "U+2584 (lower half block) as canonical character: fg=bottom, bg=top"

patterns-established:
  - "subcell_ensure_buffer(): lazy alloc pattern for draw functions to use"
  - "subcell_write_halfblock(): shadow-buffer-to-ncurses write-through pattern"
  - "Mode mixing via memset + mode tag reset"

# Metrics
duration: 5min
completed: 2026-02-21
---

# Phase 8 Plan 1: Sub-Cell Buffer Infrastructure + Half-Block Rendering Summary

**Shadow buffer infrastructure with lazy allocation, layer/frame lifecycle hooks, and half-block rendering at 1x2 virtual pixel resolution using U+2584/U+2580**

## Performance

- **Duration:** 5 min
- **Started:** 2026-02-21T09:03:18Z
- **Completed:** 2026-02-21T09:08:22Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- Created the complete sub-cell type system (TUI_SubCellMode, TUI_SubCell union, TUI_SubCellBuffer)
- Integrated shadow buffer into layer lifecycle (create, destroy, resize) and frame pipeline (clear alongside werase)
- Bridged layer to draw functions via TUI_SubCellBuffer** double pointer on DrawContext
- Implemented half-block plot and fill_rect with correct fg/bg assignment and independent top/bottom preservation

## Task Commits

Each task was committed atomically:

1. **Task 1: Sub-cell buffer types, lifecycle, and integration points** - `f7fcd86` (feat)
2. **Task 2: Half-block plot and fill_rect with shadow buffer write-through** - `e84fdc3` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_subcell.h` - Sub-cell mode enum, per-cell state union, buffer type, lifecycle API declarations
- `src/graphics/tui_subcell.c` - Buffer create/clear/resize/destroy implementations
- `include/cels-ncurses/tui_draw_context.h` - Added subcell_buf field (TUI_SubCellBuffer**) with forward declaration
- `include/cels-ncurses/tui_layer.h` - Added subcell_buf field (TUI_SubCellBuffer*) with forward declaration
- `src/layer/tui_layer.c` - Buffer init (NULL), destroy, resize hooks; DrawContext bridge passes &layer->subcell_buf
- `src/frame/tui_frame.c` - Buffer clear alongside werase in dirty-layer loop
- `include/cels-ncurses/tui_draw.h` - Half-block plot and fill_rect declarations
- `src/graphics/tui_draw.c` - subcell_ensure_buffer, subcell_write_halfblock, plot, fill_rect implementations
- `CMakeLists.txt` - Added tui_subcell.c to both cels-ncurses INTERFACE and draw_test targets

## Decisions Made
None - followed plan as specified.

## Deviations from Plan
None - plan executed exactly as written.

## Issues Encountered
None.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Shadow buffer infrastructure is complete and ready for quadrant and braille modes (Plan 02)
- Half-block rendering validated via successful build; visual verification deferred to Plan 03's draw_test integration
- Pre-existing blocker noted: cels-clay CELS_Input type error prevents full `cmake --build build` with TUI backend, but cels-ncurses targets (draw_test, cels-debug) build cleanly

---
*Phase: 08-sub-cell-rendering*
*Completed: 2026-02-21*
