# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-07)

**Core value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal
**Current focus:** Phase 5 - Integration and Migration

## Current Position

Phase: 5 of 5 (Integration and Migration)
Plan: 1 of 3 in current phase
Status: In progress
Last activity: 2026-02-08 -- Completed 05-01-PLAN.md

Progress: [#################...] 88% (14/16 plans)

## Performance Metrics

**Velocity:**
- Total plans completed: 14
- Average duration: 2 min
- Total execution time: 0.43 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1. Foundation | 3/3 | 5 min | 1.7 min |
| 2. Drawing Primitives | 5/5 | 9 min | 1.8 min |
| 3. Layer System | 3/3 | 5 min | 1.7 min |
| 4. Frame Pipeline | 2/2 | 4 min | 2.0 min |
| 5. Integration | 1/3 | 3 min | 3.0 min |

**Recent Trend:**
- Last 5 plans: 05-01 (3 min), 04-02 (2 min), 04-01 (2 min), 03-03 (1 min), 03-02 (2 min)
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
- TUI_Layer struct: name[64], PANEL*, WINDOW*, x, y, width, height, visible, dirty -- panel-backed with swap-remove compaction
- g_layers[TUI_LAYER_MAX=32] with extern linkage (declared in .h, defined in .c) -- validated INTERFACE library pattern
- set_panel_userptr for PANEL* to TUI_Layer* reverse lookup
- del_panel before delwin (correct cleanup order per ncurses docs)
- move_panel (not mvwin) for layer repositioning -- mvwin bypasses panel tracking
- wresize then replace_panel for resize -- replace_panel updates panel size bookkeeping
- DrawContext from layer uses local (0,0) origin -- each panel WINDOW has own coordinate system
- tui_layer_resize_all resizes every layer to full terminal size; apps needing per-layer policies iterate g_layers directly
- Resize ordering: resize all layers FIRST, then update WindowState, then notify observers (prevents stale-dimension draws)
- Frame loop uses update_panels() + doupdate() -- panel library handles overlap resolution and refresh ordering
- werase (not wclear) for dirty layer clearing -- avoids flicker from clearok
- wattr_set(A_NORMAL, 0, NULL) style reset on dirty layers at frame_begin
- Auto-dirty on DrawContext access (tui_layer_get_draw_context sets layer->dirty = true)
- Background layer created at init and lowered to z=0 via tui_layer_lower
- Frame timing via clock_gettime(CLOCK_MONOTONIC) for accurate delta_time and fps
- ECS frame systems: FrameBegin at EcsPreStore, FrameEnd at EcsPostFrame
- overwrite(stdscr, bg->win) bridge for renderer migration -- widgets still write to stdscr, full DrawContext migration deferred to Phase 5
- Frame loop simplified: Engine_Progress + usleep only (no update_panels/doupdate in window loop)
- tui_frame_init + tui_frame_register_systems wired in engine module after tui_renderer_init
- KEY_RESIZE calls tui_frame_invalidate_all after tui_layer_resize_all
- Widget render functions use explicit (x, y) coordinates with caller-managed layout (no g_render_row global)
- theme_init() with static bool guard for runtime TUI_Style initialization (tui_color_rgb not constexpr)
- App-level render provider registered via _CEL_DefineFeature + _CEL_Provides (same pattern as module renderer)
- Both old module renderer and new app renderer temporarily coexist (Plan 02 removes old)

### Pending Todos

None -- Plan 05-01 complete, Plan 02 next.

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

- Phase 3 risk RESOLVED: stdscr-to-panels migration complete. INTERFACE library extern globals validated.
- Phase 4 RESOLVED: duplicate update_panels/doupdate removed from tui_window_frame_loop. Frame_end is the single compositing point.
- Phase 5 note: App-level DrawContext widgets created (05-01). Old module renderer still active. Plan 02 removes module renderer files and wires app renderer exclusively.

## Session Continuity

Last session: 2026-02-08
Stopped at: Completed 05-01-PLAN.md (widget extraction + render provider)
Resume file: None
