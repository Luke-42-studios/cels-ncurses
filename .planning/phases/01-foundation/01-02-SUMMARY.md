---
phase: 01-foundation
plan: 02
subsystem: graphics
tags: [ncurses, color, xterm-256, alloc_pair, wattr_set, style, rgb]

# Dependency graph
requires:
  - phase: none
    provides: none (first color/style module)
provides:
  - TUI_Color type with eager RGB-to-nearest-256 mapping
  - TUI_Style type combining fg/bg colors with attribute flags
  - TUI_ATTR_* bitflags (BOLD, DIM, UNDERLINE, REVERSE)
  - TUI_COLOR_DEFAULT sentinel for terminal default colors
  - tui_style_apply function using alloc_pair + wattr_set
affects: [02-drawing-primitives, 03-layer-system, 05-integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Eager color resolution: RGB mapped to xterm-256 index at creation time, not at draw time"
    - "Invisible pair management: alloc_pair handles dedup and LRU, developer never sees pair numbers"
    - "Atomic attribute application: wattr_set replaces attron/attroff for all new drawing code"

key-files:
  created:
    - include/cels-ncurses/tui_color.h
    - src/graphics/tui_color.c
  modified:
    - CMakeLists.txt

key-decisions:
  - "Use ncurses alloc_pair directly instead of custom color pair pool"
  - "Pair 0 used for default+default without calling alloc_pair"
  - "RGB mapping uses squared Euclidean distance comparing color cube and greyscale ramp"

patterns-established:
  - "src/graphics/ subdirectory for color and style implementation"
  - "TUI_ATTR_* flags as uint32_t bitflags, converted to attr_t internally"
  - "Static helper functions for internal logic (nearest_cube_index, tui_attrs_to_ncurses)"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 1 Plan 2: Color and Style System Summary

**TUI_Color with eager RGB-to-xterm-256 mapping, TUI_Style with attribute flags, and tui_style_apply using alloc_pair + wattr_set for atomic color pair resolution**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T06:45:53Z
- **Completed:** 2026-02-08T06:48:06Z
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments
- Created TUI_Color type that eagerly resolves RGB to nearest xterm-256 color index using squared Euclidean distance comparison against both the 6x6x6 color cube and greyscale ramp
- Created TUI_Style type combining foreground/background colors with TUI_ATTR_* attribute bitflags
- Implemented tui_style_apply that uses pair 0 for default colors and alloc_pair for everything else, with wattr_set for atomic attribute application (no attron/attroff)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create tui_color.h with TUI_Color, TUI_Style, attribute flags, and function declarations** - `0cf42ab` (feat)
2. **Task 2: Implement tui_color.c with RGB mapping, attribute conversion, and style application** - `8b37c7f` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_color.h` - Color/style type definitions, attribute flag macros, function declarations
- `src/graphics/tui_color.c` - RGB-to-nearest-256 mapping, attribute conversion, tui_style_apply implementation
- `CMakeLists.txt` - Added tui_color.c to INTERFACE target_sources, added -lm to target_link_libraries

## Decisions Made
- Used ncurses alloc_pair directly for color pair management (built-in LRU eviction and dedup, no custom pool needed)
- Special-cased default+default (both fg and bg index -1) to use pair 0 directly without calling alloc_pair, avoiding wasted pair slot
- RGB mapping compares squared Euclidean distance between color cube match and greyscale ramp match, returning the closer index

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Color and style system complete, ready for use by all drawing primitives in Phase 2
- TUI_DrawContext (Plan 01-03) will use TUI_Style via tui_style_apply for styled drawing
- No blockers for subsequent plans

---
*Phase: 01-foundation*
*Completed: 2026-02-08*
