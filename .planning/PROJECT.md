# cels-ncurses

## What This Is

A terminal graphics API for the CELS framework that treats ncurses like a 2D rendering engine. Developers use character-based primitives (rectangles, text, borders, lines) drawn into panel-backed layers with z-ordering, composited through a retained-mode frame pipeline. The module provides the same mental model as a game engine's graphics backend -- but targeting terminal output.

## Core Value

Provide a low-level, backend-agnostic drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal -- the same way clay_renderer_SDL3.c targets SDL3.

## Requirements

### Validated

- ✓ Window lifecycle management (ncurses init/cleanup, frame loop, FPS throttling) -- existing
- ✓ Input provider (getch -> CELS_Input mapping, backend-agnostic input struct) -- existing
- ✓ TUI_Engine module facade (single TUI_Engine_use() entry point) -- existing
- ✓ WindowState state machine with observer-based reactivity -- existing
- ✓ Signal handling and clean terminal restoration -- existing
- ✓ Rectangle drawing primitives (filled and outlined, rounded corners) -- v1.0
- ✓ Text rendering with positioned output and attribute control -- v1.0
- ✓ Border drawing with box-drawing characters and per-side control -- v1.0
- ✓ Scissor/clipping regions with nested intersection -- v1.0
- ✓ Explicit layer system with z-order (panel-backed, composited each frame) -- v1.0
- ✓ Color/attribute system (dynamic alloc_pair, wattr_set, RGB mapping) -- v1.0
- ✓ Drawing context/surface abstraction (TUI_DrawContext wrapping WINDOW*) -- v1.0
- ✓ Frame-based rendering pipeline (begin/end lifecycle, dirty tracking, single doupdate) -- v1.0
- ✓ App-specific rendering extracted to example app (pure graphics backend) -- v1.0
- ✓ Terminal resize handling (KEY_RESIZE, layer resize, observer notification) -- v1.0

### Active

- [ ] Mouse input (click, drag, scroll wheel, hover/motion tracking)
- [ ] Sub-cell rendering (half-block and braille characters for higher resolution)
- [ ] True color 24-bit RGB (bypass ncurses color pair limits with direct escape sequences)
- [ ] Damage tracking (only redraw changed cells between frames)
- [ ] Layer transparency/blending (transparent regions showing through to layers below)

### Out of Scope

- Clay integration directly in this module -- separate cels-clay module will translate Clay_RenderCommandArray into ncurses drawing primitives
- Widget/component library -- this is a graphics API, not a UI toolkit; widgets belong in app code or higher-level modules
- Windows/macOS support -- POSIX/Linux only (existing constraint)
- Animation/tweening system -- draw primitives only, animation is app-level concern
- Text measurement/layout -- Clay handles text measurement via MeasureText callback
- Image/bitmap rendering -- terminal image protocols (Sixel, Kitty) are too terminal-specific

## Context

- **CELS Framework**: This module is a CELS provider module. It follows the CEL_DefineModule / CEL_Use pattern. Sources compile as an INTERFACE CMake library in the consumer's translation unit.
- **Shipped v1.0**: 3,886 LOC C across src/ and include/. Foundation types, drawing primitives, panel-backed layers, frame pipeline, clean module boundary.
- **Clay target**: The primary downstream consumer will be a cels-clay module that receives `Clay_RenderCommandArray` and translates each command into calls to this module's drawing primitives.
- **ncurses panels**: The ncurses `panel` library provides native window stacking/z-ordering for the layer system.
- **flecs ECS**: The module uses flecs directly for system registration. The graphics API is usable both from ECS systems and standalone code.

## Constraints

- **Tech stack**: C99, ncurses (wide-character variant), CELS framework, flecs ECS -- all established
- **Library type**: INTERFACE CMake library -- sources compile in consumer context
- **Rendering model**: Character grid with sub-cell rendering via Unicode block elements. Base coordinates in cell units (col, row).
- **Dependencies**: Must remain lightweight -- ncurses + CELS core only, no additional external libraries

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Graphics API mental model (not widget library) | Developer wants game-engine-style drawing control | ✓ Good |
| Explicit layers with z-order (panel-backed) | Native compositing via ncurses panel library | ✓ Good |
| Separate cels-clay module for Clay integration | Keeps ncurses module focused on primitives | ✓ Good |
| App-specific rendering extracted to example app | Module is pure graphics backend | ✓ Good |
| alloc_pair (not init_pair) for color pairs | Built-in LRU eviction, no manual pool | ✓ Good |
| wattr_set (not attron/attroff) | Atomic attribute application, no state leaks | ✓ Good |
| WINDOW* borrowed not owned | Caller manages lifetime, avoids double-free | ✓ Good |
| werase (not wclear) for dirty clearing | Avoids flicker from clearok | ✓ Good |
| stdscr for input only | All drawing through TUI_DrawContext/layers | ✓ Good |

---
*Last updated: 2026-02-20 after v1.0 milestone*
