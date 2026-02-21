# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-20)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** Phase 7 - True Color (COMPLETE)

## Current Position

Phase: 7 of 10 (True Color) -- COMPLETE
Plan: 2 of 2
Status: Phase complete
Last activity: 2026-02-21 -- Completed 07-02-PLAN.md

Progress: [################....] 80% (v1.0 complete, v1.1 4/10 plans)

## Performance Metrics

**v1.0 Velocity:**
- Total plans completed: 15
- Average duration: 2 min
- Total execution time: 0.50 hours

**v1.1 Velocity:**
- Total plans completed: 4
- Average duration: 3.5 min
- Total execution time: 14 min

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
- [06-02]: Keep cels/backend.h include in game.c -- still needed for ECS context functions
- [06-02]: Remove CELS_Context* ctx only when solely used for cels_input_get(ctx)
- [07-01]: color_mode uses int convention: 0=auto, 1-3=mode+1 to avoid cross-header include
- [07-01]: wattr_set opts pointer replaces (short)pair,NULL for extended pair support in all modes
- [07-01]: Detection priority: tigetflag(RGB) > COLORTERM+can_change_color > can_change_color > 256-fallback
- [07-01]: Palette allocator manages slots 16-255 only (slots 0-15 are user terminal theme)
- [07-02]: Standalone examples that bypass Engine_use() must call tui_color_init(-1) explicitly after start_color()

### Pending Todos

None.

### Reference: Clay ncurses renderer PR (nicbarker/clay#569)

Lessons from reviewing an upstream Clay ncurses renderer. Relevant for v1.1+:
- Wide-char text measurement: use `mbtowc()`/`wcwidth()` loop for correct column width
- Clay uses logical "pixel" units -- cels-clay will need CELL_WIDTH/CELL_HEIGHT constants

### Blockers/Concerns

- Pre-existing: tui_window.h references CELS_WindowState type removed from cels core (Phase 26 refactoring). Does not block Phase 8 work but affects full project build.

## Session Continuity

Last session: 2026-02-21
Stopped at: Completed 07-02-PLAN.md (Phase 7 complete, 2 of 2 plans)
Resume file: None
