# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-07)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** Phase 2 - Drawing Primitives (complete)

## Current Position

Phase: 2 of 5 (Drawing Primitives)
Plan: 5 of 5 in current phase
Status: Phase complete
Last activity: 2026-02-08 -- Completed 02-02-PLAN.md (final Phase 2 plan)

Progress: [#########...........] 47% (8/17 plans)

## Performance Metrics

**Velocity:**
- Total plans completed: 8
- Average duration: 2 min
- Total execution time: 0.22 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Foundation | 3/3 | 5 min | 1.7 min |
| 2. Drawing Primitives | 5/5 | 9 min | 1.8 min |

**Recent Trend:**
- Last 5 plans: 02-01 (3 min), 02-05 (1 min), 02-04 (2 min), 02-03 (1 min), 02-02 (2 min)
- Trend: stable

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- x/y/w/h representation for TUI_Rect (matches Clay_BoundingBox and ncurses conventions)
- Signed int for TUI_CellRect coordinates (negative values valid for off-screen clipping)
- Zero-area rect (w=0, h=0) as intersection no-overlap sentinel
- Use ncurses alloc_pair directly for color pair pool (built-in LRU eviction, no custom pool)
- Pair 0 for default+default colors without alloc_pair call
- RGB mapping via squared Euclidean distance against color cube and greyscale ramp
- TUI_DrawContext embeds single TUI_CellRect clip field (not a clip stack)
- clip defaults to full drawable area on creation (scissor stack updates it in Phase 2)
- WINDOW* is borrowed, not owned -- caller manages lifetime
- _XOPEN_SOURCE 700 required for ncurses wide-char API (WACS_ macros, mvwadd_wch, setcchar, cchar_t) -- now set globally via CMakeLists.txt target_compile_definitions, do NOT add to .c files
- Scissor stack: silent early return for overflow/underflow (no assert, matching project error handling convention)

### Pending Todos

None yet.

### Reference: Clay ncurses renderer PR (nicbarker/clay#569)

Lessons from reviewing an upstream Clay ncurses renderer. Our architecture is stronger (layers, alloc_pair, wattr_set, cell-space clipping) but these specific techniques are worth adopting:

**Phase 2 -- Text rendering (DRAW-03, DRAW-04):**
- Wide-char text measurement: use `mbtowc()`/`wcwidth()` loop to get correct column width for multibyte UTF-8 strings. Cannot assume 1 byte = 1 column.
- Bounded/clipped text rendering: when text is partially clipped, must skip N columns from left and take M columns -- non-trivial with wide chars. Track column offset per `wchar_t`, use `mvaddnwstr()` for the visible slice.

**Phase 5 -- cels-clay bridge (MIGR-04):**
- Clay uses logical "pixel" units (e.g., 8.0f width, 16.0f height per cell). The cels-clay module will need `CELL_WIDTH`/`CELL_HEIGHT` constants to convert `Clay_BoundingBox` floats into our `TUI_Rect` floats. This validates our float->cell conversion as the right abstraction boundary.

**Anti-patterns confirmed (things we already avoid):**
- Manual color pair cache with init_pair + linear search (we use alloc_pair with built-in LRU)
- attron/attroff state leaks (we use wattr_set atomically)
- mvinch to sample background color for text (our layer system eliminates this need)
- Drawing everything to stdscr (our panel-backed layers give z-ordering)

### Blockers/Concerns

- Phase 3 (Layer System) is highest risk: stdscr-to-panels migration must be complete, no partial adoption. Mixing stdscr with panels causes visual corruption.
- INTERFACE library static globals: Layer manager global state needs extern declarations with single definition in one .c file. Needs validation during Phase 3.

## Session Continuity

Last session: 2026-02-08T07:42:36Z
Stopped at: Completed 02-02-PLAN.md (Phase 2 complete)
Resume file: None
