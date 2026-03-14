---
phase: 04-layer-entities
plan: 01
subsystem: ui
tags: [ncurses, panels, z-order, layer, ecs, lifecycle, composition]

# Dependency graph
requires:
  - phase: 01.2-window-lifecycle-rewrite
    provides: CEL_Lifecycle + CEL_Observe pattern, cels_entity_set/get_component, cels_lifecycle_bind_entity
  - phase: 03-input-system
    provides: CEL_System pattern at pipeline phases, NCurses_InputState component pattern
provides:
  - TUI_Renderable, TUI_LayerConfig, TUI_DrawContext_Component CEL_Component types
  - TUILayer composition + call macro for entity-driven layer creation
  - TUI_LayerLC lifecycle with on_create/on_destroy for panel management
  - TUI_LayerSyncSystem for per-frame z-order and visibility synchronization
  - Internal layer entity registry with sorted z-order panel stacking
affects: [04-02-module-registration, 05-frame-pipeline, 06-demo]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Internal entity registry with cached z_order for sorted panel stacking"
    - "struct completion pattern: CEL_Component forward-declare in cels_ncurses.h, struct body in cels_ncurses_draw.h"
    - "Z-order rebuild via insertion sort + top_panel() loop (ncurses has no insert-at-position)"

key-files:
  created:
    - src/layer/tui_layer_entity.c
  modified:
    - include/cels_ncurses.h
    - include/cels_ncurses_draw.h
    - src/tui_internal.h

key-decisions:
  - "TUI_DrawContext_Component full struct in cels_ncurses_draw.h (depends on PANEL/WINDOW/DrawContext types), forward-declared in cels_ncurses.h"
  - "TUI_LayerConfig (not TUI_Layer) avoids collision with existing v1.0 typedef struct TUI_Layer"
  - "ctx.subcell_buf set to NULL in on_create to avoid dangling pointer after cels_entity_set_component copy (Pitfall 4)"
  - ".visible defaults to false (C99 zero-init) -- developer must pass .visible = true explicitly"

patterns-established:
  - "Struct completion: forward-declare CEL_Component in primary header, define struct body in draw header for types with ncurses dependencies"
  - "Layer entity registry: static array + swap-remove + dirty flag for deferred z-order rebuild"

# Metrics
duration: 6min
completed: 2026-03-14
---

# Phase 4 Plan 01: Layer Entity Types, Lifecycle, and Z-Order Summary

**Three layer CEL_Component types with TUILayer composition, lifecycle observers for panel management, and z-order sync system with sorted internal registry**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-14T17:16:58Z
- **Completed:** 2026-03-14T17:23:05Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Defined TUI_Renderable (tag), TUI_LayerConfig (config), TUI_DrawContext_Component (ncurses-attached) as CEL_Component types
- TUILayer composition + call macro provides natural entity-driven layer creation syntax
- CEL_Lifecycle(TUI_LayerLC) with on_create (creates WINDOW + PANEL + attaches DrawContext) and on_destroy (cleanup)
- Internal LayerEntry registry with insertion sort z-order synchronization and panel_hidden() visibility sync
- TUI_LayerSyncSystem at PreRender phase detects runtime z_order and visibility changes

## Task Commits

Each task was committed atomically:

1. **Task 1: Define layer component types and TUILayer composition in public headers** - `25d05a1` (feat)
2. **Task 2: Implement layer entity lifecycle, registry, z-order sync, and composition** - `d424c29` (feat)

## Files Created/Modified
- `include/cels_ncurses.h` - Added CEL_Component(TUI_Renderable), CEL_Component(TUI_LayerConfig), CEL_Component(TUI_DrawContext_Component) forward-declaration, CEL_Define(TUILayer) + call macro
- `include/cels_ncurses_draw.h` - Added struct TUI_DrawContext_Component definition with ctx, dirty, panel, win, subcell_buf fields
- `src/layer/tui_layer_entity.c` - Complete layer entity implementation: lifecycle, observers, registry, z-order sync, composition (328 lines)
- `src/tui_internal.h` - Added CEL_Define_Lifecycle(TUI_LayerLC), CEL_Define_System(TUI_LayerSyncSystem), ncurses_register_layer_systems forward declarations

## Decisions Made
- TUI_DrawContext_Component uses struct completion pattern: CEL_Component forward-declaration in cels_ncurses.h (no ncurses types needed), full struct body in cels_ncurses_draw.h (where PANEL/WINDOW/DrawContext types are available). This avoids making cels_ncurses.h depend on ncurses headers while allowing consumers who include both headers to use cel_watch with the full type.
- Named TUI_LayerConfig instead of TUI_Layer to avoid collision with the existing `typedef struct TUI_Layer` in cels_ncurses_draw.h (v1.0 imperative API still used by draw_test).
- ctx.subcell_buf set to NULL in on_create observer to avoid dangling pointer after cels_entity_set_component copies the struct (Pitfall 4 from RESEARCH.md). Sub-cell buffer allocation is lazy and handled by draw functions.
- Visibility defaults to false (C99 zero-initialization). Developer must pass `.visible = true` explicitly -- no sentinel-value workaround.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- All layer component types and the complete layer lifecycle implementation are in place
- Plan 02 (module registration + build integration) is unblocked: needs to register TUI_LayerLC in CEL_Module(NCurses) init body and add tui_layer_entity.c to CMakeLists.txt INTERFACE sources
- Old tui_layer.c API is untouched -- draw_test backward compatibility preserved

---
*Phase: 04-layer-entities*
*Completed: 2026-03-14*
