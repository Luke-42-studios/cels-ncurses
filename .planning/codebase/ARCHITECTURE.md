# Architecture

**Analysis Date:** 2026-02-07

## Pattern Overview

**Overall:** Provider Module Pattern (Backend implementation for the CELS declarative ECS framework)

**Key Characteristics:**
- INTERFACE CMake library: sources compile in the consumer's translation unit, not as a separate .so/.a
- Implements the CELS provider contract: `*_use()` functions register startup hooks, frame loops, and ECS systems
- Three independent providers (Window, Input, Renderer) bundled by a single Engine module
- Decouples terminal I/O (ncurses) from application logic via CELS's backend-agnostic `CELS_Input` and `CELS_Window` abstractions
- Feature/Provider system: rendering is declared via `CEL_DefineFeature` / `CEL_Provides` / `CEL_Feature`, not hardcoded

## Layers

**Engine Module (Facade):**
- Purpose: Single entry point that bundles all three providers into one `TUI_Engine_use()` call
- Location: `src/tui_engine.c`, `include/cels-ncurses/tui_engine.h`
- Contains: `CEL_DefineModule(TUI_Engine)` implementation, `TUI_EngineConfig` and `TUI_EngineContext` types
- Depends on: Window provider, Input provider, Renderer provider, CELS core (`cels_module_register`, `cels_state_register_ptr`)
- Used by: Consumer application's `CEL_Build()` block

**Window Provider:**
- Purpose: Owns the terminal lifecycle -- ncurses initialization (`initscr`/`endwin`), color pair setup, the main frame loop with FPS timing, and the `WindowState` state machine
- Location: `src/window/tui_window.c`, `include/cels-ncurses/tui_window.h`
- Contains: `TUI_Window_use()`, `tui_window_startup()`, `tui_window_frame_loop()`, `TUI_WindowState` global, signal handler
- Depends on: CELS core (`cels_register_startup`, `cels_register_frame_loop`, `cels_state_register`, `Engine_Progress`), ncurses
- Used by: Engine module, Input provider (for `g_running` pointer), Renderer (for `TUI_WindowState` fields)

**Input Provider:**
- Purpose: Reads ncurses `getch()` each frame and populates the backend-agnostic `CELS_Input` struct
- Location: `src/input/tui_input.c`, `include/cels-ncurses/tui_input.h`
- Contains: `TUI_Input_use()`, ECS system callback at `OnLoad` phase, key mapping switch
- Depends on: CELS core (`cels_get_context`, `cels_input_set`, `cels_get_world`), ncurses (`wgetch`), Window provider (`tui_window_get_running_ptr`)
- Used by: Engine module

**Renderer Provider:**
- Purpose: Implements the `Renderable` feature for `Canvas` entities using ncurses output. Queries the ECS world for `Canvas`, `Selectable`, `Text`, `ClickArea`, `Range`, and `GraphicsSettings` entities and renders the full screen
- Location: `src/renderer/tui_renderer.c`, `include/cels-ncurses/tui_renderer.h`
- Contains: `tui_renderer_init()`, `CEL_DefineFeature(Renderable)`, `CEL_Provides(TUI, Renderable, Canvas, ...)`, render callback
- Depends on: CELS core (Feature/Provider API, ECS queries), ncurses, consumer's `components.h` (resolved via consumer include paths), Component widgets layer
- Used by: Engine module

**Component Widgets (Header-Only):**
- Purpose: Static inline ncurses widget renderers -- canvas header, button, cycle slider, toggle slider, hint text, info box
- Location: `include/cels-ncurses/tui_components.h`
- Contains: `tui_render_canvas()`, `tui_render_button()`, `tui_render_slider_cycle()`, `tui_render_slider_toggle()`, `tui_render_hint()`, `tui_render_info_box()`, color pair constants
- Depends on: ncurses, `tui_renderer.h` (for `CP_*` color pair enum)
- Used by: Renderer provider callback (`tui_prov_render_screen`)

## Data Flow

**Frame Loop (main application loop):**

1. `main()` calls `cels_run_providers()` which calls registered `startup_fn` then `frame_loop_fn`
2. `tui_window_startup()` initializes ncurses (initscr, cbreak, keypad, nodelay, color pairs)
3. `tui_window_frame_loop()` fast-tracks `TUI_WindowState` to `WINDOW_STATE_READY` and enters `while (g_running)` loop
4. Each iteration: `Engine_Progress(delta)` advances the flecs ECS world through all phases:
   - **OnLoad**: `CELS_LifecycleSystem` evaluates lifecycle visibility, `TUI_InputSystem` reads `getch()` into `CELS_Input`
   - **PostLoad**: `CELS_RecompositionSystem` processes dirty compositions
   - **OnUpdate**: Application-defined systems (e.g., `MainMenuInputSystem`, `SettingsInputSystem`) handle input logic
   - **OnStore**: `TUI_Renderable` provider system calls `tui_prov_render_screen()` which queries ECS and renders via ncurses
5. After `Engine_Progress`, `doupdate()` flushes all ncurses window changes to terminal
6. `usleep(delta * 1000000)` throttles to target FPS

**Input Flow:**

1. `tui_read_input_ncurses()` calls `wgetch(stdscr)` (non-blocking)
2. Switch on key code maps to `CELS_Input` fields (axis_left, button_accept, etc.)
3. `cels_input_set(ctx, &input)` copies into global context
4. Application systems read via `cels_input_get(ctx)` -- completely backend-agnostic
5. Special case: 'Q' key directly sets `*g_tui_running_ptr = 0` to signal quit

**Render Flow:**

1. Provider system `tui_prov_render_screen()` fires at `OnStore` phase
2. Checks for state changes (screen, selection, settings) to skip redundant redraws
3. Iterates Canvas entities via `ecs_each_id(world, CanvasID)` for header
4. Iterates Selectable entities by index order, inspecting co-located components to determine widget type:
   - `Text + ClickArea` -> `tui_render_button()`
   - `Text + Range(RANGE_CYCLE)` -> `tui_render_slider_cycle()`
   - `Text + Range(RANGE_TOGGLE)` -> `tui_render_slider_toggle()`
5. Renders hint text and info box
6. Calls `wnoutrefresh(stdscr)` -- actual terminal flush happens in frame loop's `doupdate()`

**State Management:**
- `TUI_WindowState` is a module-global struct (extern'd in header) with `WindowState` enum, dimensions, title, version, FPS, delta_time
- Registered via `cels_state_register_ptr()` so compositions can observe it with `CEL_ObserveById(ctx.windowState, TUI_WindowState_t)`
- State changes trigger `cels_state_notify_change(TUI_WindowStateID)` which marks dependent compositions dirty for recomposition

## Key Abstractions

**Provider Contract:**
- Purpose: Defines how a backend module integrates with CELS
- Pattern: Each provider defines a `Type_use(Type config)` function that registers hooks with CELS core
- Window: `TUI_Window_use()` -> `cels_register_startup()` + `cels_register_frame_loop()`
- Input: `TUI_Input_use()` -> registers a flecs ECS system at `OnLoad` phase
- Renderer: `tui_renderer_init()` -> `CEL_Feature()` + `CEL_Provides()` + `CEL_ProviderConsumes()`

**WindowState State Machine (Vulkan-aligned):**
- Purpose: Enables window lifecycle observation across different backends (TUI fast-tracks, Vulkan uses full state chain)
- Examples: `include/cels-ncurses/tui_window.h` (enum), `src/window/tui_window.c` (transitions)
- Pattern: `NONE -> READY` (TUI fast-tracks), `READY -> CLOSING -> CLOSED` on quit. Vulkan backends would use `NONE -> CREATED -> SURFACE_READY -> READY -> RESIZING -> MINIMIZED -> CLOSING -> CLOSED`

**Feature/Provider Model:**
- Purpose: Decouples "what to render" (components) from "how to render" (backend)
- Examples: `src/renderer/tui_renderer.c` defines `CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnStore)`, declares `CEL_Feature(Canvas, Renderable)`, provides `CEL_Provides(TUI, Renderable, Canvas, tui_prov_render_screen)`
- Pattern: Framework auto-generates flecs systems from provider registrations at first `Engine_Progress`

**CEL_DefineModule:**
- Purpose: Idempotent module initialization that bundles multiple providers
- Examples: `src/tui_engine.c` uses `CEL_DefineModule(TUI_Engine)` macro
- Pattern: First call registers module entity, executes init body (registers all sub-providers, calls root composition). Subsequent calls are no-ops.

## Entry Points

**Consumer Entry (`CEL_Build`):**
- Location: Consumer's `app.c` (e.g., `/home/cachy/workspaces/libs/cels/examples/app.c`)
- Triggers: `main()` in `/home/cachy/workspaces/libs/cels/src/backends/main.c` calls `CELS_BuildInit()` which is generated by `CEL_Build(App)` macro
- Responsibilities: Call `TUI_Engine_use(config)` with title, version, fps, and root composition function pointer

**TUI_Engine_use (Module Facade):**
- Location: `src/tui_engine.c`
- Triggers: Called by consumer's `CEL_Build()` block
- Responsibilities: Store config, trigger `TUI_Engine_init()` which registers Window, Input, Renderer providers and calls root composition

**TUI_Window_use (Window Provider):**
- Location: `src/window/tui_window.c`
- Triggers: Called by `CEL_DefineModule(TUI_Engine)` body via `CEL_Use(TUI_Window, ...)`
- Responsibilities: Store config, ensure `TUI_WindowStateID`, register `tui_window_startup` and `tui_window_frame_loop` with CELS runtime

**tui_renderer_init (Renderer Provider):**
- Location: `src/renderer/tui_renderer.c`
- Triggers: Called by `CEL_DefineModule(TUI_Engine)` body
- Responsibilities: Register Feature/Provides declarations and ensure consumed component IDs

## Error Handling

**Strategy:** Minimal -- ncurses errors are not checked, ECS queries return NULL/0 on failure with null-guard checks

**Patterns:**
- Null pointer guards: `if (!world) return;`, `if (!gfx) return NULL;`
- Signal handling: `SIGINT` handler sets `g_running = 0` for clean shutdown
- `atexit(cleanup_endwin)` ensures terminal is restored even on abnormal exit
- `isendwin()` check prevents double-endwin calls

## Cross-Cutting Concerns

**Logging:** No logging within the module. CELS core provides `debug_output` mode for entity/state events (enabled via `CELS_AppConfig.debug_output`).

**Validation:** Minimal. Config defaults applied if zero/null (title defaults to "CELS App", FPS defaults to 60). No bounds checking on component widget output positions.

**Authentication:** Not applicable.

**Thread Safety:** Single-threaded. `g_running` is `volatile int` for signal handler safety. No mutexes or atomics.

**Rendering Optimization:** Dirty-checking in `tui_prov_render_screen` skips redraws when no state has changed, with periodic forced refresh every 30 frames.

---

*Architecture analysis: 2026-02-07*
