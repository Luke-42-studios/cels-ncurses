# Phase 4: Layer Entities - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

Developer creates drawable layers by adding TUI_Layer components to entities. NCurses manages panels internally, attaches TUI_DrawContext for drawing, and stacks layers by z-order. Layers are nestable and support runtime z-order changes and visibility toggling.

</domain>

<decisions>
## Implementation Decisions

### Layer creation API
- Component tree model: entities have multiple components working together
  - **TUI_Renderable** — marks entity as something NCurses should manage (panel/WINDOW creation)
  - **TUI_UIComponent** — layout (position, bounds) + tree structure (parent entity, child ordering)
  - **TUI_Layer** — layer-specific config (z-order, size, visibility)
- NCurses reacts to the combination: sees TUI_Renderable + TUI_Layer → creates panel, attaches TUI_DrawContext
- Layers are nestable — child layers clip to parent bounds via TUI_UIComponent tree structure
- No default background layer — developer explicitly creates all layers in their composition

### DrawContext attachment
- NCurses attaches TUI_DrawContext to layer entities on creation (observer fires immediately)
- Developer accesses via `cel_watch(layer, TUI_DrawContext)` — consistent with WindowState pattern
- DrawContext is opaque — developer uses tui_draw_* functions only, no raw WINDOW* exposure
- Each layer has its own scissor stack — nested clipping is isolated per layer

### Z-order and stacking
- Integer z-order field on TUI_Layer — higher values render on top, gaps allowed (z=0, z=10, z=100)
- Tie-breaking: same z-order → later-created entity renders on top (deterministic)
- Global z-order space — nested child layers participate in the same global ordering
- Z-order is changeable at runtime — NCurses reorders panels via observer on component change

### Layer lifecycle
- Layers ARE entities — no separate layer lifecycle, entity lifecycle drives everything
- Entity created with TUI_Layer → observer fires → panel/WINDOW created
- Entity destroyed → on_destroy observer fires → panel cleaned up automatically
- TUI_Layer component modified → observer reacts → panel updated (z-order, visibility, size)
- Visibility toggle via bool on TUI_Layer — NCurses calls hide_panel/show_panel
- On terminal resize: fullscreen layers (no explicit size) auto-resize; fixed-size layers unchanged

### Carried from v1.0
- Retained-mode with dirty tracking — layers keep content between frames, only dirty layers get erased
- Auto-mark dirty on any draw call into a layer's DrawContext
- werase + style reset on dirty layers at frame_begin

### Claude's Discretion
- Exact component struct fields and naming
- Observer registration approach (CEL_Observe vs direct)
- Panel creation/destruction implementation details
- How dirty tracking integrates with the new per-layer scissor stack
- Whether TUI_Renderable is a tag component or carries data

</decisions>

<specifics>
## Specific Ideas

- Component tree model inspired by typical UI frameworks: Renderable + UIComponent + domain-specific component
- "We need a clear way to build composition and z-index, we will use this for fake3d later"
- Consistent API patterns: cel_watch for DrawContext matches cel_watch for WindowState

</specifics>

<deferred>
## Deferred Ideas

- Render passes / composition phases for fake-3D layering — future phase
- Clay_Surface-to-TUI_Layer bridge — future cels-clay integration
- Proportional resize on SIGWINCH — potential future enhancement

</deferred>

---

*Phase: 04-layer-entities*
*Context gathered: 2026-03-14*
