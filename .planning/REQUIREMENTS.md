# Requirements: cels-ncurses v1.1 Enhanced Rendering

**Defined:** 2026-02-20
**Core Value:** Provide a low-level drawing primitive API that a future cels-clay module can target to render Clay UI layouts in the terminal

## v1.1 Requirements

### Mouse Input

- [ ] **MOUS-01**: Developer can detect left, right, and middle mouse button clicks with cell coordinates and modifier keys (shift, ctrl, alt)
- [ ] **MOUS-02**: Developer can detect scroll wheel up/down events with cell coordinates
- [ ] **MOUS-03**: Developer can determine which layer received a mouse event via z-order hit testing (top-to-bottom traversal of visible layers)
- [ ] **MOUS-04**: Developer can detect drag operations via press + motion + release state machine with drag start coordinates and current position
- [ ] **MOUS-05**: Developer can track mouse hover position with motion updates consolidated per frame (final position only)

### Sub-Cell Rendering

- [ ] **CELL-01**: Developer can draw half-block pixels at 2x vertical resolution using upper/lower half block characters (U+2580/U+2584) with independent top/bottom colors per cell
- [ ] **CELL-02**: Developer can draw quadrant block pixels at 2x2 resolution per cell using Unicode quadrant characters (U+2596-U+259F) with fg/bg two-color constraint
- [ ] **CELL-03**: Developer can draw braille pattern pixels at 2x4 resolution per cell using braille characters (U+2800-U+28FF) with read-modify-write compositing for multiple pixels in same cell

### True Color

- [ ] **COLR-01**: Color system uses packed RGB values via init_extended_pair/alloc_pair when terminal supports direct color mode (detected via tigetflag or COLORTERM)
- [ ] **COLR-02**: Color system falls back gracefully to xterm-256 color mapping when direct color is unavailable (existing tui_color_rgb behavior preserved)
- [ ] **COLR-03**: Color mode can be detected via COLORTERM=truecolor environment variable and overridden via TUI_Window configuration

### Damage Tracking

- [ ] **DMGT-01**: Frame pipeline tracks dirty rectangles per layer (TUI_CellRect array) instead of boolean dirty flag, accumulated from draw call clip regions
- [ ] **DMGT-02**: Frame pipeline clears only previously-dirty regions instead of calling werase on entire layer each frame
- [ ] **DMGT-03**: Frame pipeline skips update_panels/doupdate entirely when no draw calls were issued in a frame (zero-change detection)

### Layer Transparency

- [ ] **TRNS-01**: Developer can mark a layer as transparent so undrawn cells show content from layers below (application-level compositing for transparent layers, panel compositing for opaque layers)
- [ ] **TRNS-02**: Developer can set layer-wide transparent background (layer has no default fill, only explicitly drawn cells are visible)
- [ ] **TRNS-03**: Developer can set layer alpha (0.0-1.0) for RGB color blending with layers below during compositing (requires true color mode)
- [ ] **TRNS-04**: Developer can overlay color/attribute changes without replacing base layer characters (character-preserving transparency)

## v2 Requirements

### Enhanced Sub-Cell

- **ECLL-01**: Sextant characters (2x3 resolution, Unicode 13) for middle-ground resolution
- **ECLL-02**: Sub-cell line drawing (Bresenham at braille resolution)

### Performance

- **PERF-01**: Synchronized output protocol (\033[?2026h/l) for tear-free rendering on supported terminals
- **PERF-02**: Color pair pool caching for high-throughput true color applications

### Debug

- **DBUG-01**: Debug overlay showing layer boundaries, dirty regions, and compositing order
- **DBUG-02**: Frame stats (FPS, cells written, cells skipped, damage ratio)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Mouse gesture recognition (swipe/pinch) | Application-level logic built on raw events |
| Custom mouse cursor rendering | Terminal emulator owns cursor rendering |
| Sixel/Kitty image protocols | Bypasses character cell model, terminal-specific |
| Sub-cell text rendering | Text belongs in full cells; sub-cell is for graphical elements |
| Raw ANSI escape sequences for color | Breaks ncurses internal state tracking |
| Per-cell shadow buffer duplicating ncurses | ncurses already tracks changes internally |
| Multi-threaded rendering | ncurses is not thread-safe |
| GPU-style blend modes (multiply, screen) | Only source-over alpha and binary transparency |
| Per-cell alpha in TUI_Style | Transparency is a layer property, not per-draw-call |
| HSL/HSV/LAB color spaces | Utility library concern, not core renderer |
| Widget/component library | Graphics API provides primitives only |
| Clay integration | Separate cels-clay module |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| MOUS-01 | Phase 6 | Pending |
| MOUS-02 | Phase 6 | Pending |
| MOUS-03 | Phase 6 | Pending |
| MOUS-04 | Phase 6 | Pending |
| MOUS-05 | Phase 6 | Pending |
| CELL-01 | Phase 8 | Pending |
| CELL-02 | Phase 8 | Pending |
| CELL-03 | Phase 8 | Pending |
| COLR-01 | Phase 7 | Pending |
| COLR-02 | Phase 7 | Pending |
| COLR-03 | Phase 7 | Pending |
| DMGT-01 | Phase 9 | Pending |
| DMGT-02 | Phase 9 | Pending |
| DMGT-03 | Phase 9 | Pending |
| TRNS-01 | Phase 10 | Pending |
| TRNS-02 | Phase 10 | Pending |
| TRNS-03 | Phase 10 | Pending |
| TRNS-04 | Phase 10 | Pending |

**Coverage:**
- v1.1 requirements: 18 total
- Mapped to phases: 18
- Unmapped: 0

---
*Requirements defined: 2026-02-20*
*Last updated: 2026-02-20 after v1.1 roadmap creation*
