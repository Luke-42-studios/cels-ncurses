# Phase 5: Frame Pipeline - Context

**Gathered:** 2026-03-14
**Status:** Ready for planning

<domain>
## Phase Boundary

NCurses owns the frame lifecycle through CELS pipeline phases. Begin-frame and end-frame run as ECS systems, developer render systems run naturally between them at OnRender. Remove legacy layer infrastructure in favor of entity-only layers.

</domain>

<decisions>
## Implementation Decisions

### Phase ordering
- Keep current pipeline: PreRender (framework) -> OnRender (developer draws) -> PostRender (framework compositing)
- TUI_LayerSystem stays at PreRender (sync visibility + z-order before developer draws)
- WindowState is single source of truth for timing (delta_time, fps) — remove duplicate timing from tui_frame_begin
- FPS throttle moves from tui_hook_frame_end to NCurses_WindowUpdateSystem (window system owns all timing)

### Layer clearing strategy
- Framework auto-clears all visible entity layers at PreRender (TUI_LayerSystem)
- Developer always gets a blank canvas — just draw in their OnRender system
- All drawing happens in CEL_Systems, not composition bodies (compositions declare structure only)
- Clearing is internal only — no exposed clear helper for developers
- layers_demo needs updating to match this pattern (move drawing from composition to system)

### Legacy layer cleanup
- Remove legacy g_layers[] array and all functions: tui_layer_create/destroy/show/hide/raise/lower/move/resize/resize_all/get_draw_context
- Delete draw_test example entirely (minimal + layers_demo cover visual testing)
- No automatic background layer — developer creates their own TUILayer(.z_order = 0) if needed
- Remove tui_frame_begin/end functions — inline SIGWINCH blocking and update_panels/doupdate directly into ECS systems
- Delete src/layer/tui_layer.c and src/frame/tui_frame.c (logic absorbed into ECS systems)

### Claude's Discretion
- Exact SIGWINCH blocking placement within system bodies
- Whether tui_layer.c types (TUI_Layer struct) need any backward-compat period
- How to handle the TUI_FrameState global (keep, absorb into WindowState, or remove)

</decisions>

<specifics>
## Specific Ideas

- minimal.c pattern is the reference: CEL_Compose declares structure, CEL_System(Renderer, .phase = OnRender) handles drawing
- cel_query/cel_each in render systems to iterate layers and draw into them

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 05-frame-pipeline*
*Context gathered: 2026-03-14*
