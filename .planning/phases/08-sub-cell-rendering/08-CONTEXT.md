# Phase 8: Sub-Cell Rendering - Context

**Gathered:** 2026-02-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Drawing graphical elements at resolutions finer than one character cell. Three modes: half-block (2x vertical), quadrant (2x2), and braille (2x4). This phase adds the drawing primitives and shadow buffer infrastructure. Damage tracking and layer transparency are separate phases.

</domain>

<decisions>
## Implementation Decisions

### Drawing API shape
- Pixel-coordinate API: callers think in virtual pixels, library converts to cells and sub-cells internally
- Functions live on DrawContext (consistent with existing draw API)
- Per-call mode selection: `tui_draw_halfblock_rect()`, `tui_draw_braille_plot()`, etc. — no stateful mode switching
- Both plot and rect primitives for all modes: single-pixel `plot(x,y)` and area `filled_rect(x,y,w,h)`

### Color handling
- One color per plot/rect call + transparent other half — drawing top half preserves bottom half's existing color
- Sub-cell functions accept TUI_Style (consistent with existing draw functions, reuses Phase 7 true color system)
- Quadrant two-color constraint is exposed: library tracks which two colors occupy each cell, caller is aware of the limit
- Auto fallback on 256-color terminals — uses Phase 7 nearest-match transparently, no warnings

### Compositing behavior
- Shadow buffer for braille: per-layer grid tracks dot states, no reliance on mvin_wch
- Half-block merge: drawing one half preserves the other half's color (independent top/bottom drawing across separate calls)
- Scissor system respected: all sub-cell draws clip to the active scissor rect
- Last mode wins on mode mixing: drawing braille over a half-block cell replaces it entirely; each cell is one mode at a time

### Primitive scope
- Every mode gets: plot(x,y) and filled_rect(x,y,w,h) — lines can be built from plot by the caller
- Resolution query function: `tui_draw_subcell_resolution(ctx, mode, &width, &height)` — renderers like Clay use this for viewport calculation
- Shadow buffer resets on frame begin (existing werase path) — no special sub-cell clear function needed
- Braille supports both plot and unplot: turn individual dots on or off without clearing the whole cell

### Claude's Discretion
- Shadow buffer data structure and memory management
- Exact Unicode codepoint selection for quadrant characters
- Internal cell-mapping algorithm details
- Half-block shadow buffer format (if needed beyond braille)

</decisions>

<specifics>
## Specific Ideas

- "Make it like a renderer" — API should feel like raylib or similar. Developers draw with pixels, not cells.
- Easy for engines like Clay to target: pixel coordinates + resolution query = Clay can calculate its logical viewport and draw filled rects
- Update draw_test example with sub-cell rendering demos (half-block, quadrant, braille) — same pattern as Phase 7's draw_test integration

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 08-sub-cell-rendering*
*Context gathered: 2026-02-21*
