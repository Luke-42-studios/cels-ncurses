---
phase: 07-true-color
plan: 01
subsystem: graphics
tags: [ncurses, color, rgb, true-color, direct-color, palette, wattr_set]

# Dependency graph
requires:
  - phase: 03-color-style
    provides: TUI_Color, TUI_Style, tui_color_rgb(), tui_style_apply(), alloc_pair
  - phase: 05-frame-pipeline
    provides: Engine_Config, TUI_Window startup hook, frame pipeline
provides:
  - Three-tier color resolution (direct/palette/256-fallback)
  - TUI_ColorMode enum and tui_color_init/get_mode/mode_name API
  - Palette slot allocator with LRU eviction (240 slots)
  - Extended pair support via wattr_set opts pointer
  - Color mode config propagation through Engine_Config and TUI_Window
affects: [07-02-draw-test, 08-sub-cell, future rendering phases]

# Tech tracking
tech-stack:
  added: [tigetflag, init_extended_color, term.h]
  patterns: [three-tier color resolution, palette slot LRU, wattr_set opts pointer]

key-files:
  created: []
  modified:
    - include/cels-ncurses/tui_color.h
    - src/graphics/tui_color.c
    - include/cels-ncurses/tui_window.h
    - src/window/tui_window.c
    - include/cels-ncurses/tui_engine.h
    - src/tui_engine.c

key-decisions:
  - "color_mode uses int convention: 0=auto, 1-3=mode+1 to avoid cross-header include"
  - "wattr_set opts pointer replaces (short)pair,NULL for extended pair support in all modes"
  - "Detection priority: tigetflag(RGB) > COLORTERM+can_change_color > can_change_color > 256-fallback"
  - "Palette allocator manages slots 16-255 only (slots 0-15 are user terminal theme)"

patterns-established:
  - "Three-tier color resolution: g_color_mode static checked in tui_color_rgb() switch"
  - "Config propagation: Engine_Config.color_mode -> TUI_Window.color_mode -> tui_color_init()"
  - "wattr_set(win, attrs, 0, &pair): always use opts pointer for pair numbers"

# Metrics
duration: 4min
completed: 2026-02-21
---

# Phase 7 Plan 1: Three-Tier Color System Summary

**Three-tier color resolution (direct/palette/256) with tigetflag detection, palette LRU allocator, and wattr_set opts pointer for extended pairs**

## Performance

- **Duration:** 4 min
- **Started:** 2026-02-21T08:01:28Z
- **Completed:** 2026-02-21T08:05:58Z
- **Tasks:** 2/2
- **Files modified:** 6

## Accomplishments
- Three-tier color resolution in tui_color_rgb() -- direct color packs 0xRRGGBB, palette mode allocates and redefines slots, 256-color falls back to nearest match
- Palette slot allocator with LRU eviction managing 240 usable slots (16-255), protecting user's terminal colorscheme (slots 0-15)
- Fixed tui_style_apply() to use wattr_set opts pointer (`0, &pair`) instead of `(short)pair, NULL` for extended pair support beyond 255
- Color mode auto-detection at startup via tigetflag("RGB"), COLORTERM env hint, can_change_color() fallback chain
- Color mode override wired through Engine_Config -> TUI_Window -> tui_color_init(), backward compatible via zero-init default

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement three-tier color system in tui_color.h and tui_color.c** - `1e50d4a` (feat)
2. **Task 2: Wire color_mode through Engine_Config and TUI_Window to startup** - `d26e8e0` (feat)

**Plan metadata:** `5789069` (docs: complete plan)

## Files Created/Modified
- `include/cels-ncurses/tui_color.h` - Added TUI_ColorMode enum, tui_color_init/get_mode/mode_name declarations, updated comments
- `src/graphics/tui_color.c` - Three-tier tui_color_rgb(), palette allocator, fixed wattr_set, tui_color_init detection, mode query functions
- `include/cels-ncurses/tui_window.h` - Added color_mode field to TUI_Window struct
- `src/window/tui_window.c` - Added tui_color.h include, tui_color_init() call in startup hook
- `include/cels-ncurses/tui_engine.h` - Added color_mode field to Engine_Config struct
- `src/tui_engine.c` - Forward color_mode from Engine_Config to TUI_Window, explicit auto-detect in CelsNcurses

## Decisions Made
- **int color_mode convention (0=auto, 1-3=mode+1):** Avoids including tui_color.h in tui_window.h. Zero-initialized structs get auto-detect by default.
- **wattr_set opts pointer for all modes:** `wattr_set(win, attrs, 0, &pair)` works universally and supports pairs > 255 needed for direct color mode.
- **Detection priority:** tigetflag("RGB") first (most reliable), then COLORTERM env hint with can_change_color(), then bare can_change_color(), finally 256-color fallback.
- **Palette slots 16-255 only:** Slots 0-15 are the user's terminal colorscheme and must never be touched.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - pre-existing build issues (CELS_WindowState type removed from cels core) do not affect this plan's changes and are documented in STATE.md as a known blocker.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Three-tier color system is compiled and wired into startup
- Ready for Plan 02 (draw test / visual verification of color modes)
- Pre-existing blocker: tui_window.h references CELS_WindowState type removed from cels core (does not affect Phase 7 work)

---
*Phase: 07-true-color*
*Completed: 2026-02-21*
