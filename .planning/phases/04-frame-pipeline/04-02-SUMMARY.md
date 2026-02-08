---
phase: 04-frame-pipeline
plan: 02
subsystem: rendering
tags: [ncurses, frame-pipeline, integration, overwrite-bridge, panels, ecs, resize]

# Dependency graph
requires:
  - phase: 04-frame-pipeline-01
    provides: TUI_FrameState, tui_frame_init, tui_frame_begin/end, tui_frame_register_systems, tui_frame_invalidate_all, tui_frame_get_background
  - phase: 03-layer-system
    provides: TUI_Layer struct, tui_layer_resize_all, tui_layer_get_draw_context
provides:
  - Frame loop simplified to Engine_Progress + usleep (no direct panel/refresh calls)
  - Engine module wires frame pipeline init and ECS system registration
  - Renderer uses background layer via overwrite bridge (stdscr -> bg panel window)
  - KEY_RESIZE handler invalidates all layers for full redraw
  - Window dimensions use COLS/LINES instead of hardcoded values
  - Single doupdate() per frame (in frame_end only)
affects: [05-integration renderer migration to direct DrawContext]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "overwrite() bridge: widgets draw to stdscr, overwrite copies to background panel window"
    - "Frame compositing centralized in frame_end ECS system (single doupdate per frame)"

key-files:
  created: []
  modified:
    - src/tui_engine.c
    - src/window/tui_window.c
    - src/renderer/tui_renderer.c
    - src/input/tui_input.c
    - include/cels-ncurses/tui_engine.h

key-decisions:
  - "overwrite(stdscr, bg->win) bridge for Phase 4 renderer migration -- widgets still write to stdscr, full DrawContext migration deferred to Phase 5"
  - "werase(stdscr) instead of erase() for explicit target clarity"

patterns-established:
  - "stdscr overwrite bridge: intermediate pattern until widgets migrate to DrawContext"
  - "Frame pipeline wiring: init + register_systems called after tui_renderer_init in engine module"

# Metrics
duration: 2min
completed: 2026-02-08
---

# Phase 4 Plan 2: Frame Pipeline Integration Summary

**Frame loop simplified to single Engine_Progress call, renderer bridged to background layer via overwrite(), KEY_RESIZE invalidates all layers, window dimensions fixed to COLS/LINES**

## Performance

- **Duration:** 2 min
- **Started:** 2026-02-08T18:28:55Z
- **Completed:** 2026-02-08T18:30:57Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Removed duplicate update_panels/doupdate from frame loop (frame_end handles compositing)
- Wired tui_frame_init and tui_frame_register_systems into engine module initialization
- Migrated renderer from wnoutrefresh(stdscr) to overwrite() bridge through background layer
- Added tui_frame_invalidate_all in KEY_RESIZE handler after tui_layer_resize_all
- Fixed hardcoded 600x800 window dimensions to use COLS/LINES

## Task Commits

Each task was committed atomically:

1. **Task 1: Wire frame pipeline into engine module and simplify frame loop** - `17c7b30` (feat)
2. **Task 2: Migrate renderer from stdscr to background layer via overwrite bridge** - `5ab17e9` (feat)

## Files Created/Modified
- `include/cels-ncurses/tui_engine.h` - Added tui_frame.h include for consumer access
- `src/tui_engine.c` - Added tui_frame_init + tui_frame_register_systems calls after renderer init
- `src/window/tui_window.c` - Removed panel.h, update_panels/doupdate; fixed dimensions to COLS/LINES
- `src/renderer/tui_renderer.c` - Added overwrite bridge, replaced erase with werase, removed wnoutrefresh
- `src/input/tui_input.c` - Added tui_frame_invalidate_all in KEY_RESIZE handler

## Decisions Made
None - followed plan as specified. All implementation choices were pre-decided in CONTEXT.md and RESEARCH.md.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 4 (Frame Pipeline) is complete: core pipeline + integration both done
- Ready for Phase 5: migrate widgets from mvprintw/stdscr to tui_draw_text(&ctx) using DrawContext directly, eliminating the overwrite bridge
- All success criteria met: single doupdate per frame, background layer compositing, dirty tracking, ECS bracket systems

---
*Phase: 04-frame-pipeline*
*Completed: 2026-02-08*
