---
phase: 02-drawing-primitives
plan: 01
subsystem: ui
tags: [ncurses, box-drawing, WACS, cchar_t, rectangle, border, clipping]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: TUI_CellRect, TUI_Style, TUI_DrawContext, tui_cell_rect_intersect, tui_cell_rect_contains, tui_style_apply
provides:
  - tui_draw.h header with all Phase 2 type declarations and function prototypes
  - TUI_BorderStyle enum (SINGLE/DOUBLE/ROUNDED)
  - TUI_BorderChars struct for border character lookup
  - TUI_SIDE_* bitmask defines for per-side border control
  - tui_draw_fill_rect implementation (clip-then-draw filled rectangle)
  - tui_draw_border_rect implementation (outlined rectangle with box-drawing characters)
  - tui_border_chars_get implementation (border style to WACS/cchar_t lookup)
  - Rounded corner cchar_t lazy initialization (U+256D-U+2570)
affects: [02-02 text drawing, 02-03 per-side border, 02-04 scissor stack, 02-05 line drawing, 03 layer system]

# Tech tracking
tech-stack:
  added: []
  patterns: [clip-then-draw, border-style-table, _XOPEN_SOURCE-700-for-wide-char]

key-files:
  created:
    - include/cels-ncurses/tui_draw.h
    - src/graphics/tui_draw.c
  modified:
    - CMakeLists.txt

key-decisions:
  - "_XOPEN_SOURCE 700 required for ncurses wide-char API (WACS_ macros, mvwadd_wch, setcchar, cchar_t)"
  - "Rounded corners lazy-initialized via static cchar_t globals with setcchar on first use"

patterns-established:
  - "Clip-then-draw: every draw function intersects rect with ctx->clip before ncurses calls"
  - "Per-cell clip check: border_rect uses tui_cell_rect_contains for each individual cell"
  - "Border style lookup table: switch on TUI_BorderStyle returns TUI_BorderChars struct"
  - "_XOPEN_SOURCE 700 in .c files that use wide-char ncurses (not _POSIX_C_SOURCE)"

# Metrics
duration: 3min
completed: 2026-02-08
---

# Phase 2 Plan 1: Rectangle Drawing Summary

**Filled and outlined rectangle primitives with WACS box-drawing characters, clip-then-draw pattern, and complete Phase 2 header**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-08T07:27:57Z
- **Completed:** 2026-02-08T07:30:43Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Complete tui_draw.h header declaring all 8 Phase 2 drawing functions plus helper, with TUI_BorderStyle enum, TUI_BorderChars struct, and TUI_SIDE_* bitmask defines
- tui_draw_fill_rect draws filled rectangles with clip-then-draw pattern using tui_cell_rect_intersect
- tui_draw_border_rect draws outlined rectangles with single, double, or rounded box-drawing characters, per-cell clip checking via tui_cell_rect_contains
- tui_border_chars_get provides border style lookup returning WACS_ macro pointers for single/double and lazily-initialized cchar_t globals for rounded corners

## Task Commits

Each task was committed atomically:

1. **Task 1: Create tui_draw.h with all Phase 2 type declarations and function prototypes** - `cab49f7` (feat)
2. **Task 2: Implement filled rect, outlined rect, and border char infrastructure in tui_draw.c** - `d148466` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_draw.h` - Complete Phase 2 header: TUI_BorderStyle, TUI_BorderChars, TUI_SIDE_* defines, all drawing function declarations
- `src/graphics/tui_draw.c` - Filled rect, outlined rect, border char lookup, rounded corner cchar_t initialization
- `CMakeLists.txt` - Added tui_draw.c to INTERFACE target_sources

## Decisions Made
- **_XOPEN_SOURCE 700 instead of _POSIX_C_SOURCE 199309L**: ncurses wide-char API (WACS_ macros, mvwadd_wch, setcchar, cchar_t type) requires NCURSES_WIDECHAR to be enabled, which is gated behind _XOPEN_SOURCE >= 500. Changed feature test macro for tui_draw.c to _XOPEN_SOURCE 700. Future Phase 2 files that use wide-char ncurses should use the same macro.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Changed _POSIX_C_SOURCE to _XOPEN_SOURCE 700 for wide-char ncurses**
- **Found during:** Task 2 (build verification)
- **Issue:** WACS_ macros, mvwadd_wch, setcchar, and cchar_t are conditionally compiled behind NCURSES_WIDECHAR in ncurses.h, which requires _XOPEN_SOURCE_EXTENDED or _XOPEN_SOURCE >= 500. The plan specified _POSIX_C_SOURCE 199309L which does not enable wide-char support.
- **Fix:** Changed `#define _POSIX_C_SOURCE 199309L` to `#define _XOPEN_SOURCE 700` in tui_draw.c
- **Files modified:** src/graphics/tui_draw.c
- **Verification:** Build succeeds with no errors or warnings
- **Committed in:** d148466 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** Essential fix for compilation. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviation above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- tui_draw.h declares all Phase 2 function prototypes -- subsequent plans only add implementations to tui_draw.c
- Plans 02-02 through 02-05 can proceed immediately
- Future .c files using wide-char ncurses must use `_XOPEN_SOURCE 700` (not `_POSIX_C_SOURCE 199309L`)

---
*Phase: 02-drawing-primitives*
*Completed: 2026-02-08*
