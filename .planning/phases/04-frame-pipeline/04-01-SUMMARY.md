---
phase: 04-frame-pipeline
plan: 01
subsystem: rendering
tags: [ncurses, frame-pipeline, dirty-tracking, retained-mode, ecs, clock_gettime, panels]

# Dependency graph
requires:
  - phase: 03-layer-system
    provides: TUI_Layer struct, g_layers array, panel-backed layers, tui_layer_get_draw_context
provides:
  - TUI_FrameState struct with frame_count, delta_time, fps, in_frame
  - g_frame_state extern global (INTERFACE library pattern)
  - tui_frame_begin/end frame lifecycle
  - tui_frame_invalidate_all for forced full redraws
  - tui_frame_init with default background layer at z=0
  - tui_frame_register_systems for ECS integration at PreStore/PostFrame
  - Dirty flag per layer with auto-mark on DrawContext access
affects: [04-02 frame loop integration, 05-integration renderer migration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Retained-mode dirty tracking: only dirty+visible layers cleared per frame"
    - "Frame timing via clock_gettime(CLOCK_MONOTONIC)"
    - "ECS system registration at PreStore/PostFrame bracketing renderer"
    - "Background layer at z=0 as default drawing surface"

key-files:
  created:
    - include/cels-ncurses/tui_frame.h
    - src/frame/tui_frame.c
  modified:
    - include/cels-ncurses/tui_layer.h
    - src/layer/tui_layer.c
    - CMakeLists.txt

key-decisions:
  - "werase (not wclear) for dirty layer clearing -- avoids flicker from clearok"
  - "wattr_set(A_NORMAL, 0, NULL) style reset on dirty layers at frame_begin"
  - "uint64_t for frame_count (avoids 32-bit overflow at high frame rates)"
  - "Background layer created at init and lowered to z=0 via tui_layer_lower"
  - "Auto-dirty on DrawContext access (first line of tui_layer_get_draw_context)"

patterns-established:
  - "Frame lifecycle bracket: begin clears dirty, end composites via panels"
  - "Dirty flag pattern: bool in struct, auto-set on access, cleared in begin"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 4 Plan 1: Frame Pipeline Core Summary

**Retained-mode frame pipeline with dirty tracking, CLOCK_MONOTONIC timing, ECS-registered begin/end at PreStore/PostFrame, and default background layer at z=0**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T18:25:11Z
- **Completed:** 2026-02-08T18:27:22Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Frame pipeline header and implementation with 6 public functions
- Retained-mode dirty tracking: only dirty+visible layers cleared with werase + style reset
- Frame timing via clock_gettime(CLOCK_MONOTONIC) for accurate delta_time and fps
- Default fullscreen background layer at z=0 replacing stdscr usage
- ECS systems bracketing renderer: FrameBegin at PreStore, FrameEnd at PostFrame
- Auto-dirty flag on TUI_Layer set by tui_layer_get_draw_context

## Task Commits

Each task was committed atomically:

1. **Task 1: Create tui_frame.h header and add dirty flag to TUI_Layer** - `70d9d40` (feat)
2. **Task 2: Implement tui_frame.c with frame lifecycle, dirty tracking, timing, and background layer** - `be51a07` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_frame.h` - TUI_FrameState struct, frame pipeline API declarations (6 functions)
- `src/frame/tui_frame.c` - Frame pipeline implementation: begin/end lifecycle, dirty tracking, timing, ECS registration
- `include/cels-ncurses/tui_layer.h` - Added bool dirty field to TUI_Layer struct
- `src/layer/tui_layer.c` - Auto-set dirty=true in get_draw_context, dirty=false in create
- `CMakeLists.txt` - Added src/frame/tui_frame.c to INTERFACE sources

## Decisions Made
None - followed plan as specified. All implementation choices were pre-decided in CONTEXT.md and RESEARCH.md.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Frame pipeline core is complete and builds cleanly
- Ready for Plan 04-02: frame loop integration (remove duplicate update_panels/doupdate from tui_window_frame_loop, call tui_frame_init/register_systems, migrate renderer off stdscr)
- Background layer is created but renderer still draws to stdscr (migration in 04-02)
- KEY_RESIZE handler should call tui_frame_invalidate_all after resize (to add in 04-02)

---
*Phase: 04-frame-pipeline*
*Completed: 2026-02-08*
