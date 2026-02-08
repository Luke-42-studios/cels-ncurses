# Technology Stack

**Analysis Date:** 2026-02-07

## Languages

**Primary:**
- C99 - All module source files (`src/tui_engine.c`, `src/window/tui_window.c`, `src/input/tui_input.c`, `src/renderer/tui_renderer.c`) and all headers (`include/cels-ncurses/*.h`)

**Secondary:**
- C++ 17 - The parent CELS runtime (`/home/cachy/workspaces/libs/cels/src/cels.cpp`) is implemented in C++ but exposes a C99 API via `extern "C"`. Module code is pure C99 and links against CELS through its C API.

**Note:** The cels-ncurses module is an INTERFACE library in CMake, meaning its `.c` sources compile in the **consumer's** translation unit context. The consumer application (e.g., `examples/app.c`) links against both `cels` (static C++ lib) and `cels-ncurses` (interface C lib) plus ncurses.

## Runtime

**Environment:**
- Linux (POSIX) - Uses `_POSIX_C_SOURCE 199309L`, `_DEFAULT_SOURCE`, `<unistd.h>`, `<signal.h>`, `usleep()`, SIGINT handling
- No Windows or macOS support currently (hardcoded POSIX APIs in `src/window/tui_window.c`)

**Package Manager:**
- None - Dependencies are fetched via CMake FetchContent or system packages
- No lockfile

**System Package Dependencies:**
- ncurses (wide-character variant, `ncursesw`) - Required at system level via `find_package(Curses REQUIRED)`
- Detected version on current system: 6.5.20240427

## Frameworks

**Core:**
- CELS Framework v0.1.0 - Declarative ECS application framework (`/home/cachy/workspaces/libs/cels/`)
  - API header: `/home/cachy/workspaces/libs/cels/include/cels/cels.h`
  - Runtime: `/home/cachy/workspaces/libs/cels/src/cels.cpp` (static library)
  - Provides: component system, state management, reactivity, lifecycle control, feature/provider model, module system

- flecs v4.1.4 - Entity Component System backend (fetched via CMake FetchContent)
  - Used directly in `src/input/tui_input.c` (`#include <flecs.h>`) for ECS system registration
  - Used directly in `src/renderer/tui_renderer.c` for entity queries (`ecs_each_id`, `ecs_field`, `ecs_has_id`, `ecs_get_id`)
  - CELS wraps flecs but module code accesses flecs APIs directly for system registration and entity queries

- ncurses (wide-character) - Terminal UI library
  - Used in all source files for terminal rendering
  - Key APIs: `initscr`, `endwin`, `wgetch`, `mvprintw`, `attron`/`attroff`, `COLOR_PAIR`, `doupdate`/`wnoutrefresh`

**Build:**
- CMake >= 3.21 - Build system (`CMakeLists.txt`)
- GCC 15.2.1 (current system) - C/C++ compiler

## Key Dependencies

**Critical (linked at build time):**
- `cels` (static library) - The CELS framework runtime. Linked via `target_link_libraries(cels-ncurses INTERFACE cels)` in `CMakeLists.txt`
- `ncursesw` (system library) - Wide-character ncurses. Found via `find_package(Curses REQUIRED)` with `CURSES_NEED_WIDE TRUE`
- `flecs::flecs_static` v4.1.4 - ECS engine. Not directly linked by cels-ncurses but required by the consumer executable (e.g., `target_link_libraries(app PRIVATE cels cels-ncurses flecs::flecs_static)`)

**Consumed at compile time (INTERFACE headers):**
- `<cels/cels.h>` - CELS macro API (CEL_Component, CEL_DefineFeature, CEL_Provides, etc.)
- `<flecs.h>` - Direct flecs API for system registration and entity queries
- `<ncurses.h>` - Terminal UI rendering functions
- Consumer's `components.h` - Application-specific component definitions (resolved via consumer's include paths)

## Configuration

**Build Configuration:**
- `CMakeLists.txt` - Module build configuration
  - `CURSES_NEED_WIDE TRUE` - Forces wide-character ncurses
  - `CURSES_NEED_NCURSES TRUE` - Forces ncurses (not plain curses)
  - Library type: INTERFACE (sources compile in consumer context)

**Parent Project Build Options:**
- `CELS_DEBUG` - Enables FlecsStats import for REST explorer debug endpoints
- `CELS_BUILD_TOOLS` - Builds cels-debug inspector tool
- Build script: `/home/cachy/workspaces/libs/cels/run.sh` - Configures Debug build with compile_commands.json export

**Runtime Configuration:**
- No config files - All configuration is done via C structs at compile time
- `TUI_EngineConfig` struct: `.title`, `.version`, `.fps`, `.root` function pointer
- `TUI_Window` struct: `.title`, `.version`, `.fps`, `.width`, `.height`

**Environment Variables:**
- None required - No env vars used by the module

## Build Process

**Build from parent project:**
```bash
# From /home/cachy/workspaces/libs/cels/
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build --parallel
```

**Run the demo app:**
```bash
./build/app
# Or via run.sh:
./run.sh app     # Launches in kitty terminal
./run.sh both    # Launches app + cels-debug inspector
```

**Git submodule setup (if cloned fresh):**
```bash
git submodule update --init modules/cels-ncurses
```

## Platform Requirements

**Development:**
- Linux with ncursesw development headers installed
- CMake >= 3.21
- C99-capable compiler (GCC recommended)
- C++17-capable compiler (for CELS runtime)
- Git (for submodule management)

**Production:**
- Linux with ncursesw shared library
- Terminal emulator with color support (256-color or true-color)
- POSIX-compliant environment (for usleep, signal handling)

## Source File Summary

| File | Lines | Purpose |
|------|-------|---------|
| `src/tui_engine.c` | 59 | Module aggregator - bundles Window + Input + Renderer |
| `src/window/tui_window.c` | 166 | Terminal lifecycle, frame loop, ncurses init |
| `src/input/tui_input.c` | 153 | Key reading, CELS_Input population, ECS system |
| `src/renderer/tui_renderer.c` | 231 | Feature/Provider rendering, entity queries |
| `include/cels-ncurses/tui_engine.h` | 80 | Engine module public API |
| `include/cels-ncurses/tui_window.h` | 91 | Window provider public API + state types |
| `include/cels-ncurses/tui_input.h` | 45 | Input provider public API |
| `include/cels-ncurses/tui_renderer.h` | 39 | Renderer public API + color pair constants |
| `include/cels-ncurses/tui_components.h` | 238 | Static inline widget renderers (canvas, button, slider, toggle, hint, info box) |
| `CMakeLists.txt` | 33 | Build configuration |

---

*Stack analysis: 2026-02-07*
