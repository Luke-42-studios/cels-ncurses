---
phase: 05-frame-pipeline
plan: 01
subsystem: rendering
tags: [ncurses, ecs, sigwinch, panel, frame-pipeline, sigprocmask]

# Dependency graph
requires:
  - phase: 04-layer-entities
    provides: TUI_LayerSystem, TUI_LayerConfig, TUI_DrawContext_Component, ncurses_layer_clear_window
provides:
  - TUI_FrameBeginSystem at PreRender (SIGWINCH blocking)
  - TUI_FrameEndSystem at PostRender (panel composite + doupdate + unblock + FPS throttle)
  - Auto-clearing of visible entity layers at PreRender
  - Input system cleaned of legacy layer/frame references
affects: [05-02-legacy-deletion, 06-demo]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Queryless CEL_System with cel_run for frame lifecycle bracketing"
    - "SIGWINCH block/unblock bracket spanning OnRender phase"

key-files:
  created: []
  modified:
    - src/ncurses_module.c
    - src/input/tui_input.c

key-decisions:
  - "TUI_FrameBeginSystem defined AFTER TUI_LayerSystem for correct registration ordering"
  - "tui_hook_frame_end() kept as function call in TUI_FrameEndSystem (not inlined)"
  - "cels_ncurses_draw.h removed from tui_input.c (no remaining references)"
  - "Single cels_register call in module init (merged two calls + new systems)"

patterns-established:
  - "Queryless systems: CEL_System(Name, .phase = Phase) { cel_run { body } } for framework lifecycle"
  - "System ordering via definition order within ncurses_module.c"

# Metrics
duration: 2min
completed: 2026-03-14
---

# Phase 5 Plan 1: Frame Pipeline Systems Summary

**TUI_FrameBeginSystem + TUI_FrameEndSystem inline in ncurses_module.c with SIGWINCH bracketing and auto-clear of visible entity layers**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-15T05:50:33Z
- **Completed:** 2026-03-15T05:52:29Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- TUI_FrameBeginSystem (PreRender) blocks SIGWINCH before developer draws
- TUI_FrameEndSystem (PostRender) composites panels, flushes terminal, unblocks SIGWINCH, throttles FPS
- TUI_LayerSystem auto-clears visible entity layers so developers get a blank canvas at OnRender
- Input system stripped of all legacy tui_layer_resize_all/tui_frame_invalidate_all/clearok calls

## Task Commits

Each task was committed atomically:

1. **Task 1: Add frame systems and layer clearing to ncurses_module.c** - `40c80f2` (feat)
2. **Task 2: Strip legacy layer/frame references from input system** - `9df0583` (fix)

## Files Created/Modified
- `src/ncurses_module.c` - Added signal.h/panel.h/tui_window.h includes, TUI_FrameBeginSystem, TUI_FrameEndSystem, layer clearing in TUI_LayerSystem, merged module init registration
- `src/input/tui_input.c` - Simplified KEY_RESIZE to consume-and-continue, removed cels_ncurses_draw.h include, updated header comment

## Decisions Made
- Kept tui_hook_frame_end() as a function call rather than inlining its body -- it accesses static timing variables in tui_window.c
- Merged two cels_register calls into one in module init for cleaner registration
- Removed cels_ncurses_draw.h from tui_input.c since no types/functions from that header remain after cleanup

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Frame systems are defined and registered inline in ncurses_module.c
- Legacy tui_frame.c still exists (contains duplicate system definitions) -- will be deleted in Plan 02
- tui_internal.h still declares ncurses_register_frame_systems() -- will be cleaned in Plan 02
- Full build verification deferred to Plan 02 when legacy files are deleted

---
*Phase: 05-frame-pipeline*
*Completed: 2026-03-14*
