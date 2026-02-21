# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-20)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** Phase 6 - Mouse Input (v1.1 Enhanced Rendering)

## Current Position

Phase: 6 of 10 (Mouse Input)
Plan: 1 of 2
Status: In progress
Last activity: 2026-02-21 -- Completed 06-01-PLAN.md

Progress: [###########.........] 55% (v1.0 complete, v1.1 1/10 plans)

## Performance Metrics

**v1.0 Velocity:**
- Total plans completed: 15
- Average duration: 2 min
- Total execution time: 0.50 hours

**v1.1 Velocity:**
- Total plans completed: 1
- Average duration: 4 min
- Total execution time: 4 min

## Accumulated Context

### Decisions

See PROJECT.md Key Decisions table for full list.
Recent decisions affecting current work:

- [v1.1 Roadmap]: Mouse input is Phase 6 (first, independent of rendering pipeline)
- [v1.1 Roadmap]: True color before sub-cell (half-block needs fg/bg true color for full power)
- [v1.1 Roadmap]: All draw functions before damage tracking (instrument everything in one pass)
- [06-01]: Module-local TUI_InputState replaces removed CELS_Input -- no dependency on cels/backend.h
- [06-01]: Mouse position persists across frames; button events are per-frame
- [06-01]: mouseinterval(0) disables click detection -- raw press/release only
- [06-01]: No printf escape sequences -- mousemask() only for now
- [06-01]: Backward compat typedef CELS_Input = TUI_InputState for gradual migration

### Pending Todos

None.

### Reference: Clay ncurses renderer PR (nicbarker/clay#569)

Lessons from reviewing an upstream Clay ncurses renderer. Relevant for v1.1+:
- Wide-char text measurement: use `mbtowc()`/`wcwidth()` loop for correct column width
- Clay uses logical "pixel" units -- cels-clay will need CELL_WIDTH/CELL_HEIGHT constants

### Blockers/Concerns

- Pre-existing: tui_window.h references CELS_WindowState type removed from cels core (Phase 26 refactoring). Does not block mouse input work but affects full project build.

## Session Continuity

Last session: 2026-02-21
Stopped at: Completed 06-01-PLAN.md (mouse input foundation)
Resume file: None
