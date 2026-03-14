# Phase 1: Module Boundary - Context

**Gathered:** 2026-02-27
**Updated:** 2026-02-27 (API review with developer feedback)
**Status:** Ready for re-planning

<domain>
## Phase Boundary

Replace the Engine + CelsNcurses provider-registration facade with a single `CEL_Module(NCurses)` that exposes an entity-component API surface. This phase absorbs Phase 2 (Window Entity) — the module skeleton AND a working window lifecycle are delivered together. Developer creates entities with NCurses components to configure behavior; NCurses systems react through ECS queries and observers.

**Absorbed from Phase 2:** Window entity with NCurses_WindowConfig/NCurses_WindowState driven by an observer system.

</domain>

<decisions>
## Implementation Decisions

### Module loading (REVISED)
- `cels_register(NCurses)` is the developer-facing call -- standard CELS registration, same as components and systems
- Phase 0 (CELS repo) adds `Name_register()` to `CEL_Module` macro so this works
- NOT `NCurses_init()` directly -- that's an implementation detail, not the developer API
- NOT `cel_module_load(NCurses)` in main -- `cels_register` is the uniform verb

### Public compositions via CEL_Define (NEW)
- NCurses module provides `CEL_Define(NCursesWindow, ...)` in its public header
- Call macro: `#define NCursesWindow(...) cel_init(NCursesWindow, __VA_ARGS__)`
- Developer writes: `NCursesWindow(.title = "My App", .fps = 30) {}` -- natural syntax
- The composition body uses `cel_has(NCurses_WindowConfig, ...)` internally
- Developer never touches `cel_has(NCurses_WindowConfig, ...)` directly
- Implementation in a .c file via `CEL_Compose(NCursesWindow) { ... }`

### Component reading via cel_watch (NEW)
- Developer reads NCurses_WindowState via `cel_watch(entity, NCurses_WindowState)` in compositions
- Reactive: triggers recomposition when dimensions change (SIGWINCH resize)
- This is key for future cels-clay integration -- UI recomposes on terminal resize

### No (void)props in compositions (CLARIFIED)
- CEL_Composition generates `_CEL_UNUSED Name##_props cel` -- no `(void)props` needed
- Composition fields accessed via `cel.field_name`

### No cel_entity for creating entities (CLARIFIED)
- `cel_entity(.name = "X")` exists but is for stable string IDs, not the primary creation pattern
- Primary pattern: `cel_init(CompositionName, .field = value) {}` or call macros
- Module-provided compositions use the call macro pattern (CEL_Define + #define)

### Public header organization
- Single umbrella header: `#include <cels-ncurses/ncurses.h>` gives the developer everything
- Developer includes `<cels/cels.h>` separately — NCurses header does NOT re-export CELS macros
- Internal files consolidate into 3 headers: `tui_ncurses.h` (public component types + module + compositions), `tui_internal.h` (implementation details), `tui_draw.h` (drawing API unchanged)
- Component structs are fully defined in the public header so developers can use `cel_has()` with struct literals
- CEL_Define(NCursesWindow, ...) and call macro go in tui_ncurses.h

### Migration approach
- Refactor in place — keep implementation internals (ncurses calls, input parsing, layer management, frame pipeline logic), rewrap the API surface
- Old `Engine_use()` / provider-registration pattern → new `CEL_Module(NCurses)` with systems
- Old global state (`g_input_state`, `Engine_WindowState`, etc.) → ECS components on entities
- Drop Clay/Renderer integration entirely — v0.2.0 is pure NCurses module only. Clay integration becomes a separate cels-clay module later
- Delete old examples. Rebuild draw_test progressively — each phase rebuilds the portion it enables (Phase 1: module loads + window opens)

### Module initialization
- Standard CELS patterns: `cels_register(NCurses)` in main, `cel_module_requires(NCurses)` in other modules
- No special initialization dance — follow CELS conventions

### Module skeleton depth
- Phase 1 delivers a working module + working window lifecycle (not just stubs)
- Window system uses observer pattern — watches for NCurses_WindowConfig being added to an entity
- Single window entity enforced — second NCurses_WindowConfig entity logs a warning and is ignored

### Component naming & grouping
- NCurses_ prefix for all component types (matches module name): NCurses_WindowConfig, NCurses_WindowState, NCurses_InputState, NCurses_Layer, etc.
- Window: NCurses_WindowConfig (developer sets: title, fps, color_mode) + NCurses_WindowState (NCurses fills: width, height, running, actual_fps)
- Input: separate entity from window — sets the modularity standard for other backends (SDL, etc.)
- Layers: child entities of the window entity — destruction cascades (destroy window → all layers destroyed)

### Claude's Discretion
- Internal file organization within src/
- Observer implementation details (CELS hooks vs. system-based detection)
- Exact component registration order in module init
- Error message wording for single-window enforcement
- How to handle the progressive draw_test rebuild structure

</decisions>

<specifics>
## Specific Ideas

- "We need to look at how to make NCurses the first CELS module" — this should be the reference implementation for how CELS modules are built
- "We have a lot of good code — refactor, don't rewrite" — preserve battle-tested implementation, change the wrapping
- "Input is separate to show modularity" — even though terminal has one input source, separate entity sets the pattern for SDL and other backends
- "The draw example has amazing coverage — rebuild it progressively" — draw_test is the validation vehicle, rebuilt piece by piece as each phase lands
- Draw functions (tui_draw_rect, tui_draw_text, etc.) stay as function API for now, but future ECS conversion is expected in a later milestone

</specifics>

<deferred>
## Deferred Ideas

- Clay/Renderer integration → separate cels-clay module (future milestone)
- Draw API ECS conversion → future milestone after v0.2.0
- Roadmap update needed: merge Phase 1 & Phase 2, renumber subsequent phases (5 phases instead of 6)

</deferred>

---

*Phase: 01-module-boundary*
*Context gathered: 2026-02-27*
*Updated: 2026-02-27 after API review (cels_register, CEL_Define, cel_watch)*
