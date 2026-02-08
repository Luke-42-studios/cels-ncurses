# Requirements: cels-ncurses Graphics API

**Defined:** 2026-02-07
**Core Value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal

## v1 Requirements

### Foundation

- [x] **FNDN-01**: Developer can create a TUI_Style struct with foreground color, background color, and attribute flags (bold, dim, underline, reverse)
- [x] **FNDN-02**: Color system dynamically allocates ncurses color pairs from a pool, replacing hardcoded CP_* constants
- [x] **FNDN-03**: Color system uses wattr_set() for atomic attribute application (no attron/attroff state leaks)
- [x] **FNDN-04**: Developer can create a TUI_DrawContext that wraps an ncurses WINDOW* with position, dimensions, and clipping state
- [x] **FNDN-05**: TUI_Rect struct provides integer cell coordinates (x, y, w, h) for all drawing operations
- [x] **FNDN-06**: Float-to-cell coordinate mapping uses floorf for position and ceilf for dimensions (Clay compatibility)
- [x] **FNDN-07**: setlocale(LC_ALL, "") is called before initscr() to enable Unicode box-drawing characters

### Drawing Primitives

- [ ] **DRAW-01**: Developer can draw a filled rectangle at (x, y) with given width, height, fill character, and style
- [ ] **DRAW-02**: Developer can draw an outlined rectangle using box-drawing characters with configurable border style
- [ ] **DRAW-03**: Developer can render text at a specific (x, y) position with a given style
- [ ] **DRAW-04**: Developer can render bounded text that does not exceed a maximum column width (Clay text bounding)
- [ ] **DRAW-05**: Developer can draw borders with per-side control (left, right, top, bottom independently)
- [ ] **DRAW-06**: Developer can draw borders with single, double, or rounded corner styles
- [ ] **DRAW-07**: Corner radius is treated as boolean per-corner -- radius > 0 uses rounded Unicode box-drawing corners
- [ ] **DRAW-08**: Developer can push a scissor/clip rectangle that restricts all subsequent drawing to that region
- [ ] **DRAW-09**: Developer can pop a scissor/clip rectangle to restore the previous clipping region
- [ ] **DRAW-10**: Scissor regions nest correctly -- inner clip is intersection of parent and child clip rects
- [ ] **DRAW-11**: Developer can draw horizontal and vertical lines with box-drawing characters

### Layer System

- [ ] **LAYR-01**: Developer can create a named layer with position (x, y), dimensions (w, h), and z-order
- [ ] **LAYR-02**: Developer can destroy a layer, freeing its ncurses WINDOW and PANEL resources
- [ ] **LAYR-03**: Developer can show/hide a layer (hidden layers are skipped during compositing)
- [ ] **LAYR-04**: Developer can raise or lower a layer in the z-order stack
- [ ] **LAYR-05**: Developer can move a layer to a new position using move_panel (not mvwin)
- [ ] **LAYR-06**: Developer can resize a layer (wresize + replace_panel)
- [ ] **LAYR-07**: Developer can get a TUI_DrawContext from a layer for drawing into it
- [ ] **LAYR-08**: Layers are backed by ncurses PANEL library for native z-ordered compositing

### Frame Pipeline

- [ ] **FRAM-01**: Developer calls tui_frame_begin() to clear all layers and reset drawing state
- [ ] **FRAM-02**: Developer calls tui_frame_end() which calls update_panels() to composite all layers
- [ ] **FRAM-03**: doupdate() is called exactly once per frame after tui_frame_end() (in existing frame loop)
- [ ] **FRAM-04**: Drawing primitives never call wrefresh/wnoutrefresh/doupdate internally
- [ ] **FRAM-05**: Frame pipeline integrates with existing TUI_Window frame loop (replaces current wnoutrefresh(stdscr) pattern)

### Terminal Resize

- [ ] **RSZE-01**: Terminal resize (SIGWINCH) is detected via KEY_RESIZE from wgetch in input loop
- [ ] **RSZE-02**: On resize, all layer surfaces are resized (wresize + replace_panel for each)
- [ ] **RSZE-03**: On resize, TUI_WindowState dimensions are updated and observers notified

### Migration

- [ ] **MIGR-01**: App-specific rendering code (Canvas, Button, Slider, toggle widget renderers) is moved from module to example app
- [ ] **MIGR-02**: tui_components.h is removed from the module's public headers
- [ ] **MIGR-03**: tui_renderer.c is stripped to CELS feature/provider registration only (no app-specific logic)
- [ ] **MIGR-04**: Example app uses new graphics API primitives for all its rendering
- [ ] **MIGR-05**: All drawing targets WINDOW* via the graphics API (no direct stdscr usage after initialization)

## v2 Requirements

### Performance

- **PERF-01**: Damage tracking / dirty rectangles -- only redraw cells that changed since last frame
- **PERF-02**: Double buffering with frame diff for minimal terminal I/O

### Enhanced Drawing

- **EDRW-01**: Fill character customization for rectangle textures
- **EDRW-02**: Text truncation with ellipsis character when exceeding bounds
- **EDRW-03**: Cell read-back (query character/style at position)

### Debug

- **DBUG-01**: Debug overlay / wireframe mode showing bounding boxes and layer boundaries
- **DBUG-02**: Frame stats (FPS, cells written, cells elided)

### Color

- **COLR-01**: True color support via init_extended_color() for terminals that support it
- **COLR-02**: 8-color fallback mode for limited terminals

## Out of Scope

| Feature | Reason |
|---------|--------|
| Widget library (buttons, sliders, lists) | Graphics API provides primitives, not widgets. Widgets belong in app code or cels-widgets module |
| Layout engine | Clay IS the layout engine. Module receives pre-computed coordinates |
| Clay integration | Separate cels-clay module translates Clay_RenderCommandArray into these primitives |
| Animation / tweening | Application-level concern. API draws static frames |
| Mouse input | v1 is keyboard/gamepad only. Input provider is a separate concern |
| Image / bitmap rendering | Terminal image protocols (Sixel, Kitty) are too terminal-specific |
| Sub-cell rendering / braille | Specialized visualization, complicates coordinate system |
| Scroll state management | Clay manages scroll state. API only clips via scissor |
| Text editing / input fields | Widget-level concern (cursor, selection, clipboard, undo) |
| Windows/macOS support | POSIX/Linux only (existing constraint) |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FNDN-01 | Phase 1 | Complete |
| FNDN-02 | Phase 1 | Complete |
| FNDN-03 | Phase 1 | Complete |
| FNDN-04 | Phase 1 | Complete |
| FNDN-05 | Phase 1 | Complete |
| FNDN-06 | Phase 1 | Complete |
| FNDN-07 | Phase 1 | Complete |
| DRAW-01 | Phase 2 | Pending |
| DRAW-02 | Phase 2 | Pending |
| DRAW-03 | Phase 2 | Pending |
| DRAW-04 | Phase 2 | Pending |
| DRAW-05 | Phase 2 | Pending |
| DRAW-06 | Phase 2 | Pending |
| DRAW-07 | Phase 2 | Pending |
| DRAW-08 | Phase 2 | Pending |
| DRAW-09 | Phase 2 | Pending |
| DRAW-10 | Phase 2 | Pending |
| DRAW-11 | Phase 2 | Pending |
| LAYR-01 | Phase 3 | Pending |
| LAYR-02 | Phase 3 | Pending |
| LAYR-03 | Phase 3 | Pending |
| LAYR-04 | Phase 3 | Pending |
| LAYR-05 | Phase 3 | Pending |
| LAYR-06 | Phase 3 | Pending |
| LAYR-07 | Phase 3 | Pending |
| LAYR-08 | Phase 3 | Pending |
| RSZE-01 | Phase 3 | Pending |
| RSZE-02 | Phase 3 | Pending |
| RSZE-03 | Phase 3 | Pending |
| FRAM-01 | Phase 4 | Pending |
| FRAM-02 | Phase 4 | Pending |
| FRAM-03 | Phase 4 | Pending |
| FRAM-04 | Phase 4 | Pending |
| FRAM-05 | Phase 4 | Pending |
| MIGR-01 | Phase 5 | Pending |
| MIGR-02 | Phase 5 | Pending |
| MIGR-03 | Phase 5 | Pending |
| MIGR-04 | Phase 5 | Pending |
| MIGR-05 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 39 total
- Mapped to phases: 39
- Unmapped: 0

---
*Requirements defined: 2026-02-07*
*Last updated: 2026-02-08 after Phase 1 completion*
