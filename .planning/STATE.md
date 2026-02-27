# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-26)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** v0.2.0 ECS Module Architecture -- planning

## Current Position

Milestone: v0.2.0 ECS Module Architecture
Phase: None (milestone planning)
Status: Planning
Last activity: 2026-02-26 -- Closed v1.1, began v0.2.0 planning

## Architecture Direction

**Problem:** v1.0/v1.1 used a monolithic Engine module that "provides" Window, Input, FramePipeline as sub-modules. This doesn't follow CELS ECS patterns.

**New approach:** One NCurses module that registers:
- **Components**: Window config, Input state, Color config, Layer, DrawContext, etc.
- **Systems**: Input polling, frame begin/end, rendering -- hooked into CELS pipeline phases
- **Entities**: Developer creates entities with NCurses components to configure behavior

Developer updates component data to tell NCurses what they need. NCurses systems react through ECS queries in the appropriate phases.

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

### Carried Forward from v1.1

- alloc_pair (not init_pair) for color pairs -- built-in LRU eviction
- wattr_set (not attron/attroff) for atomic attribute application
- werase (not wclear) to avoid flicker
- stdscr for input only, all drawing through layers
- U+2584 (lower half block) as canonical sub-cell character
- Lazy allocation for subcell buffers

### Blockers/Concerns

- tui_window.h references CELS_WindowState type removed from cels core -- will be resolved in v0.2.0 restructure

## Session Continuity

Last session: 2026-02-26
Stopped at: v1.1 closed, v0.2.0 milestone planning
Resume file: None
