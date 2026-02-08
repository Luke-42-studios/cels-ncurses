# Codebase Structure

**Analysis Date:** 2026-02-07

## Directory Layout

```
cels-ncurses/
├── CMakeLists.txt              # INTERFACE library build definition
├── include/
│   └── cels-ncurses/
│       ├── tui_engine.h        # Engine module facade (single include for consumers)
│       ├── tui_window.h        # Window provider: state machine, config, lifecycle
│       ├── tui_input.h         # Input provider: zero-config ECS system
│       ├── tui_renderer.h      # Renderer provider: color pair constants, init
│       └── tui_components.h    # Widget renderers: canvas, button, slider, etc.
├── src/
│   ├── tui_engine.c            # Engine module implementation (CEL_DefineModule)
│   ├── window/
│   │   └── tui_window.c        # ncurses init, frame loop, state transitions
│   ├── input/
│   │   └── tui_input.c         # getch() key mapping, ECS system registration
│   └── renderer/
│       └── tui_renderer.c      # Feature/Provider registration, render callback
└── .planning/
    └── codebase/               # Architecture and structure docs (this file)
```

## Directory Purposes

**`include/cels-ncurses/`:**
- Purpose: Public headers for consumer applications
- Contains: Type definitions (structs, enums), function declarations, inline widget renderers
- Key files: `tui_engine.h` is the primary consumer include; `tui_components.h` provides header-only widget functions

**`src/`:**
- Purpose: Implementation files for the three providers and the engine module
- Contains: Provider registration logic, ncurses terminal management, ECS system callbacks, rendering pipeline
- Key files: `tui_engine.c` (module facade), `window/tui_window.c` (frame loop owner)

**`src/window/`:**
- Purpose: Window provider implementation
- Contains: ncurses lifecycle (initscr/endwin), frame loop, signal handling, WindowState transitions
- Key files: `tui_window.c`

**`src/input/`:**
- Purpose: Input provider implementation
- Contains: Key reading (getch), key-to-CELS_Input mapping, ECS system registration at OnLoad phase
- Key files: `tui_input.c`

**`src/renderer/`:**
- Purpose: Renderer provider implementation
- Contains: Feature/Provider declarations, full-screen render callback that queries ECS world
- Key files: `tui_renderer.c`

**`.planning/codebase/`:**
- Purpose: Architecture documentation for GSD tooling
- Generated: No (manually written)
- Committed: Yes

## Key File Locations

**Entry Points:**
- `src/tui_engine.c`: Module facade -- `TUI_Engine_use()` is the consumer's single entry point
- `src/window/tui_window.c`: `TUI_Window_use()` registers startup + frame loop hooks with CELS

**Configuration:**
- `CMakeLists.txt`: Build configuration (INTERFACE library, ncurses dependency, source lists)
- `include/cels-ncurses/tui_engine.h`: `TUI_EngineConfig` struct (title, version, fps, root function pointer)
- `include/cels-ncurses/tui_window.h`: `TUI_Window` config struct and `TUI_WindowState_t` observable state

**Core Logic:**
- `src/window/tui_window.c`: Frame loop (`tui_window_frame_loop`), ncurses initialization (`tui_window_startup`), WindowState transitions
- `src/input/tui_input.c`: Input reading and key mapping (`tui_read_input_ncurses`), quit signaling
- `src/renderer/tui_renderer.c`: Full-screen render callback (`tui_prov_render_screen`), Feature/Provider registration

**Widget Rendering:**
- `include/cels-ncurses/tui_components.h`: All static inline widget renderers (`tui_render_canvas`, `tui_render_button`, `tui_render_slider_cycle`, `tui_render_slider_toggle`, `tui_render_hint`, `tui_render_info_box`)
- `include/cels-ncurses/tui_renderer.h`: Color pair enum (`CP_SELECTED`, `CP_HEADER`, `CP_ON`, `CP_OFF`, `CP_NORMAL`, `CP_DIM`)

**Testing:**
- No tests exist within this module. Tests live in the parent CELS project at `/home/cachy/workspaces/libs/cels/tests/`

## Naming Conventions

**Files:**
- Source files: `tui_<subsystem>.c` (e.g., `tui_window.c`, `tui_input.c`, `tui_renderer.c`, `tui_engine.c`)
- Headers: `tui_<subsystem>.h` matching the source file name
- Include guard: `CELS_NCURSES_TUI_<SUBSYSTEM>_H`

**Directories:**
- Source subdirectories: lowercase singular noun matching the provider name (`window/`, `input/`, `renderer/`)
- Headers: flat under `include/cels-ncurses/` (no subdirectories)

**Functions:**
- Provider registration: `TUI_<Provider>_use(TUI_<Provider> config)` (e.g., `TUI_Window_use`, `TUI_Input_use`)
- Internal callbacks: `tui_<subsystem>_<action>` (e.g., `tui_window_startup`, `tui_read_input_ncurses`, `tui_prov_render_screen`)
- Widget renderers: `tui_render_<widget>` (e.g., `tui_render_button`, `tui_render_canvas`)
- State helpers: `tui_window_get_running_ptr()`, `TUI_WindowState_ensure()`

**Types:**
- Provider config structs: `TUI_<Provider>` (e.g., `TUI_Window`, `TUI_Input`)
- State structs: `TUI_<Provider>State_t` (e.g., `TUI_WindowState_t`)
- Enums: `PascalCase` (e.g., `WindowState`, `WINDOW_STATE_READY`)
- Color pair constants: `CP_<NAME>` (e.g., `CP_SELECTED`, `CP_HEADER`)

**Variables:**
- Module-local static state: `g_<name>` (e.g., `g_running`, `g_tui_config`, `g_render_row`, `g_render_frame`)
- External globals: `TUI_WindowState`, `TUI_WindowStateID`, `TUI_Engine`

## Where to Add New Code

**New Provider (e.g., Audio, Physics):**
- Header: `include/cels-ncurses/tui_<provider>.h` -- define config struct `TUI_<Provider>` and `TUI_<Provider>_use()` declaration
- Implementation: `src/<provider>/tui_<provider>.c` -- implement `TUI_<Provider>_use()`, register ECS systems or hooks
- Register in engine: Add `CEL_Use(TUI_<Provider>)` call inside `CEL_DefineModule(TUI_Engine)` body in `src/tui_engine.c`
- Build: Add source to `target_sources()` in `CMakeLists.txt`

**New Widget Renderer:**
- Add a new `static inline void tui_render_<widget>(...)` function in `include/cels-ncurses/tui_components.h`
- Use `g_render_row++` for vertical positioning, `mvprintw()` for output, `COLOR_PAIR(CP_*)` for styling
- Call the new widget from the render callback in `src/renderer/tui_renderer.c`

**New Color Pair:**
- Add constant to the enum in `include/cels-ncurses/tui_renderer.h` (next value after `CP_DIM = 6`)
- Add `init_pair()` call in `tui_window_startup()` in `src/window/tui_window.c`

**New Key Binding:**
- Add case to the switch in `tui_read_input_ncurses()` in `src/input/tui_input.c`
- If new `CELS_Input` fields needed, update the struct in `/home/cachy/workspaces/libs/cels/include/cels/cels.h`

**New ECS Feature/Provides Pair:**
- Define feature: `CEL_DefineFeature(FeatureName, .phase = CELS_Phase_X)` in the renderer or a new provider
- Declare component-feature link: `CEL_Feature(ComponentName, FeatureName)` in the provider init function
- Register implementation: `CEL_Provides(TUI, FeatureName, ComponentName, callback_fn)` in the provider init function

## Special Directories

**`.planning/codebase/`:**
- Purpose: GSD tooling architecture and structure documentation
- Generated: No
- Committed: Yes

## Relationship to Parent CELS Project

This module is a **git submodule** of the parent CELS project at `/home/cachy/workspaces/libs/cels/`. The parent project:

- Includes this module via `add_subdirectory(modules/cels-ncurses)` in its `CMakeLists.txt`
- Provides the `cels` static library that this module links against (`target_link_libraries(cels-ncurses INTERFACE cels)`)
- Defines the consumer application (`examples/app.c`) that includes `<cels-ncurses/tui_engine.h>`
- Provides shared component definitions in `examples/components.h` which the renderer includes via `#include "components.h"` (resolved through consumer include paths)

Because this is an INTERFACE library, all source files compile in the consumer's translation unit. This means:
- `components.h` is resolved via the consumer's include paths, not this module's
- Static variables in headers (like `g_render_row` in `tui_components.h`) exist per-translation-unit
- The consumer must link both `cels` and `ncurses` (handled automatically by CMake transitive dependencies)

---

*Structure analysis: 2026-02-07*
