# Coding Conventions

**Analysis Date:** 2026-02-07

## Naming Patterns

**Files:**
- Source files: `tui_<subsystem>.c` -- lowercase with underscores, prefixed with `tui_`
- Header files: `tui_<subsystem>.h` -- mirror source file names
- Subdirectories per subsystem: `src/window/tui_window.c`, `src/input/tui_input.c`, `src/renderer/tui_renderer.c`
- Top-level module file: `src/tui_engine.c` (no subdirectory for the orchestrator)

**Types (structs/enums):**
- PascalCase with `TUI_` prefix for module-public types: `TUI_Window`, `TUI_WindowState_t`, `TUI_EngineConfig`, `TUI_EngineContext`
- State structs use `_t` suffix: `TUI_WindowState_t`
- Enums use PascalCase values with SCREAMING_SNAKE prefix: `WINDOW_STATE_NONE`, `WINDOW_STATE_READY`
- CELS framework enums use SCREAMING_SNAKE: `SCREEN_MAIN_MENU`, `RANGE_CYCLE`
- Color pair constants use SCREAMING_SNAKE with `CP_` prefix: `CP_SELECTED`, `CP_HEADER`, `CP_ON`

**Functions:**
- Public API: `TUI_<Type>_<verb>()` -- PascalCase type, lowercase verb: `TUI_Window_use()`, `TUI_Input_use()`, `TUI_Engine_use()`
- Internal/static functions: `tui_<subsystem>_<action>()` -- all lowercase with underscores: `tui_window_startup()`, `tui_read_input_ncurses()`, `tui_sigint_handler()`
- Inline widget renderers: `tui_render_<widget>()` -- `tui_render_button()`, `tui_render_canvas()`, `tui_render_slider_cycle()`
- ECS system callbacks: `tui_<name>_system_callback()` or `tui_prov_<name>()`
- Provider getter: `tui_window_get_running_ptr()`

**Variables:**
- Static globals: `g_<name>` prefix -- `g_running`, `g_render_row`, `g_tui_config`, `g_ncurses_active`, `g_render_frame`
- CELS entity IDs: `<TypeName>ID` -- `TUI_WindowStateID`, `CanvasID`, `SelectableID`
- Config structs: `g_tui_config` (static), passed by value in `_use()` functions

**Macros/Constants:**
- SCREAMING_SNAKE_CASE: `TUI_CANVAS_WIDTH`
- Include guards: `CELS_NCURSES_TUI_<SUBSYSTEM>_H`

## Code Style

**Formatting:**
- No automated formatter configured (no `.clang-format` or `.editorconfig` detected)
- Indent: 4 spaces (consistent across all files)
- Opening brace on same line as function/control statement
- Single blank line between functions
- No trailing whitespace

**Line Width:**
- Aim for ~80 characters, occasionally exceeding for readability (format strings, etc.)

**Braces:**
- Always use braces for `if`/`else`/`for`/`while`, even single-line bodies
- Exception: simple one-line `if` returns are sometimes braceless in switch cases

**Switch Statements:**
- Case labels indented one level inside switch
- Comments after `break;` for clarity: `case 0: /* Play Game -- no-op */ break;`
- Fall-through is not used; each case has explicit `break` or `return`

**Linting:**
- Compiler flags in parent CELS Makefile: `-std=c99 -Wall -Wextra -pedantic`
- CMake enforces `C_STANDARD 99`, `C_STANDARD_REQUIRED ON`, `C_EXTENSIONS OFF`
- Use `(void)param;` to suppress unused parameter warnings: `(void)sig;`, `(void)config;`, `(void)it;`, `(void)props;`

## Header Documentation Pattern

Every header file and every major section within files follows this pattern:

```c
/*
 * TUI <Module Name> - <One-line description>
 *
 * <Multi-line explanation of purpose, architecture notes, and design rationale>
 *
 * Usage:
 *   #include <cels-ncurses/tui_<module>.h>
 *
 *   <Example code showing typical usage>
 */
```

Use section separators with `=` dividers for grouping within files:

```c
/* ============================================================================
 * Section Name
 * ============================================================================
 *
 * Optional multi-line description of the section purpose.
 */
```

## Import Organization

**Order:**
1. Own module headers: `#include "cels-ncurses/tui_engine.h"`
2. Sibling module headers: `#include "cels-ncurses/tui_window.h"`, `#include "cels-ncurses/tui_components.h"`
3. Framework headers: `#include <cels/cels.h>`
4. External library headers: `#include <flecs.h>`, `#include <ncurses.h>`
5. System headers: `#include <unistd.h>`, `#include <signal.h>`, `#include <stdlib.h>`, `#include <stdbool.h>`, `#include <string.h>`
6. Consumer-resolved headers: `#include "components.h"` (resolved from consumer's include path)

**Path Style:**
- Own module headers use angle brackets when included from consumers: `#include <cels-ncurses/tui_window.h>`
- Own module headers use quotes when included from within the module: `#include "cels-ncurses/tui_engine.h"`
- Use the `cels-ncurses/` namespace prefix for all module public headers

**POSIX Feature Test Macros:**
- Place at the very top of `.c` files before any includes:
```c
#define _POSIX_C_SOURCE 199309L
#define _DEFAULT_SOURCE
```

## Error Handling

**Strategy:** Defensive checks with early return; no exceptions (C codebase). No error codes or result types used.

**Patterns:**
- NULL checks with early return: `if (!world) return;`, `if (!gfx) return NULL;`
- Zero-ID guard for CELS entity IDs: `if (CanvasID == 0) return NULL;`
- Signal handler for SIGINT: sets `g_running = 0` for clean shutdown
- `atexit()` handler for ncurses cleanup: `cleanup_endwin()` ensures `endwin()` is called
- ncurses session guard: `if (g_ncurses_active && !isendwin()) endwin();`
- No `assert()` usage -- all checks are runtime guards with graceful degradation

## Logging

**Framework:** No logging framework. No `printf` or `fprintf` in module code.

**Patterns:**
- Module code is silent in production -- no logging output
- Debug info goes through CELS framework (`CEL_Description()` for entity metadata)
- The example app (`app.c`) also has no direct printf output

## Comments

**When to Comment:**
- Every file gets a top-level block comment explaining purpose, architecture, and usage
- Every major code section gets an `=` divider comment block
- Inline comments for non-obvious code: `/* Fast escape key response (default ~1000ms is too slow) */`
- Comment `NOTE:` prefixes for important caveats: `/* NOTE: This file compiles in the CONSUMER's context */`
- End-of-file include guard comment: `#endif /* CELS_NCURSES_TUI_COMPONENTS_H */`

**Style:**
- Use `/* ... */` block comments exclusively (C99 compatibility, no `//`)
- Inline comments on the same line for quick explanations
- Multi-line block comments with ` * ` continuation prefix

## Function Design

**Size:** Functions are typically 20-60 lines. The largest function is `tui_prov_render_screen()` at ~140 lines (render callback with ECS iteration).

**Parameters:**
- Config structs passed by value for small configs: `TUI_Window config`, `TUI_EngineConfig config`
- Pointers for output/state: `CELS_Iter* it`, `const CELS_Input* input`
- Use `const` for read-only pointers: `const char* title`, `const CELS_Input* input`
- `bool` for toggles: `bool selected`, `bool value`

**Return Values:**
- Pointer returns for state access: `TUI_WindowState_t* TUI_Window_use()`
- `void` for registration/setup functions: `TUI_Input_use()`, `tui_renderer_init()`
- `volatile int*` for shared mutable state: `tui_window_get_running_ptr()`

## Provider Pattern (Module Design)

The cels-ncurses module follows the CELS provider/module pattern:

**Provider Registration (`_use()` functions):**
```c
/* Pattern: Store config, register hooks, return state pointer */
TUI_WindowState_t* TUI_Window_use(TUI_Window config) {
    g_tui_config = config;
    TUI_WindowState_ensure();
    cels_register_startup(tui_window_startup);
    cels_register_frame_loop(tui_window_frame_loop);
    return &TUI_WindowState;
}
```

**Module Bundling (`CEL_DefineModule`):**
```c
/* Pattern: Module orchestrates multiple providers */
CEL_DefineModule(TUI_Engine) {
    CEL_Use(TUI_Window, .title = ..., .fps = ...);
    CEL_Use(TUI_Input);
    tui_renderer_init();
    /* Call root composition if provided */
    if (g_tui_config.root) { ... }
}
```

**Feature/Provider Registration:**
```c
/* Pattern: Declare features, register providers, declare consumed components */
void tui_renderer_init(void) {
    CEL_Feature(Canvas, Renderable);
    CEL_Provides(TUI, Renderable, Canvas, tui_prov_render_screen);
    CEL_ProviderConsumes(Text, ClickArea, Selectable, Range, GraphicsSettings);
}
```

## Static Inline Pattern (Header-Only Widgets)

Widget renderers in `tui_components.h` use `static inline` for header-only implementation:

```c
static inline void tui_render_button(const char* label, bool selected) {
    if (selected) {
        attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvprintw(g_render_row, 2, "> %-20s <", label);
        attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
    } else {
        mvprintw(g_render_row, 4, "%-20s", label);
    }
    g_render_row++;
}
```

- All widget functions are `static inline` in the header
- Global row tracker `g_render_row` is `static` in the header (file-scoped per TU)
- Call `tui_render_reset_row()` at start of each frame
- Use `attron()`/`attroff()` pairs symmetrically for ncurses attributes
- Use `mvprintw()` for positioned output, `printw()` for continuation

## CELS DSL Conventions (Consumer Code)

When writing code that uses this module, follow the CELS macro DSL:

```c
/* State definition */
CEL_State(MenuState, { ScreenType screen; int selected; });

/* Observer + Lifecycle */
CEL_Observer(MainMenuVisible, MenuState) {
    return MenuState.screen == SCREEN_MAIN_MENU;
}
CEL_Lifecycle(MainMenuLC, .systemVisibility = &MainMenuVisible);

/* Composition */
CEL_Composition(MainMenu) {
    (void)props;
    CEL_Has(Canvas, .title = "CELS DEMO", .screen = SCREEN_MAIN_MENU);
    CEL_Init(Button, .label = "Play", .index = 0) {}
}

/* System */
CEL_System(InputSystem) {
    (void)it;
    /* ... */
}

/* Entry point */
CEL_Build(App) {
    (void)props;
    TUI_Engine_use((TUI_EngineConfig){ .title = "App", .fps = 60, .root = AppUI });
}
```

- Always `(void)props;` or `(void)it;` if unused
- Use designated initializers: `.field = value`
- Empty block `{}` after `CEL_Init()` calls (required by macro expansion)

## INTERFACE Library Pattern

This module is a CMake INTERFACE library -- sources compile in the consumer's translation unit:

```cmake
add_library(cels-ncurses INTERFACE)
target_sources(cels-ncurses INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/...)
target_include_directories(cels-ncurses INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(cels-ncurses INTERFACE ${CURSES_LIBRARIES} cels)
```

This means:
- `#include "components.h"` in `tui_renderer.c` resolves from the consumer's include paths
- Static variables in source files are per-consumer (not shared across consumers)
- No separate `.so`/`.a` artifact is built for this module

---

*Convention analysis: 2026-02-07*
