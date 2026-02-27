# Phase 1: Module Boundary - Research

**Researched:** 2026-02-27
**Domain:** CELS ECS module architecture, ncurses terminal lifecycle, refactoring API surface
**Confidence:** HIGH

## Summary

This phase replaces the existing Engine + CelsNcurses provider-registration facade with a single `CEL_Module(NCurses)` that exposes an entity-component API surface. Per CONTEXT.md, Phase 2 (Window Entity) is absorbed -- the module skeleton AND working window lifecycle are delivered together. The research covers: the existing codebase architecture, the CELS module/component/system macro API, the flecs observer pattern for reactive window initialization, and the specific mapping from old globals to new ECS components.

The existing codebase has well-structured implementation code (ncurses init/shutdown, input reading, frame pipeline, drawing) that should be preserved. The wrapping layer -- Engine_use(), provider-registration, backend descriptor hooks, global state structs -- is what gets replaced. The new `CEL_Module(NCurses)` registers components and systems; the developer creates entities with NCurses components; NCurses systems react through flecs observers and ECS queries.

A critical finding is that the old code uses the deprecated `cel_module(Engine)` lowercase macro. The new module MUST use `CEL_Module(NCurses)` (uppercase), which is the current CELS v0.5 API. The old `cels/backend.h` dependency (`CELS_BackendDesc`, `cels_backend_register`) is removed entirely -- the new module owns its own main loop hooks via CELS pipeline phases.

**Primary recommendation:** Refactor in three layers: (1) define new component types in public header, (2) rewrite the module init to register components and systems instead of calling provider-registration functions, (3) implement the window observer that reacts to NCurses_WindowConfig being added to an entity. Preserve all ncurses implementation internals.

## Standard Stack

### Core (unchanged dependencies)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses (wide) | System | Terminal I/O, window management | Required -- entire drawing API built on it |
| panel | System | Z-ordered layer compositing | Required -- layer system uses update_panels/doupdate |
| flecs | v4.1.4 | ECS engine (via CELS) | Required -- CELS framework is built on flecs |
| CELS | v0.5.0 | Declarative application framework | Required -- module/component/system macros |

### Internal (files being refactored)

| File | Current Purpose | Phase 1 Action |
|------|----------------|----------------|
| `tui_engine.h` / `tui_engine.c` | Engine module facade, CelsNcurses module | **Delete** -- replaced by new NCurses module |
| `backend.h` | Umbrella header re-exporting Engine + Clay | **Delete** -- replaced by `ncurses.h` |
| `tui_window.h` / `tui_window.c` | Window provider with backend hooks | **Rewrite header** (new component types), **refactor source** (observer-based init) |
| `tui_input.h` / `tui_input.c` | Input provider with ECS system | **Keep mostly**, remove provider-registration pattern |
| `tui_frame.h` / `tui_frame.c` | Frame pipeline with ECS systems | **Keep mostly**, register in module init instead |
| `tui_draw.h`, `tui_color.h`, etc. | Drawing primitives | **Keep unchanged** |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| flecs observer for window init | System-based polling each frame | Observer fires once on component add -- cleaner and more ECS-idiomatic. CONTEXT.md locks this decision. |
| Separate Window/Input modules | Single NCurses module | Single module is decided. Window and Input are components/systems within it, not sub-modules. |

## Architecture Patterns

### Recommended Header Structure

```
include/cels-ncurses/
  ncurses.h           # NEW umbrella header (replaces backend.h)
  tui_ncurses.h       # NEW public component types + module declaration
  tui_internal.h      # NEW internal implementation details
  tui_draw.h          # UNCHANGED drawing primitives
  tui_draw_context.h  # UNCHANGED draw context
  tui_types.h         # UNCHANGED coordinate types
  tui_color.h         # UNCHANGED color/style system
  tui_scissor.h       # UNCHANGED scissor stack
  tui_subcell.h       # UNCHANGED sub-cell rendering
```

### Recommended Source Structure

```
src/
  ncurses_module.c    # NEW module definition (CEL_Module(NCurses))
  window/
    tui_window.c      # REFACTORED: observer-based init, no backend hooks
  input/
    tui_input.c       # REFACTORED: registered by module, not provider
  frame/
    tui_frame.c       # REFACTORED: registered by module, not provider
  layer/
    tui_layer.c       # UNCHANGED (refactored later in Phase 4)
  graphics/
    tui_color.c       # UNCHANGED
    tui_draw.c        # UNCHANGED
    tui_scissor.c     # UNCHANGED
    tui_subcell.c     # UNCHANGED
```

### Pattern 1: CEL_Module Definition

**What:** The module macro generates an init function with idempotent guard, registers components, and sets up systems/observers.
**When to use:** Module entry point.
**Example:**
```c
// Source: cels/cels.h line 606 -- CEL_Module macro expansion
// Generates: NCurses entity, NCurses_init_body(), NCurses_init()

CEL_Module(NCurses) {
    // Register component types
    cels_ensure_component(&NCurses_WindowConfig_id, "NCurses_WindowConfig",
                          sizeof(NCurses_WindowConfig), CELS_ALIGNOF(NCurses_WindowConfig));
    cels_ensure_component(&NCurses_WindowState_id, "NCurses_WindowState",
                          sizeof(NCurses_WindowState), CELS_ALIGNOF(NCurses_WindowState));

    // Register observer: when NCurses_WindowConfig is added to any entity,
    // initialize the terminal
    ncurses_register_window_observer();

    // Register input system at OnLoad phase
    ncurses_register_input_system();

    // Register frame pipeline systems
    ncurses_register_frame_systems();
}
```

### Pattern 2: Flecs Observer for Reactive Component Initialization

**What:** A flecs observer watches for `EcsOnSet` events on a specific component type. When a developer adds `NCurses_WindowConfig` to an entity, the observer fires and initializes the ncurses terminal.
**When to use:** Single-shot initialization triggered by entity creation.
**Example:**
```c
// Source: flecs.h line 5088 -- ecs_observer_desc_t
// Source: cels world_manager.c line 149 -- observer registration pattern

static void on_window_config_set(ecs_iter_t* it) {
    // Enforce single window entity
    if (g_window_entity != 0) {
        fprintf(stderr, "[NCurses] Warning: only one window entity allowed, ignoring\n");
        return;
    }

    NCurses_WindowConfig* configs = ecs_field(it, NCurses_WindowConfig, 0);
    for (int i = 0; i < it->count; i++) {
        g_window_entity = it->entities[i];

        // Initialize ncurses terminal (reuse existing tui_hook_startup logic)
        ncurses_terminal_init(&configs[i]);

        // Attach NCurses_WindowState to the same entity
        ecs_world_t* world = it->world;
        NCurses_WindowState state = {
            .width = COLS,
            .height = LINES,
            .running = true,
            .actual_fps = configs[i].fps > 0 ? (float)configs[i].fps : 60.0f
        };
        ecs_set_id(world, it->entities[i], NCurses_WindowState_id,
                   sizeof(NCurses_WindowState), &state);
    }
}

static void ncurses_register_window_observer(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    ecs_observer_desc_t desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "NCurses_WindowConfigObserver";
    desc.entity = ecs_entity_init(world, &entity_desc);
    desc.query.terms[0].id = NCurses_WindowConfig_id;
    desc.events[0] = EcsOnSet;
    desc.callback = on_window_config_set;
    ecs_observer_init(world, &desc);
}
```

### Pattern 3: Component Registration in Public Header

**What:** Components are defined in the public header using `CEL_Component` for use with `cel_has()` and ECS queries. The `static cels_entity_t` generated by `CEL_Component` means each translation unit gets its own ID variable, but `cels_ensure_component` deduplicates by name.
**When to use:** All NCurses component types.
**Example:**
```c
// In tui_ncurses.h (public header)

CEL_Component(NCurses_WindowConfig) {
    const char* title;
    int fps;
    int color_mode;  // 0=auto, 1=256, 2=palette, 3=direct-RGB
};

CEL_Component(NCurses_WindowState) {
    int width;
    int height;
    bool running;
    float actual_fps;
    float delta_time;
};
```

### Pattern 4: System Registration in Module Init

**What:** ECS systems are registered by the module init body using the raw flecs API (same pattern as existing `tui_input.c` and `tui_frame.c`).
**When to use:** All NCurses systems (input, frame_begin, frame_end).
**Example:**
```c
// Source: tui_input.c line 356 -- system registration pattern

static void ncurses_register_input_system(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());
    ecs_system_desc_t sys_desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "NCurses_InputSystem";
    ecs_id_t phase_ids[3] = {
        ecs_pair(EcsDependsOn, EcsOnLoad),
        EcsOnLoad,
        0
    };
    entity_desc.add = phase_ids;
    sys_desc.entity = ecs_entity_init(world, &entity_desc);
    sys_desc.callback = ncurses_input_system_callback;
    ecs_system_init(world, &sys_desc);
}
```

### Pattern 5: Consumer Application Usage (Target API)

**What:** The developer's application after Phase 1 is complete.
**Example:**
```c
#include <cels/cels.h>
#include <cels-ncurses/ncurses.h>

// Root composition
CEL_Composition(World) {
    (void)props;

    // Create window entity with config component
    cel_entity(.name = "Window") {
        cel_has(NCurses_WindowConfig,
            .title = "My App",
            .fps = 60,
            .color_mode = 0  // auto
        );
    }
}

cels_main() {
    cels_scope {
        cels_world->title = "My App";
    }

    // Load NCurses module (registers components, systems, observers)
    NCurses_init();

    cels_register(World);

    cels_session(World) {
        while (cels_running()) {
            cels_step(0);
        }
    }
}
```

### Anti-Patterns to Avoid

- **Keeping global state alongside ECS components:** The old code has `Engine_WindowState` global, `g_input_state` global, `g_frame_state` global. These must either become ECS component data on entities OR remain as implementation-internal statics that back the component data. Do NOT expose both a global and a component.
- **Registering systems at OnLoad from provider_use():** The old pattern calls `TUI_Input_use()` which registers systems. In the new pattern, the MODULE registers all systems at init time. Systems should not be registered lazily from user-facing API calls.
- **Including `<flecs.h>` in public headers:** The public header should only include `<cels/cels.h>` (for `CEL_Component` macro and `cels_entity_t` type). Flecs internals stay in `.c` files. Exception: `tui_layer.h` and `tui_draw_context.h` include `<ncurses.h>` which is fine since ncurses is a hard dependency.
- **Using deprecated `cel_module()` lowercase macro:** The old code uses `cel_module(Engine)`. The new code MUST use `CEL_Module(NCurses)` (uppercase).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Module idempotent init | Custom init guard | `CEL_Module(NCurses)` macro | Macro generates the `if (NCurses != 0) return;` guard automatically |
| Component deduplication across TUs | Manual ID tracking | `cels_ensure_component()` | Deduplicates by name, handles INTERFACE library pattern |
| Observer for component-add events | Polling system that checks entity existence each frame | `ecs_observer_init()` with `EcsOnSet` event | Fires exactly once when component is set, zero per-frame cost |
| System phase ordering | Manual function call ordering | `ecs_pair(EcsDependsOn, phase)` in entity_desc.add | Flecs pipeline handles ordering automatically |
| ncurses terminal init/shutdown | -- | Reuse existing `tui_hook_startup()`/`tui_hook_shutdown()` logic | Battle-tested code handles signal handlers, atexit, locale, color init |
| Main loop scheduling | Custom frame loop | `cels_step()` / `cels_engine_progress()` + pipeline phases | CELS owns the main loop; systems execute in pipeline phase order |

**Key insight:** The existing implementation code (ncurses calls, input parsing, frame timing, color management) is correct and battle-tested. Only the wrapping layer changes: how that code gets invoked (observer/system callbacks instead of provider-registration calls).

## Common Pitfalls

### Pitfall 1: INTERFACE Library and Static Variables

**What goes wrong:** `cels-ncurses` is a CMake INTERFACE library -- sources compile in the consumer's context. `static` variables in headers get duplicated per TU.
**Why it happens:** `CEL_Component()` macro generates `static cels_entity_t Name_id = 0`. Each TU that includes the header gets its own copy.
**How to avoid:** `cels_ensure_component()` deduplicates by component name string. The per-TU static gets lazily populated on first use. This is the intended CELS pattern -- no action needed, but be aware of it.
**Warning signs:** If you see component IDs that are 0 when they should be initialized, a TU may not have called `cels_ensure_component` yet.

### Pitfall 2: Observer Fires Before Module Fully Initialized

**What goes wrong:** If the developer creates a window entity before `NCurses_init()` completes, the observer fires mid-init.
**Why it happens:** Flecs observers fire synchronously when a component is set. If the module init body itself creates entities, the observer fires in the middle of module setup.
**How to avoid:** The module init body should NOT create entities. It only registers components, observers, and systems. The developer creates entities AFTER module init returns. This aligns with the CELS pattern: `cels_register(World)` happens after `NCurses_init()`.
**Warning signs:** Crash or incomplete state during observer callback.

### Pitfall 3: Single-Window Enforcement Race

**What goes wrong:** Two entities with NCurses_WindowConfig could be created in the same frame.
**Why it happens:** If the developer creates multiple window entities before the pipeline runs.
**How to avoid:** The observer callback checks `g_window_entity != 0` and logs a warning + ignores subsequent adds. Per CONTEXT.md: "Single window entity enforced -- second NCurses_WindowConfig entity logs a warning and is ignored."
**Warning signs:** Warning message about duplicate window entity.

### Pitfall 4: Backend Descriptor Removal Breaks Existing Consumers

**What goes wrong:** The old `cels_backend_register(&tui_backend_desc)` hooks into CELS's main loop. Removing it without providing equivalent functionality in the new module breaks the frame loop.
**Why it happens:** The backend hooks (startup, shutdown, frame_begin, frame_end, should_quit, get_delta_time) drive the application lifecycle.
**How to avoid:** Map each backend hook to an equivalent mechanism in the new module:
  - `startup` -> observer on NCurses_WindowConfig (EcsOnSet)
  - `shutdown` -> observer on NCurses_WindowConfig (EcsOnRemove) or atexit
  - `frame_begin` -> system at EcsPreStore (already exists)
  - `frame_end` -> system at EcsPostFrame (already exists)
  - `should_quit` -> NCurses_WindowState.running becomes false, checked via `cels_should_quit()`
  - `get_delta_time` -> NCurses_WindowState.delta_time (or g_frame_state.delta_time)
**Warning signs:** Application starts but never renders, or never exits cleanly.

### Pitfall 5: draw_test Example Has No CELS Runtime

**What goes wrong:** The `draw_test` example compiles WITHOUT the CELS framework (standalone ncurses). It directly calls ncurses init, frame pipeline, and draw functions.
**Why it happens:** `draw_test` was designed as a standalone visual test. Its CMake target links only ncurses, not cels.
**How to avoid:** The rebuilt `draw_test` should either: (a) use the new CELS-based API (`NCurses_init()` + entity creation), or (b) remain standalone but test only the drawing primitives (which are unchanged). Per CONTEXT.md: "Delete old examples. Rebuild draw_test progressively -- each phase rebuilds the portion it enables (Phase 1: module loads + window opens)."
**Warning signs:** draw_test fails to compile because it references old APIs.

### Pitfall 6: CEL_Window and CELS_WindowState Dependencies

**What goes wrong:** The old `tui_window.c` references `CEL_Window`, `CELS_WindowState`, `CEL_Window_register()`, `cels_window_set()`, and `cels_backend_register()` from `<cels/backend.h>`. These may have been removed or changed in CELS v0.5.
**Why it happens:** STATE.md notes: "tui_window.h references CELS_WindowState type removed from cels core -- will be resolved in Phase 1."
**How to avoid:** The new module does NOT use `CEL_Window`, `CELS_WindowState`, or `cels_backend_register`. It defines its own `NCurses_WindowConfig` and `NCurses_WindowState` components. All references to the old types are removed.
**Warning signs:** Compile errors referencing `CELS_WindowState`, `CEL_Window`, or `cels_backend_register`.

## Code Examples

### Example 1: New Umbrella Header (ncurses.h)

```c
// include/cels-ncurses/ncurses.h
// Single include for NCurses module consumers

#ifndef CELS_NCURSES_H
#define CELS_NCURSES_H

// Public component types and module declaration
#include <cels-ncurses/tui_ncurses.h>

// Drawing API (unchanged)
#include <cels-ncurses/tui_types.h>
#include <cels-ncurses/tui_color.h>
#include <cels-ncurses/tui_draw_context.h>
#include <cels-ncurses/tui_draw.h>
#include <cels-ncurses/tui_scissor.h>
#include <cels-ncurses/tui_subcell.h>

// Layer and frame API (will be entity-ified in later phases, but headers exist)
#include <cels-ncurses/tui_layer.h>
#include <cels-ncurses/tui_frame.h>

#endif // CELS_NCURSES_H
```

### Example 2: Public Component Types (tui_ncurses.h)

```c
// include/cels-ncurses/tui_ncurses.h

#ifndef CELS_NCURSES_TUI_NCURSES_H
#define CELS_NCURSES_TUI_NCURSES_H

#include <cels/cels.h>
#include <stdbool.h>

// --- Module declaration ---
extern cels_entity_t NCurses;
extern void NCurses_init(void);

// --- Component types ---

// Developer sets this on an entity to configure the terminal window
CEL_Component(NCurses_WindowConfig) {
    const char* title;
    int fps;
    int color_mode;  // 0=auto, 1=256-color, 2=palette-redef, 3=direct-RGB
};

// NCurses system fills this on the same entity after terminal init
CEL_Component(NCurses_WindowState) {
    int width;
    int height;
    bool running;
    float actual_fps;
    float delta_time;
};

// Input state component (on a separate entity from window)
CEL_Component(NCurses_InputState) {
    float axis_left[2];
    bool button_accept;
    bool button_cancel;
    bool key_tab;
    bool key_shift_tab;
    // ... (full field list from existing TUI_InputState)
    int raw_key;
    bool has_raw_key;
    int mouse_x;
    int mouse_y;
    // ... mouse fields
};

// Layer component (on child entities of the window entity, Phase 4)
// Forward declared here, implemented in Phase 4
CEL_Component(NCurses_Layer);

#endif // CELS_NCURSES_TUI_NCURSES_H
```

### Example 3: Module Implementation Skeleton (ncurses_module.c)

```c
// src/ncurses_module.c

#include <cels-ncurses/tui_ncurses.h>
#include <cels-ncurses/tui_internal.h>
#include <flecs.h>

// Internal functions (defined in tui_window.c, tui_input.c, tui_frame.c)
extern void ncurses_register_window_observer(void);
extern void ncurses_register_window_remove_observer(void);
extern void ncurses_register_input_system(void);
extern void ncurses_register_frame_systems(void);

CEL_Module(NCurses) {
    // Register all component types
    cels_ensure_component(&NCurses_WindowConfig_id, "NCurses_WindowConfig",
                          sizeof(NCurses_WindowConfig), CELS_ALIGNOF(NCurses_WindowConfig));
    cels_ensure_component(&NCurses_WindowState_id, "NCurses_WindowState",
                          sizeof(NCurses_WindowState), CELS_ALIGNOF(NCurses_WindowState));
    cels_ensure_component(&NCurses_InputState_id, "NCurses_InputState",
                          sizeof(NCurses_InputState), CELS_ALIGNOF(NCurses_InputState));

    // Register observers (reactive initialization)
    ncurses_register_window_observer();       // EcsOnSet NCurses_WindowConfig -> init terminal
    ncurses_register_window_remove_observer(); // EcsOnRemove -> clean shutdown

    // Register systems (frame pipeline)
    ncurses_register_input_system();           // OnLoad: read getch() each frame
    ncurses_register_frame_systems();          // PreStore: frame_begin, PostFrame: frame_end
}
```

### Example 4: Mapping Old to New

```
OLD (Engine_use pattern)                    NEW (entity-component pattern)
---------------------------------------    ----------------------------------------
Engine_use((Engine_Config){                 cel_entity(.name = "Window") {
    .title = "App",                             cel_has(NCurses_WindowConfig,
    .fps = 60,                                      .title = "App",
    .color_mode = 0,                                .fps = 60,
    .root = AppUI                                   .color_mode = 0
});                                             );
                                            }

Engine_WindowState.width                    // Query NCurses_WindowState from window entity
                                            const NCurses_WindowState* state =
                                                cel_watch(window_entity, NCurses_WindowState);
                                            state->width

g_input_state                               // Query NCurses_InputState from input entity
                                            const NCurses_InputState* input = ...;

TUI_Engine_use(config)                      NCurses_init();  // load module
                                            // then create entity with NCurses_WindowConfig
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `cel_module(Name)` lowercase | `CEL_Module(Name)` uppercase | CELS v0.4 (Phase 26.7 DSL restructure) | Module definition macro changed; old lowercase deleted |
| `Engine_use(config)` monolithic | Entity with components | v0.2.0 (this phase) | Configuration is data on entities, not function calls |
| `CELS_BackendDesc` hook system | Module registers pipeline systems | v0.2.0 (this phase) | No more backend abstraction layer; module owns its systems |
| `CEL_Build(App)` entry point | `cels_main()` entry point | CELS v0.4 | `CEL_Build` still works but `cels_main` is the new standard |
| Global `Engine_WindowState` | `NCurses_WindowState` on entity | v0.2.0 (this phase) | State is queryable via ECS, not via global pointer |

**Deprecated/outdated:**
- `cel_module()` lowercase: Removed in CELS Phase 26.7. Use `CEL_Module()`.
- `cels/backend.h`: Backend descriptor system. Being replaced by module-owned systems.
- `Engine_use()` / `TUI_Engine_use()`: Provider-registration facade. Replaced by entity-component pattern.
- `CEL_Build(Name)`: Still works via compatibility shims, but `cels_main()` is the modern entry point.

## Open Questions

1. **How does `cels_should_quit()` integrate with NCurses_WindowState.running?**
   - What we know: The old code calls `cels_request_quit()` or sets `g_running = 0`. The backend hook `should_quit` checks `!g_running`. CELS's main loop calls `cels_should_quit()`.
   - What's unclear: Does the NCurses module need to call `cels_request_quit()` when the input system detects 'q'? Or does the consumer check `NCurses_WindowState.running`?
   - Recommendation: The input system should call `cels_request_quit()` when 'q' is pressed (same as today's `*g_tui_running_ptr = 0`). Additionally, set `NCurses_WindowState.running = false` so consumer code can also query it. This keeps `cels_running()` loop working naturally.

2. **INTERFACE library + CEL_Module: will the module entity be duplicated across TUs?**
   - What we know: `CEL_Module(NCurses)` expands to `cels_entity_t NCurses = 0;` at file scope. For an INTERFACE library, this definition appears in every TU that includes the module source.
   - What's unclear: The existing `cel_module(Engine)` macro worked as INTERFACE. The new `CEL_Module()` has the same expansion pattern (non-static global). The linker may complain about multiple definitions.
   - Recommendation: The module source file (`ncurses_module.c`) should be the ONLY file that includes the `CEL_Module(NCurses)` expansion. The public header should use `extern` declarations only (`extern cels_entity_t NCurses; extern void NCurses_init(void);`). This matches the existing pattern in `tui_engine.h` lines 89-90.

3. **What happens to the `cels-widgets` dependency in CMakeLists.txt?**
   - What we know: Current `CMakeLists.txt` line 59 links `cels-widgets`. The CONTEXT.md says to drop Clay/Renderer integration, but doesn't mention widgets.
   - What's unclear: Is `cels-widgets` still needed? The old `minimal.c` uses it, but v0.2.0 examples won't.
   - Recommendation: Remove the `cels-widgets` link dependency from the INTERFACE library. If consumers need widgets, they link it themselves. The NCurses module should have minimal dependencies: `cels`, `ncurses`, `panel`, `m`.

## Sources

### Primary (HIGH confidence)
- `/home/cachy/workspaces/libs/cels/include/cels/cels.h` -- CEL_Module macro (line 606), CEL_Component macro (line 445), cel_has macro (line 897), cels_ensure_component (line 899), cels_main (line 727), cels_session (line 782)
- `/home/cachy/workspaces/libs/cels/include/cels/private/cels_runtime.h` -- cels_module_register, cels_component_register, cels_ensure_component declarations
- `/home/cachy/workspaces/libs/cels/include/cels/private/cels_system_impl.h` -- CEL_System macro, cel_query, cel_each, cel_run, phase aliases
- `/home/cachy/workspaces/libs/cels/build-test/_deps/flecs-src/distr/flecs.h` -- ecs_observer_desc_t (line 5088), EcsOnSet/EcsOnAdd/EcsOnRemove events
- `/home/cachy/workspaces/libs/cels/src/core/world_manager.c` -- Observer registration pattern (line 149)
- `/home/cachy/workspaces/libs/cels-ncurses/src/tui_engine.c` -- Current module implementation, cel_module(Engine), cel_module(CelsNcurses)
- `/home/cachy/workspaces/libs/cels-ncurses/src/window/tui_window.c` -- Backend hooks, terminal init/shutdown logic
- `/home/cachy/workspaces/libs/cels-ncurses/src/input/tui_input.c` -- ECS system registration pattern (line 356), input reading
- `/home/cachy/workspaces/libs/cels-ncurses/src/frame/tui_frame.c` -- Frame pipeline systems (line 231)

### Secondary (MEDIUM confidence)
- `/home/cachy/workspaces/libs/cels-ncurses/.planning/phases/01-module-boundary/01-CONTEXT.md` -- User decisions constraining the implementation
- `/home/cachy/workspaces/libs/cels-ncurses/.planning/ROADMAP.md` -- Phase descriptions and success criteria
- `/home/cachy/workspaces/libs/cels-ncurses/.planning/REQUIREMENTS.md` -- MOD-01, MOD-02, MOD-03, WIN-01, WIN-02, WIN-03

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- directly verified from source code and CMakeLists.txt
- Architecture: HIGH -- patterns extracted from existing working code, CELS macro expansions verified
- Pitfalls: HIGH -- identified from actual code analysis, not hypothetical
- Observer pattern: HIGH -- flecs observer_desc_t and existing CELS observer usage verified in source
- Open questions: MEDIUM -- require testing to fully resolve, but recommendations are sound

**Research date:** 2026-02-27
**Valid until:** Indefinite (all sources are local codebase, not external dependencies)
