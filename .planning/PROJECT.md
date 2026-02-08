# cels-ncurses

## What This Is

A terminal graphics API for the CELS framework that treats ncurses like a 2D rendering engine. Instead of pixels, developers control cursor positions; instead of draw calls, they use character-based primitives (rectangles, text, borders, fills). The module provides explicit layer management with z-ordering, giving developers the same mental model as a game engine's graphics backend — but targeting terminal output.

## Core Value

Provide a low-level, backend-agnostic drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal — the same way clay_renderer_SDL3.c targets SDL3.

## Requirements

### Validated

- ✓ Window lifecycle management (ncurses init/cleanup, frame loop, FPS throttling) — existing
- ✓ Input provider (getch → CELS_Input mapping, backend-agnostic input struct) — existing
- ✓ Feature/Provider integration (CEL_DefineFeature, CEL_Provides, CEL_Feature) — existing
- ✓ TUI_Engine module facade (single TUI_Engine_use() entry point) — existing
- ✓ WindowState state machine with observer-based reactivity — existing
- ✓ Signal handling and clean terminal restoration — existing

### Active

- [ ] Rectangle drawing primitives (filled and outlined, character-based rounded corners)
- [ ] Text rendering with positioned output and attribute control (color, bold, dim, underline)
- [ ] Border drawing with box-drawing characters (single/double/rounded) and per-side control
- [ ] Scissor/clipping regions (restrict drawing to rectangular area, needed for scroll containers)
- [ ] Explicit layer system with z-order (named layers, composited each frame)
- [ ] Color/attribute system (color pairs, bold, dim, reverse, underline as material-like API)
- [ ] Drawing context/surface abstraction (target for draw commands, similar to render targets)
- [ ] Move app-specific rendering (Canvas, Button, Slider widgets) out of module into example app
- [ ] Frame-based rendering pipeline (begin frame → draw to layers → composite → flush)

### Out of Scope

- Clay integration directly in this module — separate cels-clay module will translate Clay_RenderCommandArray into ncurses drawing primitives
- Widget/component library — this is a graphics API, not a UI toolkit; widgets belong in app code or higher-level modules
- Mouse input — keyboard/gamepad-style input only for v1
- Windows/macOS support — POSIX/Linux only (existing constraint)
- True color support — standard ncurses color pairs sufficient for v1
- Animation/tweening system — draw primitives only, animation is app-level concern

## Context

- **CELS Framework**: This module is a CELS provider module. It follows the CEL_DefineModule / CEL_Use / CEL_Provides pattern. Sources compile as an INTERFACE CMake library in the consumer's translation unit.
- **Existing code**: The module already has working Window, Input, and Renderer providers. The Renderer currently contains app-specific rendering logic (Canvas headers, Button/Slider widgets) that needs to be extracted and replaced with the new graphics API.
- **Clay target**: The primary downstream consumer will be a cels-clay module that receives `Clay_RenderCommandArray` and translates each command (RECTANGLE, TEXT, BORDER, SCISSOR_START/END) into calls to this module's drawing primitives. The API surface should map cleanly to what Clay renderers need.
- **ncurses panels**: The ncurses `panel` library provides native window stacking/z-ordering which aligns with the explicit layer system requirement.
- **flecs ECS**: The module uses flecs directly for system registration and entity queries. The graphics API itself should be usable both from ECS systems and from standalone code.

## Constraints

- **Tech stack**: C99, ncurses (wide-character variant), CELS framework, flecs ECS — all established
- **Library type**: INTERFACE CMake library — sources compile in consumer context, so consumer's `components.h` resolves via their include paths
- **Rendering model**: Character grid — smallest addressable unit is one terminal cell (not sub-character). All coordinates are in cell units (col, row).
- **Dependencies**: Must remain lightweight — ncurses + CELS core only, no additional external libraries

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Graphics API mental model (not widget library) | Developer wants game-engine-style drawing control; cursor position = pixel equivalent | — Pending |
| Explicit layers with z-order (not painter's algorithm) | Provides proper compositing, cleaner API for overlapping UI regions | — Pending |
| Separate cels-clay module for Clay integration | Keeps ncurses module focused on primitives; Clay translation is a distinct concern | — Pending |
| Move app-specific rendering to example app | Module should only provide the graphics API; Canvas/Button/Slider rendering is app code | — Pending |
| Primitives match Clay renderer needs | Rectangles, text, borders, scissoring = exact set Clay renderers implement | — Pending |

---
*Last updated: 2026-02-07 after initialization*
