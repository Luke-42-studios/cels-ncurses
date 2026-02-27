# Project Milestones: cels-ncurses

## v1.0 Graphics API (Shipped: 2026-02-08)

**Delivered:** Low-level terminal graphics API with drawing primitives, panel-backed layers, and frame pipeline -- pure graphics backend suitable as render target for cels-clay.

**Phases completed:** 1-5 (15 plans total)

**Key accomplishments:**

- Foundation type system with TUI_Rect, TUI_Style, TUI_DrawContext and RGB-to-xterm-256 color mapping via alloc_pair
- Complete drawing primitives (filled/outlined rect, text, border, lines) with 16-deep scissor stack for nested clipping
- Panel-backed layer system with z-ordering, create/destroy lifecycle, resize handling
- Retained-mode frame pipeline with dirty tracking, CLOCK_MONOTONIC timing, single doupdate() per frame
- Clean module boundary enforced -- pure graphics backend, all app-specific rendering extracted to example app

**Stats:**

- 94 files created/modified
- 3,886 lines of C
- 5 phases, 15 plans, 39 requirements
- 2 days from start to ship

**Git range:** `docs: project research` -> `docs(05): complete integration and migration phase`

---

## v1.1 Enhanced Rendering (Closed: 2026-02-26)

**Delivered:** Mouse input, true color, and sub-cell rendering additions on top of v1.0 monolithic architecture.

**Phases completed:** 6-8 (6 plans total)

**Key accomplishments:**

- Module-local TUI_InputState with raw mouse press/release events (Phase 6)
- Three-tier color system: auto-detect true color, palette redefinition, 256-color fallback (Phase 7)
- Sub-cell rendering: half-block (2x), quadrant (2x2), braille (2x4) with lazy shadow buffers (Phase 8)

**Closed early:** Phases 9 (Damage Tracking) and 10 (Layer Transparency) dropped. Architecture rethink needed -- moving from monolithic Engine module to proper ECS-native module design in v2.0.

**Git range:** Phase 6 through `chore: sync with CELS v0.4 API changes`

---
