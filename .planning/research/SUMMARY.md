# Project Research Summary

**Project:** cels-ncurses
**Domain:** Terminal graphics API / ncurses rendering engine
**Researched:** 2026-02-07
**Confidence:** HIGH

## Executive Summary

cels-ncurses is a terminal graphics API that treats ncurses as a 2D rendering engine, providing the same mental model as SDL or similar game engine graphics backends but targeting character cells instead of pixels. The research confirms this is a well-understood domain: notcurses, termbox2, and every Clay renderer follow the same layered architecture of drawing context + compositing layers + frame pipeline. No new external dependencies are needed -- ncurses 6.5 plus its bundled panel library provide every required primitive. The only build change is linking `-lpanelw`.

The recommended approach is a bottom-up build: foundational types and color system first, then drawing primitives, then the panel-backed layer system, then the frame pipeline, and finally integration with the existing CELS renderer provider. This order follows the dependency graph directly -- each layer builds on the previous with no circular dependencies. The feature set maps one-to-one onto Clay's render command types (RECTANGLE, TEXT, BORDER, SCISSOR_START/END), confirming the API surface is well-scoped. Nine anti-features are explicitly identified (no widgets, no layout engine, no animation, no mouse input in v1) which keeps scope tight.

The dominant risk is the stdscr-to-panels migration. The current codebase renders everything to stdscr, which is fundamentally incompatible with the panel library's z-ordered compositing. Mixing the two causes visual corruption (ghost characters, flickering, z-order violations). This migration must happen cleanly and completely -- no partial adoption. Secondary risks are the COLOR_PAIR 256-pair truncation bug (silent data corruption) and wide-character column arithmetic (CJK/emoji break layout). All three have straightforward prevention strategies documented in the pitfalls research.

## Key Findings

### Recommended Stack

No new dependencies. ncurses 6.5 (already installed) provides all drawing primitives via WINDOW-targeted `w*` functions. The panel library (part of ncurses distribution, linked with `-lpanelw`) provides native z-ordered layer compositing. Unicode box-drawing characters are available via `WACS_*` constants for single/double/thick borders, with rounded corners constructed via `setcchar()` and Unicode codepoints U+256D-U+2570.

**Core technologies:**
- **ncurses 6.5 (wide-char variant):** All drawing primitives -- `mvwhline`, `mvwaddnstr`, `wborder`, `wattr_set` -- targeting WINDOW objects instead of stdscr
- **ncurses panel library (`-lpanelw`):** Z-ordered layer management via `new_panel`, `update_panels`, `top_panel`, `hide_panel` -- zero custom compositing code needed
- **derwin():** Parent-relative derived windows for clipping/scissoring with zero-copy memory sharing
- **Dynamic color pairs:** `init_pair` with pool-based allocation, `wattr_set` for >256 pair support, `init_extended_pair` for large pair counts

### Expected Features

**Must have (table stakes for Clay compatibility):**
- TS-1: Filled rectangle drawing (most common Clay command)
- TS-2: Bounded text rendering with attribute control
- TS-3: Border drawing with box-drawing characters and per-side control
- TS-4: Color/attribute system (Clay RGBA to terminal color mapping)
- TS-5: Scissor/clipping regions (Clay scroll containers)
- TS-6: Corner radius approximation (rounded box-drawing chars)
- TS-7: Drawing surface wrapping ncurses WINDOW
- TS-8: Layer system with z-order (Clay zIndex field)
- TS-9: Frame-based rendering pipeline
- TS-10: Float-to-cell coordinate mapping

**Should have (differentiators):**
- D-1: Damage tracking / dirty rectangles (largest performance win)
- D-5: Text truncation with ellipsis
- D-6: Terminal resize handling

**Defer (v2+):**
- D-2: Fill character customization
- D-3: Extended Unicode box-drawing sets
- D-7: Debug overlay / wireframe mode
- AF-6: True color support (design API for future addition)

### Architecture Approach

Six components in a clean dependency hierarchy: TUI_DrawContext (per-layer drawing state), TUI_Style + Color System (value-type styles replacing stateful attron/attroff), Drawing Primitives (all taking explicit DrawContext), Layer Manager (thin PANEL wrapper), Frame Pipeline (begin/end lifecycle), and a stripped-down Graphics Provider (CELS feature registration only). The data flow is linear: draw calls apply scissor + style, write to WINDOW via `w*` functions, `update_panels()` composites in z-order, `doupdate()` flushes to terminal.

**Major components:**
1. **TUI_DrawContext** -- Per-layer state: target WINDOW, scissor stack, position offsets, current style
2. **TUI_Style + Color System** -- Value-type style struct, dynamic color pair pool, replaces stateful attron/attroff
3. **Drawing Primitives** -- rect, text, text_n, border, border_ex, hline, vline, fill, push/pop scissor
4. **Layer Manager** -- PANEL-backed create/destroy/show/hide/raise/lower/move/resize
5. **Frame Pipeline** -- begin_frame (erase layers) and end_frame (update_panels) lifecycle
6. **Graphics Provider** -- CELS CEL_DefineFeature registration, frame pipeline hooks

### Critical Pitfalls

1. **Mixing stdscr with panels causes visual corruption** -- Create a dedicated full-screen WINDOW via `newwin()` for the base layer. Never write to stdscr after panel initialization. All primitives must target explicit WINDOW objects.
2. **COLOR_PAIR macro silently truncates beyond 256 pairs** -- Use `wattr_set()` with explicit pair parameter instead of `attron(COLOR_PAIR(n))`. Use `init_extended_pair()` for large pair counts.
3. **Wide characters break column arithmetic** -- Use `wcswidth()`/`wcwidth()` for all column calculations, never `strlen()`. Test with CJK/emoji strings.
4. **Missing setlocale before initscr breaks all Unicode** -- Add `setlocale(LC_ALL, "")` before `initscr()`. Without it, WACS_* box-drawing characters render as garbage.
5. **Panel resize requires replace_panel, not wresize alone** -- On SIGWINCH: `resizeterm()` first, then `wresize` + `replace_panel` + `move_panel` for each panel. Never call `resizeterm()` from signal handler.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Foundation (Types, Color, Locale)

**Rationale:** Everything depends on the type system (TUI_Style, TUI_Rect, TUI_DrawContext) and color pair management. This phase has zero CELS integration -- pure ncurses, fully testable in isolation. Also fixes the setlocale prerequisite that blocks all Unicode rendering.
**Delivers:** Style struct, rect struct, draw context struct, dynamic color pair pool, setlocale fix
**Addresses:** TS-4 (Color System), TS-10 (Coordinate Mapping), TS-7 (Drawing Surface -- struct definition)
**Avoids:** Pitfall #3 (COLOR_PAIR truncation via wattr_set design), Pitfall #5 (setlocale), Pitfall #7 (attr leaks via value-type style), Pitfall #16 (pair 0 immutable -- allocate from index 1)

### Phase 2: Drawing Primitives

**Rationale:** Builds directly on Phase 1 types. Each primitive is independently testable. Dependency order within the phase: single cell -> shapes -> text -> borders. Scissor/clipping rounds out the set.
**Delivers:** All draw functions: rect, text, text_n, border, border_ex, hline, vline, fill, push/pop scissor
**Addresses:** TS-1 (Filled Rect), TS-2 (Text), TS-3 (Borders), TS-5 (Scissor), TS-6 (Corner Radius)
**Avoids:** Pitfall #4 (wide chars -- use wcswidth), Pitfall #9 (subwin not scissors -- software clip stack), Pitfall #14 (WACS constants for borders)

### Phase 3: Layer System

**Rationale:** Requires Phase 2 primitives to draw into layers. Introduces the panel library dependency and the CMake build change. This is where stdscr migration happens -- the most dangerous transition in the project.
**Delivers:** Layer manager (create/destroy/show/hide/raise/lower/move/resize), panel-backed z-ordering
**Addresses:** TS-8 (Layer System)
**Avoids:** Pitfall #1 (stdscr + panels -- dedicated base layer WINDOW), Pitfall #2 (replace_panel on resize), Pitfall #6 (move_panel not mvwin), Pitfall #10 (SIGWINCH via KEY_RESIZE), Pitfall #13 (show_panel vs top_panel semantics)

### Phase 4: Frame Pipeline

**Rationale:** The frame pipeline orchestrates layers and drawing. Depends on Phase 3 layers being functional. Replaces the current stdscr-based refresh path with `update_panels()` + `doupdate()`.
**Delivers:** tui_frame_begin/end lifecycle, integration point for existing window frame loop
**Addresses:** TS-9 (Frame Pipeline)
**Avoids:** Pitfall #8 (werase not wclear), Pitfall #11 (frame timing via clock_gettime), Pitfall #15 (single doupdate per frame), Pitfall #17 (drain input buffer)

### Phase 5: CELS Integration and Migration

**Rationale:** Must come last -- requires the complete graphics API. Refactors the existing renderer provider to use the new API. Extracts app-specific code (Canvas, Button, Slider) to the example app.
**Delivers:** Clean renderer provider (CELS feature registration only), example app using new graphics API
**Addresses:** Module cleanup, app-specific code extraction
**Avoids:** Pitfall #12 (static globals in INTERFACE headers -- extern declarations only), ECS Pitfall A (primitives ECS-agnostic), ECS Pitfall B (graphics API owns layer lifetime)

### Phase Ordering Rationale

- **Strict dependency chain:** Each phase builds on the previous. The dependency graph (TS-10 -> TS-4 -> TS-7 -> TS-1/2/3 -> TS-5 -> TS-8 -> TS-9) maps directly to Phases 1-4. No phase can be parallelized or reordered.
- **Risk frontloading:** The stdscr-to-panels migration (Phase 3) is the riskiest change. By completing primitives first (Phase 2), Phase 3 can test layer compositing with real drawing content, catching issues early.
- **Integration last:** Phase 5 touches the most existing code and has the highest blast radius. Deferring it until the API is complete and tested minimizes rework.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 3 (Layer System):** The panel library's transparency handling for overlapping panels with empty cells needs testing. Pad + panel compositing for scroll containers (`copywin()` approach) needs profiling to validate performance.
- **Phase 5 (CELS Integration):** The INTERFACE library model means `static` globals in `.c` files are per-translation-unit -- the layer manager's global state needs careful verification to ensure it works correctly across TUs.

Phases with standard patterns (skip research-phase):
- **Phase 1 (Foundation):** Struct definitions and color pair management are well-documented in ncurses man pages. No unknowns.
- **Phase 2 (Drawing Primitives):** Every function maps directly to documented ncurses APIs. Well-trodden ground.
- **Phase 4 (Frame Pipeline):** `update_panels()` + `doupdate()` is the canonical ncurses panel frame loop. No research needed.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | ncurses 6.5 man pages verified directly. No new dependencies. Panel library confirmed bundled. |
| Features | HIGH | Feature set derived from Clay render command types (authoritative source). Anti-features clearly scoped. |
| Architecture | MEDIUM-HIGH | Pattern validated against notcurses/termbox2/Clay renderers. INTERFACE library global state needs TU verification. |
| Pitfalls | HIGH | All pitfalls sourced from ncurses 6.5 man pages and direct codebase inspection. Prevention strategies are concrete. |

**Overall confidence:** HIGH

### Gaps to Address

- **Wide-character text measurement:** Clay integration requires measuring text width in terminal cells. `wcswidth()` is the right function, but interaction with Clay's `MeasureText` callback needs validation during Phase 2 implementation.
- **Pad + panel compositing:** Scroll containers may need ncurses pads (larger-than-screen buffers), but pads cannot be panel-managed. The `copywin()` blit approach needs performance validation in Phase 3.
- **Terminal resize end-to-end:** The `SIGWINCH` -> `resizeterm()` -> `wresize()` -> `replace_panel()` chain is documented individually but the full sequence under CELS frame loop needs integration testing in Phase 3/4.
- **INTERFACE library static globals:** The layer manager needs global state (panel list). With an INTERFACE CMake library, each TU gets its own copy of statics. Must use extern declarations with a single definition in one `.c` file. Needs validation during Phase 3.

## Sources

### Primary (HIGH confidence)
- ncurses 6.5 man pages: panel(3X), curs_color(3X), curs_attr(3X), curs_refresh(3X), curs_addch(3X), curs_add_wch(3X), curs_border(3X), curs_touch(3X), resizeterm(3X), wresize(3X), new_pair(3X), ncurses(3X), wcwidth(3)
- Direct codebase inspection of existing cels-ncurses module
- Clay SDL3 renderer source (render command type reference)
- .planning/codebase/CONCERNS.md

### Secondary (MEDIUM confidence)
- notcurses architecture (planes/cells/channels model) -- validated pattern
- termbox2 architecture (cell buffer/present model) -- validated pattern

---
*Research completed: 2026-02-07*
*Ready for roadmap: yes*
