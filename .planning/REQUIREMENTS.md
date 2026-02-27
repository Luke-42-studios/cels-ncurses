# Requirements: cels-ncurses v0.2.0 ECS Module Architecture

**Defined:** 2026-02-26
**Core Value:** Provide a low-level drawing primitive API that a future cels-clay module can target — the same way clay_renderer_SDL3.c targets SDL3

## v0.2.0 Requirements

### Window Entity

- [ ] **WIN-01**: Developer can create an entity with TUI_WindowConfig component to configure terminal (title, fps, color_mode)
- [ ] **WIN-02**: NCurses system initializes terminal from TUI_WindowConfig and attaches TUI_WindowState component (width, height, running, actual_fps) to the same entity
- [ ] **WIN-03**: Developer can query TUI_WindowState to read terminal dimensions and running status

### Input System

- [ ] **INP-01**: NCurses input system runs in a CELS input phase, reading getch/mouse events and writing to input state
- [ ] **INP-02**: Developer can read input state (key presses, mouse position, button events) after the input phase completes

### Layer Entities

- [ ] **LAYR-01**: Developer can create an entity with TUI_Layer component (z_order, visible, dimensions) to define a layer
- [ ] **LAYR-02**: NCurses system creates panel/WINDOW internally for each TUI_Layer entity and attaches TUI_DrawContext component
- [ ] **LAYR-03**: Developer can get TUI_DrawContext from a layer entity and draw into it using existing draw primitives

### Frame Pipeline

- [ ] **FRAM-01**: NCurses registers begin-frame and end-frame systems in CELS pipeline phases
- [ ] **FRAM-02**: Begin-frame system clears layers; end-frame system composites panels and calls doupdate()
- [ ] **FRAM-03**: Developer systems run between begin-frame and end-frame phases (natural CELS phase ordering)

### Module Boundary

- [ ] **MOD-01**: Single cel_module(NCurses) replaces Engine and CelsNcurses modules, registering all components and systems
- [ ] **MOD-02**: Developer initializes NCurses by creating entities with components (no Engine_use() or config structs)
- [ ] **MOD-03**: Public API is entity-component based: create window entity, create layer entities, draw into contexts

### Demo

- [ ] **DEMO-01**: Example app creates a window entity, layer entities, and renders a button and box using the new entity-driven API

## v0.3.0 Requirements (Deferred)

### Damage Tracking

- **DMGT-01**: Frame pipeline tracks dirty rectangles per layer instead of boolean dirty flag
- **DMGT-02**: Frame pipeline clears only previously-dirty regions instead of werase on entire layer
- **DMGT-03**: Frame pipeline skips update_panels/doupdate when no draw calls issued

### Layer Transparency

- **TRNS-01**: Developer can mark a layer as transparent so undrawn cells show content from layers below
- **TRNS-02**: Developer can set layer alpha (0.0-1.0) for RGB color blending with layers below

### Input Routing

- **INRT-01**: Input system can route events to an active/focused entity
- **INRT-02**: Developer can set focus on an entity to receive input events

## Out of Scope

| Feature | Reason |
|---------|--------|
| Command buffer / render queue in NCurses | Not needed; Clay has its own, NCurses exposes direct draw calls |
| Clay integration | Separate cels-clay module |
| Widget/component library | Graphics API provides primitives only |
| Animation/tweening | App-level concern |
| Text measurement/layout | Clay handles via MeasureText callback |
| Sixel/Kitty image protocols | Terminal-specific, bypasses character cell model |
| Multi-threaded rendering | ncurses is not thread-safe |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| WIN-01 | TBD | Pending |
| WIN-02 | TBD | Pending |
| WIN-03 | TBD | Pending |
| INP-01 | TBD | Pending |
| INP-02 | TBD | Pending |
| LAYR-01 | TBD | Pending |
| LAYR-02 | TBD | Pending |
| LAYR-03 | TBD | Pending |
| FRAM-01 | TBD | Pending |
| FRAM-02 | TBD | Pending |
| FRAM-03 | TBD | Pending |
| MOD-01 | TBD | Pending |
| MOD-02 | TBD | Pending |
| MOD-03 | TBD | Pending |
| DEMO-01 | TBD | Pending |

**Coverage:**
- v0.2.0 requirements: 15 total
- Mapped to phases: 0
- Unmapped: 15

---
*Requirements defined: 2026-02-26*
*Last updated: 2026-02-26 after initial definition*
