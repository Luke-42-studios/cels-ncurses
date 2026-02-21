---
phase: 06-mouse-input
plan: 02
subsystem: input
tags: [ncurses, mouse, input-migration, space-invaders, backward-compat]

# Dependency graph
requires:
  - phase: 06-mouse-input
    plan: 01
    provides: TUI_InputState struct, tui_input_get_state() getter, backward compat shims
provides:
  - Space Invaders example fully migrated to TUI_InputState API
  - Full project build verification (zero errors)
  - No cels_input_set references in cels-ncurses sources
  - compile_commands.json updated for IDE support
affects: [future examples, cels-widgets migration, cels-clay mouse integration]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Consumer migration pattern: replace CELS_Input -> TUI_InputState, cels_input_get(ctx) -> tui_input_get_state()"

key-files:
  created: []
  modified:
    - examples/space_invaders/game.c

key-decisions:
  - "Keep cels/backend.h include in game.c -- still needed for ECS context functions (cels_get_context, cels_get_world, cels_component_add)"
  - "Remove CELS_Context* ctx only in systems that used it solely for cels_input_get(ctx) (TitleInput, GameOverInput)"
  - "Space Invaders app target not in CMake build -- game.c is a reference example compiled via INTERFACE library consumers"

patterns-established:
  - "Consumer migration: TUI_InputState replaces CELS_Input, tui_input_get_state() replaces cels_input_get(ctx) -- no context parameter needed"

# Metrics
duration: 3min
completed: 2026-02-21
---

# Phase 6 Plan 2: Space Invaders Migration Summary

**Space Invaders example migrated from CELS_Input to TUI_InputState API with full project build verification (zero errors)**

## Performance

- **Duration:** 3 min
- **Started:** 2026-02-21T07:33:44Z
- **Completed:** 2026-02-21T07:37:03Z
- **Tasks:** 2
- **Files modified:** 1 (+ compile_commands.json)

## Accomplishments
- Migrated all 3 input systems (TitleInput, GameOverInput, PlayerInput) to use TUI_InputState and tui_input_get_state()
- Removed unused CELS_Context* ctx from systems that only needed it for input (TitleInput, GameOverInput)
- Verified full cels project builds with zero errors
- Confirmed no cels_input_set or CELS_Input references remain in example or source files

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate Space Invaders from CELS_Input to TUI_InputState** - `c96f67c` (feat)
2. **Task 2: Full build verification and cleanup** - `fdfa759` (chore)

## Files Created/Modified
- `examples/space_invaders/game.c` - Replaced CELS_Input type, cels_input_get() calls, and sizeof() references with TUI_InputState equivalents
- `compile_commands.json` - Updated from build for IDE support

## Decisions Made
- Kept `#include <cels/backend.h>` in game.c because ECS context functions (cels_get_context, cels_get_world, cels_component_add, cels_get_current_entity) are still needed throughout the game
- Removed `CELS_Context* ctx` only from TitleInputSystem and GameOverInputSystem where it was solely used for cels_input_get(ctx); kept it in PlayerInputSystem which also needs cels_get_world(ctx)
- Space Invaders game.c has no standalone build target in CMake -- it is a reference example that compiles in context of INTERFACE library consumers

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 6 (Mouse Input) is complete: foundation (Plan 01) + consumer migration (Plan 02) both done
- TUI_InputState provides keyboard + mouse fields ready for hit-testing and UI interaction
- Backward compat shims (CELS_Input typedef, cels_input_get inline) remain for any external consumers
- Pre-existing tui_window.h CELS_WindowState issue unchanged (outside scope, does not block mouse input work)

---
*Phase: 06-mouse-input*
*Completed: 2026-02-21*
