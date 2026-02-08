# Codebase Concerns

**Analysis Date:** 2026-02-07

## Tech Debt

**Hardcoded window dimensions (commented-out real logic):**
- Issue: `TUI_WindowState.width` and `.height` are hardcoded to `600` and `800` with the correct ncurses `COLS`/`LINES` logic commented out directly beside them.
- Files: `src/window/tui_window.c` (lines 121-122)
- Impact: `TUI_WindowState` reports wrong dimensions to any consumer that reads `.width`/`.height`. Any layout logic relying on window size will produce incorrect results. The values 600/800 are pixel-style numbers that make no sense in a character-cell terminal.
- Fix approach: Uncomment the original logic (`g_tui_config.width > 0 ? g_tui_config.width : COLS` and similar for height). Verify the `TUI_Window` config struct already has `width`/`height` fields (it does, see `include/cels-ncurses/tui_window.h` line 79-80).

**Renderer is tightly coupled to example app components:**
- Issue: `tui_renderer.c` includes `"components.h"` which is located in `examples/components.h`, not inside the module. The renderer directly references app-specific types (`Canvas`, `Selectable`, `Text`, `ClickArea`, `Range`, `GraphicsSettings`, `ScreenType`, `SCREEN_MAIN_MENU`) and contains app-specific rendering logic (resolution labels, settings toggles, screen-specific hints).
- Files: `src/renderer/tui_renderer.c` (line 22, lines 34-38, lines 78-217, line 206)
- Impact: The module cannot be reused by any other application without bringing along the example's `components.h`. The renderer is not a generic TUI rendering provider; it is a bespoke renderer for the demo app embedded inside a module that claims to be reusable. This is the single largest architectural concern in the module.
- Fix approach: Extract the app-specific rendering callback (`tui_prov_render_screen`) and `RESOLUTIONS` array into the example app. The module should provide only generic rendering infrastructure (the `CEL_DefineFeature(Renderable, ...)` registration, color pairs, component renderers in `tui_components.h`) and let the consumer register its own provider callback via `CEL_Provides()`. Alternatively, split into a generic module (`cels-ncurses`) and an app-specific provider file.

**Hardcoded resolution labels in the renderer:**
- Issue: The `RESOLUTIONS[]` string array is hardcoded with three specific resolution strings (`"1920 x 1080"`, `"2560 x 1440"`, `"3840 x 2160"`). The renderer reads `gfx->resolution_index` and indexes directly into this static array.
- Files: `src/renderer/tui_renderer.c` (lines 34-38, line 191)
- Impact: Adding or removing resolution options requires modifying the module source code. No bounds checking on `resolution_index` -- values outside 0-2 cause undefined behavior (out-of-bounds array read).
- Fix approach: Move resolution labels into the application layer (e.g., as a component field or state data). At minimum, add bounds checking: `gfx->resolution_index % 3` or clamp to array size.

**Hardcoded canvas width constant:**
- Issue: `TUI_CANVAS_WIDTH` is `#define`d to `43` and the border string `"+-----------------------------------------+"` is a literal 43-character string repeated in multiple render functions. These do not adapt to terminal width.
- Files: `include/cels-ncurses/tui_components.h` (line 51, lines 60, 72, 215, 235)
- Impact: On terminals narrower than 43 columns, the canvas wraps or truncates. On wider terminals, it sits in the top-left corner with no centering.
- Fix approach: Accept width as a parameter or query `COLS` at render time. Use `snprintf` or loops to generate border strings dynamically.

**Hardcoded max selectable index in render loop:**
- Issue: The render loop iterates `target_idx` from 0 to `max_index = 20` to find selectables by index. This is a linear scan with an arbitrary upper bound.
- Files: `src/renderer/tui_renderer.c` (lines 162-202)
- Impact: More than 20 selectable items will not render. The O(n * m) iteration pattern (for each target index, iterate all selectables) is inefficient for larger menus, though acceptable for the current small demo.
- Fix approach: Use the actual `Canvas.item_count` or collect selectables into a sorted array before rendering.

**Unused `g_config_set` flag:**
- Issue: `g_config_set` is set to `true` in `TUI_Engine_use()` but never read anywhere.
- Files: `src/tui_engine.c` (line 20, line 55)
- Impact: Dead code. No functional impact but adds confusion.
- Fix approach: Remove the variable and the assignment.

**Duplicate `find_graphics_settings` function:**
- Issue: The exact same `find_graphics_settings()` function is defined in both `src/renderer/tui_renderer.c` (line 54) and `examples/app.c` (line 136). Both are `static` so they compile independently, but the duplication signals that this utility belongs in a shared location.
- Files: `src/renderer/tui_renderer.c` (lines 54-62), `/home/cachy/workspaces/libs/cels/examples/app.c` (lines 136-144)
- Impact: Bug fixes to one copy will not propagate to the other. Both use the same pattern of iterating all entities with `GraphicsSettingsID`.
- Fix approach: Move into a shared utility header or provide a CELS-level query helper for singleton components.

## Known Bugs

**Window dimensions report pixel values instead of character cells:**
- Symptoms: `TUI_WindowState.width` = 600, `TUI_WindowState.height` = 800 regardless of actual terminal size.
- Files: `src/window/tui_window.c` (lines 121-122)
- Trigger: Always occurs -- the correct code is commented out.
- Workaround: Consumers can query `COLS`/`LINES` directly from ncurses, bypassing `TUI_WindowState`.

**No CLOSING state notification before endwin():**
- Symptoms: When the application quits, `TUI_WindowState.state` transitions `CLOSING -> CLOSED` but `endwin()` is called via `atexit(cleanup_endwin)` which runs after `tui_window_frame_loop` returns. The `CLOSED` state notification (`cels_state_notify_change`) is never called, so observers watching for `WINDOW_STATE_CLOSED` will not recompose.
- Files: `src/window/tui_window.c` (lines 136-139)
- Trigger: Normal application quit (Q key or SIGINT).
- Workaround: None. Shutdown cleanup relying on CLOSED state observation will not fire.

**Escape key vs ncurses escape sequence ambiguity:**
- Symptoms: Pressing Escape triggers `button_cancel`, but ncurses function key sequences also begin with ESC (character 27). With `ESCDELAY = 25`, there is a 25ms window where a genuine ESC press is indistinguishable from the start of an escape sequence. On slow connections or terminals with unusual timing, function keys may be misinterpreted as ESC.
- Files: `src/window/tui_window.c` (line 85), `src/input/tui_input.c` (line 64)
- Trigger: Pressing Escape on terminals with slow escape sequence delivery.
- Workaround: `ESCDELAY = 25` is a reasonable tradeoff for local terminals. Remote/SSH users may experience issues.

## Security Considerations

**Signal handler modifies shared state:**
- Risk: `tui_sigint_handler` writes to `g_running` (a `volatile int`). While this is a common pattern, the signal handler could fire at any point during execution including mid-ncurses-call. ncurses is not async-signal-safe.
- Files: `src/window/tui_window.c` (lines 48-51)
- Current mitigation: `g_running` is `volatile int` (atomic on all practical platforms for single-word writes). The handler only writes a flag and does not call ncurses functions.
- Recommendations: Use `sig_atomic_t` instead of `volatile int` for formal correctness. Consider using `sigaction()` instead of `signal()` for portable behavior.

**No input sanitization on raw_key:**
- Risk: Arbitrary key codes are stored in `input.raw_key` as an `int` without validation. If a consumer uses this value as an index or passes it unsanitized, it could cause issues.
- Files: `src/input/tui_input.c` (lines 100-103)
- Current mitigation: The field is documented as raw and consumers are expected to validate.
- Recommendations: Document valid ranges. Consider clamping to printable ASCII or providing a helper.

## Performance Bottlenecks

**Frame loop uses busy-wait with usleep:**
- Problem: The frame loop calls `usleep(delta * 1000000)` which sleeps for the full frame duration without accounting for the time spent in `Engine_Progress()` and `doupdate()`. This means actual FPS is always lower than target FPS.
- Files: `src/window/tui_window.c` (lines 129-133)
- Cause: No time measurement before/after the frame work. The sleep duration is the full frame budget, not the remaining budget after work.
- Improvement path: Measure elapsed time with `clock_gettime(CLOCK_MONOTONIC)` before and after the frame, then sleep only the remaining budget. This is the standard game loop pattern.

**O(n*m) selectable render ordering:**
- Problem: The render callback iterates from index 0 to 20, and for each index, iterates all Selectable entities in the world to find the one at that index. This is O(max_index * selectable_count).
- Files: `src/renderer/tui_renderer.c` (lines 162-202)
- Cause: Rendering by index order requires finding each index sequentially rather than sorting once.
- Improvement path: Collect all Selectable entities into a local array, sort by index, then iterate once. For the current 3-5 item menus this is negligible, but it will not scale.

**Forced periodic redraw every 30 frames:**
- Problem: `force_redraw = (g_render_frame % 30 == 0)` causes a full screen clear and redraw every half second at 60 FPS even when nothing has changed. This is a safety net for the dirty-checking logic.
- Files: `src/renderer/tui_renderer.c` (line 96)
- Cause: The dirty-checking via static locals (`last_selected`, `last_screen`, etc.) may miss state changes, so the periodic redraw compensates.
- Improvement path: Improve dirty tracking to be exhaustive and remove the periodic forced redraw. Use CELS state observation instead of static locals.

## Fragile Areas

**tui_components.h global mutable state in header:**
- Files: `include/cels-ncurses/tui_components.h` (line 34)
- Why fragile: `static int g_render_row = 1;` is defined in a header file. Because it is `static`, every translation unit that includes this header gets its own copy. Currently only `tui_renderer.c` includes it so this works, but if a second `.c` file includes it, each file will have an independent `g_render_row` that does not track the other's rendering position.
- Safe modification: Do not include `tui_components.h` from more than one translation unit. If multiple TUs need it, move `g_render_row` to a `.c` file and expose it via an extern declaration.
- Test coverage: None.

**Renderer depends on consumer's `components.h` at include path level:**
- Files: `src/renderer/tui_renderer.c` (line 22: `#include "components.h"`)
- Why fragile: This include resolves via the consumer's include path, not the module's. The INTERFACE library compiles sources in the consumer's context. If the consumer does not have a `components.h` or has one with different types, compilation fails with cryptic errors.
- Safe modification: When changing the module's renderer, verify the consumer's `components.h` still matches the expected types (`Canvas`, `Selectable`, `Text`, `ClickArea`, `Range`, `GraphicsSettings`, `ScreenType`, `SCREEN_MAIN_MENU`).
- Test coverage: None.

**Implicit provider initialization order:**
- Files: `src/tui_engine.c` (lines 24-34), `src/input/tui_input.c` (lines 131-135)
- Why fragile: `TUI_Input_use()` calls `tui_window_get_running_ptr()` which returns a pointer to `g_running` in `tui_window.c`. This works because `TUI_Engine_init()` calls `CEL_Use(TUI_Window, ...)` before `CEL_Use(TUI_Input)`, and `TUI_Window_use()` initializes `g_running`. If the order changes, `TUI_Input` would get a valid pointer to `g_running` (it is file-scope static, initialized to 1), but the conceptual dependency is implicit.
- Safe modification: Do not reorder the `CEL_Use` calls in `TUI_Engine_init_body()`. Add a comment or assertion documenting the required order.
- Test coverage: None.

**Static locals for dirty tracking in render callback:**
- Files: `src/renderer/tui_renderer.c` (lines 90-94)
- Why fragile: Five static local variables (`last_selected`, `last_screen`, `last_resolution`, `last_fullscreen`, `last_vsync`) track previous frame state for change detection. Adding new rendered state requires manually adding another static variable and checking it. Missing a variable causes that state change to not trigger a redraw until the periodic forced redraw (every 30 frames).
- Safe modification: When adding new UI state, add a corresponding `last_*` static variable, add its comparison to the `changed` boolean expression, and update it after redraw.
- Test coverage: None.

## Scaling Limits

**Single provider slot in CELS framework:**
- Current capacity: `cels_register_startup` and `cels_register_frame_loop` each store a single function pointer (see `cels.cpp` lines 1874-1882).
- Limit: Only one window provider can be registered. Calling `cels_register_startup` a second time silently overwrites the first.
- Scaling path: Change to a vector of function pointers in the CELS framework to support multiple providers. This is a framework-level change, not a cels-ncurses change.

**Only one key per frame:**
- Current capacity: `tui_read_input_ncurses()` calls `wgetch(stdscr)` once per frame, reading exactly one key.
- Limit: At 60 FPS with fast typists or held keys, key events will be dropped. The ncurses input buffer will queue them, but only one is consumed per frame.
- Scaling path: Loop `wgetch()` until `ERR`, processing all buffered keys per frame. Combine into a single `CELS_Input` with bitmask or process sequentially.

## Dependencies at Risk

**Direct flecs API usage in module code:**
- Risk: `tui_input.c` (line 17) and `tui_renderer.c` (line 21) include `<flecs.h>` directly and use raw flecs APIs (`ecs_each_id`, `ecs_field`, `ecs_has_id`, `ecs_get_id`, `ecs_entity_init`, `ecs_system_init`). The module is coupled to the specific flecs version used by CELS.
- Impact: If CELS upgrades flecs or changes its ECS backend, all direct flecs usage in this module must be updated.
- Migration plan: Introduce CELS-level query abstractions (e.g., `cels_query_singleton`, `cels_each`) that wrap flecs. Module code should use CELS abstractions only.

## Missing Critical Features

**No terminal resize handling:**
- Problem: There is no `SIGWINCH` handler and no call to `getmaxyx()` during the frame loop. If the user resizes their terminal, `COLS`/`LINES` are not updated, the window state is not updated, and rendering continues using the original dimensions.
- Blocks: Any application that needs responsive layout or that users might resize.

**No text input support:**
- Problem: The input provider maps keys to discrete actions (navigation, accept, cancel) but has no text input mode. The `raw_key` field exists but there is no editing state (cursor position, text buffer).
- Blocks: Any application with text fields, search, or command entry.

**No multi-window/panel support:**
- Problem: All rendering uses `stdscr` directly. There are no ncurses `WINDOW` objects for sub-panels, scrollable regions, or overlapping windows.
- Blocks: Applications with split views, dialogs, scrollable lists, or modal overlays.

**No mouse support:**
- Problem: ncurses supports mouse events via `mousemask()` and `getmouse()`, but the input provider does not enable or process mouse input. `CELS_Input` has no mouse fields.
- Blocks: Any TUI application that wants clickable elements or mouse-driven navigation.

## Test Coverage Gaps

**No tests exist for the module:**
- What's not tested: Everything. There are zero test files in the cels-ncurses module. The parent CELS framework has tests in `/home/cachy/workspaces/libs/cels/tests/` but none cover the ncurses module.
- Files: Entire module (`src/`, `include/`)
- Risk: Any refactoring (especially the needed decoupling of the renderer from `components.h`) could introduce regressions with no safety net. The signal handler, frame loop timing, input mapping, color pair initialization, and component rendering are all untested.
- Priority: High. The module is small enough (~1150 lines) that a focused test effort could achieve good coverage. Key areas to test first:
  1. Input mapping (`tui_read_input_ncurses` -- mock `wgetch` and verify `CELS_Input` fields)
  2. Component rendering (`tui_render_button`, `tui_render_slider_cycle`, etc. -- mock `mvprintw` and verify output positions)
  3. Frame timing (verify delta calculation and sleep behavior)
  4. Engine module initialization order (verify providers are registered in correct sequence)

---

*Concerns audit: 2026-02-07*
