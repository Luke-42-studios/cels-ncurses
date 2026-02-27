# cels-ncurses

## What This Is

A terminal graphics module for the CELS framework that treats ncurses like a 2D rendering engine — the same way SDL3 is a rendering backend for games. Developers create entities with window, layer, and input components. NCurses systems process these in pipeline phases, owning the terminal lifecycle and exposing draw primitives that callers (Clay renderer, widgets, raw code) use directly.

## Current Milestone: v0.2.0 ECS Module Architecture

**Goal:** Restructure cels-ncurses from a monolithic Engine module into a proper CELS ECS module. One `cel_module(NCurses)` that registers components, systems, and entities. Developer configures by setting component data on entities; NCurses systems react through ECS queries in pipeline phases.

**Target features:**
- Window entity with config + state components (developer writes config, NCurses writes state)
- Input system running in a CELS phase (NCurses fills state, developer reads after)
- Layer entities with TUI_Layer component (NCurses creates panels internally, exposes TUI_DrawContext)
- Frame pipeline as NCurses systems in CELS pipeline phases (begin/end frame)
- Single cel_module(NCurses) replacing Engine + CelsNcurses modules
- Demo: button + box with layers, developer tells NCurses how to render via entities

## Core Value

Provide a low-level, backend-agnostic drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal -- the same way clay_renderer_SDL3.c targets SDL3.

## Requirements

### Validated

- ✓ Rectangle drawing primitives (filled and outlined, rounded corners) -- v1.0
- ✓ Text rendering with positioned output and attribute control -- v1.0
- ✓ Border drawing with box-drawing characters and per-side control -- v1.0
- ✓ Scissor/clipping regions with nested intersection -- v1.0
- ✓ Color/attribute system (dynamic alloc_pair, wattr_set, RGB mapping) -- v1.0
- ✓ Drawing context/surface abstraction (TUI_DrawContext wrapping WINDOW*) -- v1.0
- ✓ Mouse input (raw press/release with cell coordinates) -- v1.1
- ✓ True color 24-bit RGB (palette redefinition with 256-color fallback) -- v1.1
- ✓ Sub-cell rendering (half-block, quadrant, braille) -- v1.1

### Active

- [ ] Window entity: developer creates entity with TUI_WindowConfig, NCurses system initializes terminal and writes TUI_WindowState
- [ ] Input system: NCurses system fills input state per-frame in input phase, developer reads after phase completes
- [ ] Layer entities: developer creates entity with TUI_Layer component, NCurses creates panel/WINDOW internally and exposes TUI_DrawContext
- [ ] Frame pipeline: NCurses systems for begin/end frame registered in CELS pipeline phases
- [ ] Module boundary: single cel_module(NCurses) replaces Engine + CelsNcurses, clean public API
- [ ] Demo: example app renders button + box with layers using the new entity-driven API

### Out of Scope

- Clay integration — separate cels-clay module targets these primitives
- Widget/component library — graphics API provides primitives only
- Damage tracking — deferred to v0.3.0+, built on new architecture
- Layer transparency — deferred to v0.3.0+, built on new architecture
- Command buffer / render queue in NCurses — not needed; Clay has its own, NCurses exposes direct draw calls
- Windows/macOS support — POSIX/Linux only
- Animation/tweening — app-level concern
- Text measurement/layout — Clay handles via MeasureText callback

## Context

- **CELS Framework**: This is a CELS module using `cel_module()`. NCurses registers components, systems, and entities — not sub-modules. Sources compile as an INTERFACE CMake library in the consumer's translation unit.
- **Mental model**: NCurses = SDL for the terminal. Owns the terminal, exposes draw primitives. Clay/widgets are callers, not owners.
- **v1.0/v1.1 shipped**: Drawing primitives, layers, frame pipeline, mouse, true color, sub-cell rendering. All being restructured from monolithic Engine to ECS-native module.
- **Clay target**: The primary downstream consumer will be a cels-clay module that iterates `Clay_RenderCommandArray` and calls NCurses draw functions directly (same pattern as Clay SDL3 renderer).
- **ncurses panels**: Panel library provides native window stacking/z-ordering for the layer system.
- **flecs ECS**: Module uses flecs for component registration, system scheduling, and entity management.

## Constraints

- **Tech stack**: C99, ncurses (wide-character variant), CELS framework, flecs ECS
- **Library type**: INTERFACE CMake library — sources compile in consumer context
- **Rendering model**: Character grid with sub-cell rendering via Unicode block elements. Base coordinates in cell units (col, row).
- **Dependencies**: ncurses + CELS core only, no additional external libraries
- **ECS-native**: All configuration through components on entities. No global config structs or init functions with config parameters.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| NCurses = SDL mental model | Terminal backend exposes primitives, callers draw directly | — Pending |
| One cel_module(NCurses), no sub-modules | Window/Input/Layer are components+systems, not modules | — Pending |
| Window entity (config + state components) | Developer writes config, NCurses writes state on same entity | — Pending |
| TUI_DrawContext kept as surface abstraction | Layers create contexts, developer draws into them | — Pending |
| Layer entities with TUI_Layer component | NCurses creates panel/WINDOW internally | — Pending |
| Input as state filled per-phase | NCurses input system runs in phase, developer reads after | — Pending |
| No command buffer in NCurses | Clay has its own; NCurses exposes direct draw calls like SDL | — Pending |
| Defer damage tracking + transparency | v0.2.0 is purely module restructure, features come in v0.3.0+ | — Pending |
| Graphics API (not widget library) | Game-engine-style drawing control | ✓ Good |
| Separate cels-clay module | Keeps ncurses module focused on primitives | ✓ Good |
| alloc_pair (not init_pair) | Built-in LRU eviction, no manual pool | ✓ Good |
| wattr_set (not attron/attroff) | Atomic attribute application, no state leaks | ✓ Good |
| werase (not wclear) | Avoids flicker from clearok | ✓ Good |

---
*Last updated: 2026-02-26 after v0.2.0 milestone start*
