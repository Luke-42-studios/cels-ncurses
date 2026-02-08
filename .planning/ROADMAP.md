# Roadmap: cels-ncurses

## Overview

Transform the cels-ncurses module from its current stdscr-based renderer with embedded app-specific widgets into a clean, low-level terminal graphics API. The build is strictly bottom-up: foundational types and color system, then drawing primitives, then panel-backed layers with resize handling, then the frame pipeline that orchestrates it all, and finally extracting app-specific code to leave behind a pure graphics backend suitable as a render target for cels-clay.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation** - Type system, color pair management, draw context, and locale setup
- [ ] **Phase 2: Drawing Primitives** - Rectangle, text, border, line, and scissor drawing functions
- [ ] **Phase 3: Layer System** - Panel-backed named layers with z-ordering and terminal resize handling
- [ ] **Phase 4: Frame Pipeline** - Begin/end frame lifecycle integrated with existing window loop
- [ ] **Phase 5: Integration and Migration** - Extract app-specific code, wire up CELS provider to new API

## Phase Details

### Phase 1: Foundation
**Goal**: Developer has the complete type vocabulary and color system needed by all subsequent drawing code
**Depends on**: Nothing (first phase)
**Requirements**: FNDN-01, FNDN-02, FNDN-03, FNDN-04, FNDN-05, FNDN-06, FNDN-07
**Success Criteria** (what must be TRUE):
  1. Developer can construct a TUI_Style with fg/bg colors and attribute flags, and apply it to an ncurses WINDOW via wattr_set() without attron/attroff state leaks
  2. Color pairs are allocated dynamically from a pool (no hardcoded CP_* constants), and pair indices beyond 256 work correctly via wattr_set()
  3. Developer can create a TUI_DrawContext wrapping a WINDOW* with position, dimensions, and clipping state that serves as the target for all future draw calls
  4. TUI_Rect provides integer cell coordinates and float-to-cell mapping uses floorf for position and ceilf for dimensions
  5. Unicode box-drawing characters render correctly (setlocale called before initscr)
**Plans**: TBD

Plans:
- [ ] 01-01: TUI_Style, TUI_Rect structs and float-to-cell mapping
- [ ] 01-02: Dynamic color pair pool and attribute application
- [ ] 01-03: TUI_DrawContext struct and locale initialization

### Phase 2: Drawing Primitives
**Goal**: Developer can draw any shape, text, or border that a Clay renderer would need, with clipping support
**Depends on**: Phase 1
**Requirements**: DRAW-01, DRAW-02, DRAW-03, DRAW-04, DRAW-05, DRAW-06, DRAW-07, DRAW-08, DRAW-09, DRAW-10, DRAW-11
**Success Criteria** (what must be TRUE):
  1. Developer can draw filled and outlined rectangles at arbitrary cell positions with configurable characters and styles
  2. Developer can render positioned text with style attributes, and bounded text that does not exceed a maximum column width
  3. Developer can draw borders with single, double, or rounded corner styles, with independent per-side control
  4. Developer can push/pop scissor rectangles that correctly restrict drawing to the clipped region, including nested clips that intersect
  5. Developer can draw horizontal and vertical lines with box-drawing characters
**Plans**: TBD

Plans:
- [ ] 02-01: Filled and outlined rectangle drawing
- [ ] 02-02: Text rendering (positioned and bounded)
- [ ] 02-03: Border drawing with box-drawing characters and corner styles
- [ ] 02-04: Scissor/clip region stack (push, pop, nesting)
- [ ] 02-05: Horizontal and vertical line drawing

### Phase 3: Layer System
**Goal**: Developer can manage named, z-ordered drawing surfaces backed by ncurses panels, with correct terminal resize behavior
**Depends on**: Phase 2
**Requirements**: LAYR-01, LAYR-02, LAYR-03, LAYR-04, LAYR-05, LAYR-06, LAYR-07, LAYR-08, RSZE-01, RSZE-02, RSZE-03
**Success Criteria** (what must be TRUE):
  1. Developer can create named layers with position, dimensions, and z-order, and destroy them cleanly (freeing WINDOW and PANEL resources)
  2. Developer can show/hide layers, raise/lower z-order, move layers to new positions, and resize layers -- all via the panel library
  3. Developer can obtain a TUI_DrawContext from any layer and draw into it using Phase 2 primitives
  4. On terminal resize (SIGWINCH detected via KEY_RESIZE), all layers are resized via wresize + replace_panel and WindowState dimensions are updated with observers notified
**Plans**: TBD

Plans:
- [ ] 03-01: Layer create/destroy with PANEL backing and z-order
- [ ] 03-02: Layer operations (show/hide, raise/lower, move, resize)
- [ ] 03-03: Layer-to-DrawContext bridge
- [ ] 03-04: Terminal resize handling (SIGWINCH, layer resize, observer notification)

### Phase 4: Frame Pipeline
**Goal**: Developer uses a begin/end frame lifecycle that orchestrates layer compositing with a single doupdate() per frame
**Depends on**: Phase 3
**Requirements**: FRAM-01, FRAM-02, FRAM-03, FRAM-04, FRAM-05
**Success Criteria** (what must be TRUE):
  1. Developer calls tui_frame_begin() which clears all layers and resets drawing state for the new frame
  2. Developer calls tui_frame_end() which composites all visible layers via update_panels() followed by exactly one doupdate()
  3. No drawing primitive internally calls wrefresh, wnoutrefresh, or doupdate -- all screen updates go through the frame pipeline
  4. Frame pipeline integrates with the existing TUI_Window frame loop, replacing the current wnoutrefresh(stdscr) pattern
**Plans**: TBD

Plans:
- [ ] 04-01: tui_frame_begin and tui_frame_end implementation
- [ ] 04-02: Integration with existing TUI_Window frame loop

### Phase 5: Integration and Migration
**Goal**: Module contains only the graphics API -- all app-specific rendering lives in the example app using the new primitives
**Depends on**: Phase 4
**Requirements**: MIGR-01, MIGR-02, MIGR-03, MIGR-04, MIGR-05
**Success Criteria** (what must be TRUE):
  1. Canvas, Button, Slider, and toggle widget renderers are removed from the module and live in the example app
  2. tui_components.h is no longer a public header of the module, and tui_renderer.c contains only CELS feature/provider registration
  3. Example app renders all its UI using the new graphics API primitives (rectangles, text, borders, layers)
  4. No code in the module or example app writes directly to stdscr after initialization -- all drawing goes through TUI_DrawContext via the graphics API
**Plans**: TBD

Plans:
- [ ] 05-01: Extract app-specific rendering to example app
- [ ] 05-02: Strip tui_renderer.c to CELS registration only
- [ ] 05-03: Verify example app renders correctly with new API, no stdscr usage

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 0/3 | Not started | - |
| 2. Drawing Primitives | 0/5 | Not started | - |
| 3. Layer System | 0/4 | Not started | - |
| 4. Frame Pipeline | 0/2 | Not started | - |
| 5. Integration and Migration | 0/3 | Not started | - |

---
*Roadmap created: 2026-02-07*
*Last updated: 2026-02-07*
