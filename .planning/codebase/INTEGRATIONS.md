# External Integrations

**Analysis Date:** 2026-02-07

## CELS Framework Integration

The cels-ncurses module is a **provider module** for the CELS declarative ECS framework. It implements the ncurses backend for three provider roles: Window, Input, and Renderer.

**Framework Location:** `/home/cachy/workspaces/libs/cels/`
**API Header:** `/home/cachy/workspaces/libs/cels/include/cels/cels.h`
**Runtime Implementation:** `/home/cachy/workspaces/libs/cels/src/cels.cpp`

### Provider Registration Pattern

The module uses CELS's provider lifecycle hooks to integrate with the framework's execution loop.

**Window Provider** (`src/window/tui_window.c`):
```c
TUI_WindowState_t* TUI_Window_use(TUI_Window config) {
    g_tui_config = config;
    TUI_WindowState_ensure();
    cels_register_startup(tui_window_startup);      // Called before frame loop
    cels_register_frame_loop(tui_window_frame_loop); // Owns the main loop
    return &TUI_WindowState;
}
```

**Input Provider** (`src/input/tui_input.c`):
```c
void TUI_Input_use(TUI_Input config) {
    g_tui_running_ptr = tui_window_get_running_ptr();
    // Registers ECS system at OnLoad phase
    ecs_system_desc_t sys_desc = {0};
    // ... registers TUI_InputSystem via flecs API
}
```

**Render Provider** (`src/renderer/tui_renderer.c`):
```c
void tui_renderer_init(void) {
    CEL_Feature(Canvas, Renderable);
    CEL_Provides(TUI, Renderable, Canvas, tui_prov_render_screen);
    CEL_ProviderConsumes(Text, ClickArea, Selectable, Range, GraphicsSettings);
}
```

### CELS APIs Consumed

**Runtime Functions (from `cels/cels.h`):**
| Function | Used In | Purpose |
|----------|---------|---------|
| `cels_get_context()` | `tui_input.c`, `tui_renderer.c` | Get global CELS context |
| `cels_get_world()` | `tui_input.c`, `tui_renderer.c` | Get raw flecs world pointer |
| `cels_input_get()` | `tui_renderer.c` | Read current frame input |
| `cels_input_set()` | `tui_input.c` | Write input state each frame |
| `cels_register_startup()` | `tui_window.c` | Register terminal init callback |
| `cels_register_frame_loop()` | `tui_window.c` | Register main frame loop |
| `cels_state_register()` | `tui_window.c` | Register observable state entity |
| `cels_state_register_ptr()` | `tui_engine.c` | Register pointer-based state for CEL_ObserveById |
| `cels_state_notify_change()` | `tui_window.c`, `tui_engine.c` | Trigger recomposition on state change |
| `cels_module_register()` | `tui_engine.c` (via CEL_DefineModule) | Register idempotent module |

**CELS Macros Used:**
| Macro | Used In | Purpose |
|-------|---------|---------|
| `CEL_DefineModule()` | `tui_engine.c` | Define TUI_Engine module with init body |
| `CEL_Use()` | `tui_engine.c` | Register Window and Input providers |
| `CEL_DefineFeature()` | `tui_renderer.c` | Define "Renderable" feature |
| `CEL_Feature()` | `tui_renderer.c` | Declare Canvas supports Renderable |
| `CEL_Provides()` | `tui_renderer.c` | Register TUI as Renderable provider for Canvas |
| `CEL_ProviderConsumes()` | `tui_renderer.c` | Ensure component IDs are registered |

### CELS State Types Used

**CELS_Input** (from `cels/cels.h`):
- Populated by `tui_input.c` each frame via `cels_input_set()`
- Read by `tui_renderer.c` via `cels_input_get()` for dirty detection
- Fields used: `axis_left[2]`, `button_accept`, `button_cancel`, `key_tab`, `key_shift_tab`, `key_home`, `key_end`, `key_page_up`, `key_page_down`, `key_backspace`, `key_delete`, `key_number`, `key_function`, `raw_key`

**TUI_WindowState_t** (defined in `include/cels-ncurses/tui_window.h`):
- Observable state published by window provider
- Consumed by root compositions via `CEL_ObserveById(ctx.windowState, TUI_WindowState_t)`
- Fields: `state` (WindowState enum), `width`, `height`, `title`, `version`, `target_fps`, `delta_time`

### Module Architecture (TUI_Engine)

`TUI_Engine` (`src/tui_engine.c`) bundles all three providers into a single entry point:

```c
CEL_DefineModule(TUI_Engine) {
    CEL_Use(TUI_Window, .title = ..., .fps = ...);
    CEL_Use(TUI_Input);
    tui_renderer_init();
    // Calls root composition with TUI_EngineContext
}
```

**Consumer usage** (`examples/app.c`):
```c
CEL_Build(App) {
    TUI_Engine_use((TUI_EngineConfig){
        .title = "CELS Demo",
        .version = "0.9.6",
        .fps = 60,
        .root = AppUI
    });
}
```

## ncurses Library Integration

**Library:** ncursesw (wide-character variant)
**System Package:** Found via CMake `find_package(Curses REQUIRED)` with `CURSES_NEED_WIDE TRUE`
**Header:** `<ncurses.h>`

### Initialization Sequence (`src/window/tui_window.c` - `tui_window_startup()`)

```c
initscr();           // Enter ncurses mode
cbreak();            // Char-at-a-time input (no line buffering)
noecho();            // Don't echo typed characters
keypad(stdscr, TRUE); // Enable KEY_UP, KEY_F1, etc.
curs_set(0);         // Hide cursor
nodelay(stdscr, TRUE); // Non-blocking getch()
ESCDELAY = 25;       // Fast escape key response (default 1000ms is too slow)
```

### Color System (`src/window/tui_window.c` + `include/cels-ncurses/tui_renderer.h`)

Six color pairs defined as an enum in `tui_renderer.h`, initialized in `tui_window_startup()`:

| Constant | Pair | Foreground | Purpose |
|----------|------|------------|---------|
| `CP_SELECTED` | 1 | Yellow | Selected/highlighted items |
| `CP_HEADER` | 2 | Cyan | Headers and titles |
| `CP_ON` | 3 | Green | ON toggle state |
| `CP_OFF` | 4 | Red | OFF toggle state |
| `CP_NORMAL` | 5 | White | Normal bright text |
| `CP_DIM` | 6 | Default | Dim text (use with `A_DIM`) |

Background is always default terminal background (`-1`) via `use_default_colors()` and `assume_default_colors(-1, -1)`.

### Rendering Pipeline

1. **Input** (OnLoad phase): `tui_input.c` calls `wgetch(stdscr)` once per frame, maps to CELS_Input
2. **App Systems** (OnUpdate phase): Application systems modify ECS entities
3. **Render** (OnStore phase): `tui_renderer.c` queries entities, uses `tui_components.h` inline renderers
4. **Flush** (frame loop): `doupdate()` called in `tui_window_frame_loop()` after `Engine_Progress()`

Rendering uses the double-buffer pattern:
- Widget renderers call `mvprintw()` (writes to stdscr virtual buffer)
- `wnoutrefresh(stdscr)` marks stdscr for refresh (called at end of render pass)
- `doupdate()` flushes all changes to terminal in one operation (called in frame loop)

### Cleanup

```c
// Registered via atexit() for crash safety
static void cleanup_endwin(void) {
    if (g_ncurses_active && !isendwin()) {
        endwin();
    }
}
// Also: signal(SIGINT, tui_sigint_handler) sets g_running = 0
```

## flecs ECS Integration

**Library:** flecs v4.1.4 (fetched by parent project via CMake FetchContent)
**Header:** `<flecs.h>`
**Direct Usage:** `src/input/tui_input.c` and `src/renderer/tui_renderer.c`

### Input System Registration (`src/input/tui_input.c`)

The input provider registers a standalone ECS system (no query terms) at OnLoad phase:

```c
ecs_world_t* world = cels_get_world(cels_get_context());
ecs_system_desc_t sys_desc = {0};
ecs_entity_desc_t entity_desc = {0};
entity_desc.name = "TUI_InputSystem";
ecs_id_t phase_ids[3] = {
    ecs_pair(EcsDependsOn, EcsOnLoad), EcsOnLoad, 0
};
entity_desc.add = phase_ids;
sys_desc.entity = ecs_entity_init(world, &entity_desc);
sys_desc.callback = tui_input_system_callback;
ecs_system_init(world, &sys_desc);
```

### Render Entity Queries (`src/renderer/tui_renderer.c`)

The render callback queries flecs entities directly for component data:

```c
// Query all Canvas entities
ecs_iter_t canvas_it = ecs_each_id(world, CanvasID);
while (ecs_each_next(&canvas_it)) {
    Canvas* canvases = ecs_field(&canvas_it, Canvas, 0);
    // ...
}

// Check if entity has specific components
if (ecs_has_id(world, e, TextID) && ecs_has_id(world, e, ClickAreaID)) {
    const Text* text = ecs_get_id(world, e, TextID);
    // ...
}
```

**Component IDs referenced:** `CanvasID`, `SelectableID`, `TextID`, `ClickAreaID`, `RangeID`, `GraphicsSettingsID` - all defined in consumer's `components.h` via `CEL_Component()` macros, which generate static `NameID` variables with lazy registration.

## Consumer Application Integration

The module is consumed as a git submodule of the CELS framework:

**Git Submodule Config** (`/home/cachy/workspaces/libs/cels/.gitmodules`):
```
[submodule "modules/cels-ncurses"]
    path = modules/cels-ncurses
    url = https://github.com/Luke-42-studios/cels-ncurses.git
```

**CMake Integration** (`/home/cachy/workspaces/libs/cels/CMakeLists.txt`):
```cmake
add_subdirectory(modules/cels-ncurses)

add_executable(app examples/app.c)
target_link_libraries(app PRIVATE cels cels-ncurses flecs::flecs_static)
```

### Consumer Must Provide

1. **`components.h`** - Shared component definitions included by `src/renderer/tui_renderer.c` via `#include "components.h"` (resolved through consumer's include paths)
2. **`CEL_Build()` block** - Application entry point that calls `TUI_Engine_use()` or individual provider `CEL_Use()` calls
3. **Root composition function** - Passed as `.root` in `TUI_EngineConfig`, receives `TUI_EngineContext`

## Data Storage

**Databases:** None
**File Storage:** None (pure in-memory TUI)
**Caching:** None

## Authentication & Identity

Not applicable - Terminal UI module with no network or auth requirements.

## Monitoring & Observability

**Debug Infrastructure (from CELS framework):**
- flecs REST explorer at `http://localhost:27750` when `CELS_DEBUG=ON`
- `cels-debug` inspector tool (separate git submodule at `tools/cels-debug/`)
- Entity add/remove observers print `[ENTITY+]`/`[ENTITY-]` when `debug_output = true`
- State change logging: `[STATE]`, `[STATE NOTIFY]` debug prints

**Error Tracking:** None - Uses `fprintf(stderr, ...)` for warnings
**Logs:** stdout/stderr only

## CI/CD & Deployment

**Hosting:** Not deployed (library/module)
**CI Pipeline:** None detected
**Repository:** https://github.com/Luke-42-studios/cels-ncurses.git

## Environment Configuration

**Required env vars:** None
**Secrets:** None

## Webhooks & Callbacks

**Incoming:** None
**Outgoing:** None

## Frame Loop Architecture

The execution flow controlled by the module:

```
main() [cels/src/backends/main.c]
  -> Engine_Startup()
  -> CELS_BuildInit() [user's CEL_Build() block]
     -> TUI_Engine_use()
        -> TUI_Window_use()    # registers startup + frame_loop hooks
        -> TUI_Input_use()     # registers ECS system at OnLoad
        -> tui_renderer_init() # registers Feature/Provider
        -> root composition    # user's AppUI function
  -> cels_run_providers()
     -> startup_fn()           # tui_window_startup(): initscr, colors, etc.
     -> frame_loop_fn()        # tui_window_frame_loop(): blocks in while loop
        while (g_running):
           Engine_Progress(delta)
              -> flecs pipeline:
                 OnLoad:    CELS_LifecycleSystem (lifecycle eval)
                            TUI_InputSystem (getch + populate CELS_Input)
                 PostLoad:  CELS_RecompositionSystem (process dirty queue)
                 OnUpdate:  User app systems (e.g., MainMenuInputSystem)
                 OnStore:   TUI_Renderable_Canvas (render provider system)
           doupdate()         # flush ncurses buffer
           usleep(delta)      # FPS timing
  -> Engine_Shutdown()
```

## Inter-Provider Communication

**Window -> Input:** `tui_window_get_running_ptr()` returns `volatile int*` to `g_running` flag. Input provider sets `*g_tui_running_ptr = 0` on Q key to signal quit.

**Window -> Renderer:** `TUI_WindowState` is a global struct accessible by the renderer for title/version display in info boxes. `TUI_WindowStateID` is used for `cels_state_notify_change()` to trigger root composition recomposition.

**Input -> App Systems:** Input is written via `cels_input_set()` and read by app systems via `cels_input_get()`. The module does not process input semantics -- that's the application's responsibility.

---

*Integration audit: 2026-02-07*
