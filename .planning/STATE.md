# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** v0.2.0 Phase 1.1 - CELS API Purge IN PROGRESS (Plan 01 done, Plan 02 next)

## Current Position

Milestone: v0.2.0 ECS Module Architecture
Phase: 1.1 of 6 (CELS API Purge)
Plan: 1 of 2 in current phase
Status: In progress -- Plan 01 complete, Plan 02 (system registration purge) next
Last activity: 2026-02-28 -- Completed 01.1-01-PLAN.md (ECS bridge + window purge)

Progress: [█████░░░░░] 50% (7/14 estimated plans)

## Performance Metrics

**v1.0 Velocity:**
- Total plans completed: 15
- Average duration: 2 min
- Total execution time: 0.50 hours

**v1.1 Velocity:**
- Total plans completed: 6
- Average duration: 3.7 min
- Total execution time: 22 min

**v0.2.0 Velocity:**
- Phase 0: 3 plans in ~16 min total
- Phase 1: Plan 01 in 3 min, Plan 02 in 4 min, Plan 03 in 3 min (10 min total)
- Phase 1.1: Plan 01 in 4 min

## Accumulated Context

### Decisions

- [v0.2.0]: Monolithic Engine module replaced by single NCurses module with components/systems/entities
- [v0.2.0]: Window, Input, FramePipeline are components and systems, NOT sub-modules
- [v0.2.0]: Developer configures by setting component data; NCurses systems react via ECS queries
- [v0.2.0]: 6 phases derived from requirement categories: Module Boundary, Window Entity, Input System, Layer Entities, Frame Pipeline, Demo
- [v0.2.0]: cels_register(NCurses) is the developer API -- fixed in Phase 0 (Name_register alias)
- [v0.2.0]: Module provides CEL_Define(NCursesWindow, ...) + call macro -- developer writes NCursesWindow(.title = "X") {}
- [v0.2.0]: cel_watch(entity, NCurses_WindowState) for reactive state reading -- recomposes on resize
- [v0.2.0]: CELS_REGISTER phase removed in Phase 0 (7-phase pipeline)
- [v0.2.0]: Dual-remote: Luke-42-studios/cels (origin/dev), 42-Galaxies/cels (public)
- [v0.2.0]: cels repo working branch is v0.6.0 (branched from main after v0.5.1 merge)
- [v0.2.0]: build-docs.sh auto-injects version from CMakeLists.txt into docs
- [P1-01]: Per-TU NCurses_register() static inline delegates to extern NCurses_init() for INTERFACE library pattern
- [P1-01]: Weak stubs for observer/system registration allow incremental module assembly
- [P1-01]: cels-widgets dependency removed from NCurses INTERFACE library
- [P1-02]: Observer-based terminal lifecycle replaces CELS_BackendDesc hook system
- [P1-02]: ncurses_terminal_init() extracted to take NCurses_WindowConfig* instead of global config
- [P1-02]: Strong symbol overrides of weak stubs for all 4 registration functions
- [P1-02]: Input 'q' handler calls cels_request_quit() for session loop integration
- [P1-03]: Deleted 11 legacy files (2318 lines) -- Engine, CelsNcurses, widgets, space_invaders
- [P1-03]: minimal.c reference implementation proves full lifecycle: register -> entity -> watch -> session
- [P1-03]: CMake minimal target links cels-ncurses INTERFACE library for consumer compilation
- [P1.1-01]: ncurses_ecs_bridge.c is sole flecs.h contact point for observer/component-set operations
- [P1.1-01]: Observer callbacks moved to bridge; tui_window.c has zero flecs API references
- [P1.1-01]: Bridge accessor pattern (get/set) for cross-TU static state access

### Carried Forward from v1.1

- alloc_pair (not init_pair) for color pairs -- built-in LRU eviction
- wattr_set (not attron/attroff) for atomic attribute application
- werase (not wclear) to avoid flicker
- stdscr for input only, all drawing through layers
- U+2584 (lower half block) as canonical sub-cell character
- Lazy allocation for subcell buffers

### Roadmap Evolution

- Phase 1.1 inserted after Phase 1: CELS API Purge -- replace all direct flecs/ecs calls with CELS abstractions (URGENT)

### Blockers/Concerns

- Phase 1 awaiting human verification that minimal example builds and runs
- tui_window.c purged of flecs API (Plan 01 done); tui_input.c and tui_frame.c still need purge (Plan 02)
- Build verification deferred to Plan 02 final build
- TUI_Input_use() no-op stub still in tui_input.c (dead code, can be cleaned up in Phase 3)

## Session Continuity

Last session: 2026-03-01T01:18:26Z
Stopped at: Completed 01.1-01-PLAN.md (ECS bridge + window purge)
Resume file: None
