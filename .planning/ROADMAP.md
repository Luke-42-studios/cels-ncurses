# Roadmap: cels-ncurses

## Milestones

- v1.0 Foundation (Phases 1-5) -- SHIPPED 2026-02-20. Window lifecycle, input, drawing primitives, panel-backed layers, frame pipeline. 39 requirements verified, archived.
- v1.1 Enhanced Rendering (Phases 6-10) -- in progress

## Phases

<details>
<summary>v1.0 Foundation (Phases 1-5) - SHIPPED 2026-02-20</summary>

Archived. See git history for v1.0 phase details.
- Phase 1: Foundation Types (3 plans, complete)
- Phase 2: Drawing Primitives (4 plans, complete)
- Phase 3: Layer System (3 plans, complete)
- Phase 4: Frame Pipeline (3 plans, complete)
- Phase 5: Module Boundary (2 plans, complete)

</details>

### v1.1 Enhanced Rendering

**Milestone Goal:** Add mouse input, sub-cell rendering, true color, damage tracking, and layer transparency to transform the renderer from functional to high-fidelity.

**Phase Numbering:**
- Integer phases (6, 7, 8, 9, 10): Planned milestone work
- Decimal phases (e.g., 7.1): Urgent insertions (marked with INSERTED)

- [ ] **Phase 6: Mouse Input** - Click, scroll, drag, hover, and layer-aware hit testing
- [ ] **Phase 7: True Color** - 24-bit RGB via palette redefinition with 256-color fallback
- [ ] **Phase 8: Sub-Cell Rendering** - Half-block, quadrant, and braille drawing primitives
- [ ] **Phase 9: Damage Tracking** - Per-layer dirty rectangles with selective clearing
- [ ] **Phase 10: Layer Transparency** - Binary transparency and alpha blending for layered compositing

## Phase Details

### Phase 6: Mouse Input
**Goal**: Developer can receive and dispatch mouse events with layer-aware coordinate mapping
**Depends on**: Nothing (independent of rendering pipeline)
**Requirements**: MOUS-01, MOUS-02, MOUS-03, MOUS-04, MOUS-05
**Success Criteria** (what must be TRUE):
  1. Developer can click on a layer and receive the click event with layer-local cell coordinates and modifier key state
  2. Developer can scroll the mouse wheel and receive discrete up/down events with cell coordinates
  3. Developer can determine which visible layer was clicked by z-order hit testing (topmost layer wins)
  4. Developer can drag from one cell to another and receive start position, current position, and release events
  5. Developer can read the current mouse hover position each frame (consolidated to final position, no event flood)
**Plans**: TBD

Plans:
- [ ] 06-01: TBD
- [ ] 06-02: TBD

### Phase 7: True Color
**Goal**: Developer can specify exact RGB colors and have them rendered faithfully on capable terminals
**Depends on**: Nothing (independent, but must precede Phase 8)
**Requirements**: COLR-01, COLR-02, COLR-03
**Success Criteria** (what must be TRUE):
  1. Developer can pass RGB values to TUI_Style and see the exact color on a true-color-capable terminal
  2. Developer can run the same code on a 256-color terminal and see the nearest palette match without code changes
  3. Developer can query or override the detected color mode via COLORTERM environment variable or TUI_Window configuration
**Plans**: TBD

Plans:
- [ ] 07-01: TBD
- [ ] 07-02: TBD

### Phase 8: Sub-Cell Rendering
**Goal**: Developer can draw graphical elements at resolutions finer than one character cell
**Depends on**: Phase 7 (true color enables independent top/bottom pixel colors in half-block mode)
**Requirements**: CELL-01, CELL-02, CELL-03
**Success Criteria** (what must be TRUE):
  1. Developer can draw filled rectangles at 2x vertical resolution using half-block characters with independent top and bottom pixel colors
  2. Developer can draw 2x2-resolution pixels per cell using quadrant block characters with a two-color constraint
  3. Developer can plot individual dots at 2x4 resolution using braille characters with read-modify-write compositing for overlapping dots in the same cell
**Plans**: TBD

Plans:
- [ ] 08-01: TBD
- [ ] 08-02: TBD

### Phase 9: Damage Tracking
**Goal**: Frame pipeline redraws only what changed, skipping untouched regions and idle frames
**Depends on**: Phase 8 (all draw functions must exist before instrumenting them)
**Requirements**: DMGT-01, DMGT-02, DMGT-03
**Success Criteria** (what must be TRUE):
  1. Developer can observe that draw calls accumulate dirty rectangles per layer instead of marking the entire layer dirty
  2. Developer can observe that frame clearing erases only previously-dirty regions instead of the entire layer surface
  3. Developer can observe that frames with zero draw calls skip update_panels/doupdate entirely (no terminal I/O on idle frames)
**Plans**: TBD

Plans:
- [ ] 09-01: TBD
- [ ] 09-02: TBD

### Phase 10: Layer Transparency
**Goal**: Developer can create layers where undrawn cells reveal content from layers below
**Depends on**: Phase 9 (damage tracking enables efficient re-compositing of transparent layers)
**Requirements**: TRNS-01, TRNS-02, TRNS-03, TRNS-04
**Success Criteria** (what must be TRUE):
  1. Developer can mark a layer as transparent so that undrawn cells show through to the layer below (binary transparency)
  2. Developer can create a layer with no default fill where only explicitly drawn cells are visible
  3. Developer can set a layer alpha value (0.0-1.0) that blends RGB colors with layers below during compositing (true color mode required)
  4. Developer can overlay color/attribute changes on a lower layer without replacing its characters (character-preserving transparency)
**Plans**: TBD

Plans:
- [ ] 10-01: TBD
- [ ] 10-02: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 6 -> 7 -> 8 -> 9 -> 10

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1-5 | v1.0 | 15/15 | Complete | 2026-02-20 |
| 6. Mouse Input | v1.1 | 0/TBD | Not started | - |
| 7. True Color | v1.1 | 0/TBD | Not started | - |
| 8. Sub-Cell Rendering | v1.1 | 0/TBD | Not started | - |
| 9. Damage Tracking | v1.1 | 0/TBD | Not started | - |
| 10. Layer Transparency | v1.1 | 0/TBD | Not started | - |
