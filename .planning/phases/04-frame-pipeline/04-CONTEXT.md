# Phase 4: Frame Pipeline - Context

**Gathered:** 2026-02-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Begin/end frame lifecycle that orchestrates layer compositing with a single doupdate() per frame. Integrates with the existing TUI_Window frame loop via CELS ECS phases. Creates a default background layer and migrates the renderer off stdscr.

</domain>

<decisions>
## Implementation Decisions

### Layer Clearing Behavior
- Retained-mode with dirty tracking (NOT immediate-mode erase-all)
- Layers keep content between frames; only dirty layers get erased
- Auto-mark dirty on any draw call into a layer's DrawContext (bool flag per layer)
- frame_begin auto-erases all dirty layers (werase + style reset)
- `tui_frame_invalidate_all()` forces every layer dirty for full redraw (after resize, mode changes)

### Frame State Management
- Scissor stack auto-reset at frame_begin (prevents leaked clips)
- Assert on misuse: drawing outside a frame, nested frame_begin calls
- Style reset to default on dirty layers at frame_begin (wattr_set to default)
- Frame pipeline owns frame timing data (frame count, delta time, fps) — moved out of TUI_WindowState

### Integration with Existing Loop
- frame_begin registered as ECS system at PreStore (runs before renderer)
- frame_end registered as ECS system at PostFrame (runs after renderer)
- Renderer at OnStore draws between begin/end — no changes to render registration
- Remove update_panels()/doupdate() from tui_window_frame_loop — frame_end handles it
- Frame loop simplifies to: Engine_Progress(delta) + usleep
- Migrate renderer off stdscr in Phase 4 — replace wnoutrefresh(stdscr) with layer-based drawing
- Default fullscreen background layer created at init (z=0) — immediate drawing surface

### Claude's Discretion
- Exact dirty flag implementation (bool in TUI_Layer struct vs separate tracking)
- How to register the ECS systems (direct ecs_system_init vs _CEL macros)
- Default background layer naming and initialization timing
- Whether frame count is uint32 or uint64

</decisions>

<specifics>
## Specific Ideas

- "We need a clear way to build composition and z-index, we will use this for fake3d later"
- Clay surfaces will become the layers in Phase 5 — each Clay_Surface maps to a TUI_Layer
- If no width/height set on a surface, it's fullscreen
- The default background layer in Phase 4 is a stepping stone that Phase 5 replaces with Clay surface layers
- Performance-optimal for ncurses: fewer changed cells = fewer bytes sent to terminal

</specifics>

<deferred>
## Deferred Ideas

- Render passes / composition phases for fake-3D layering — future phase after Phase 5
- Clay_Surface-to-TUI_Layer bridge — Phase 5 (Integration and Migration)

</deferred>

---

*Phase: 04-frame-pipeline*
*Context gathered: 2026-02-08*
