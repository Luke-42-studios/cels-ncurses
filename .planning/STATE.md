# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** v0.2.0 Phase 0 - CELS Module Registration (prerequisite for Phase 1)

## Current Position

Milestone: v0.2.0 ECS Module Architecture
Phase: 0 of 6 (CELS Module Registration -- prerequisite in cels repo)
Plan: 1 of 3 in current phase
Status: In progress -- Plan 01 complete (dual-remote + release script)
Last activity: 2026-02-27 -- Completed 00-01-PLAN.md (dual-remote workflow + release script)

Progress: [█░░░░░░░░░] ~3% (1/3 Phase 0 plans, 6 phases total)

## Performance Metrics

**v1.0 Velocity:**
- Total plans completed: 15
- Average duration: 2 min
- Total execution time: 0.50 hours

**v1.1 Velocity:**
- Total plans completed: 6
- Average duration: 3.7 min
- Total execution time: 22 min

## Accumulated Context

### Decisions

- [v0.2.0]: Monolithic Engine module replaced by single NCurses module with components/systems/entities
- [v0.2.0]: Window, Input, FramePipeline are components and systems, NOT sub-modules
- [v0.2.0]: Developer configures by setting component data; NCurses systems react via ECS queries
- [v0.2.0]: 6 phases derived from requirement categories: Module Boundary, Window Entity, Input System, Layer Entities, Frame Pipeline, Demo
- [v0.2.0]: cels_register(NCurses) is the developer API — requires Phase 0 fix in cels repo (Name_register alias)
- [v0.2.0]: Module provides CEL_Define(NCursesWindow, ...) + call macro — developer writes NCursesWindow(.title = "X") {}
- [v0.2.0]: cel_watch(entity, NCurses_WindowState) for reactive state reading — recomposes on resize
- [v0.2.0]: CELS_REGISTER phase to be removed (unused) in Phase 0
- [00-01]: Dual-remote: origin=Luke-42-studios/cels (dev), public=42-Galaxies/cels -- standard for all libs
- [00-01]: scripts/release.sh uses git archive + manual path exclusion (no git-filter-repo dependency)
- [00-01]: Active development on v0.4 branch (ahead of main); both branches pushed to dev remote

### Carried Forward from v1.1

- alloc_pair (not init_pair) for color pairs -- built-in LRU eviction
- wattr_set (not attron/attroff) for atomic attribute application
- werase (not wclear) to avoid flicker
- stdscr for input only, all drawing through layers
- U+2584 (lower half block) as canonical sub-cell character
- Lazy allocation for subcell buffers

### Blockers/Concerns

- tui_window.h references CELS_WindowState type removed from cels core -- will be resolved in Phase 1

## Session Continuity

Last session: 2026-02-27
Stopped at: Completed 00-01-PLAN.md (dual-remote workflow + release script in cels repo)
Resume file: None
Next: 00-02-PLAN.md (CEL_Module macro fix + CELS_REGISTER removal)
