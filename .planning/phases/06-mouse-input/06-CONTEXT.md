# Phase 6: Mouse Input - Context

**Gathered:** 2026-02-20
**Status:** Ready for planning

<domain>
## Phase Boundary

Expose raw mouse state and button events through the existing input system. The module provides position polling and press/release events — no higher-level logic (hit testing, drag detection, hover enter/leave). Those responsibilities belong to consumers (cels-clay or application code).

</domain>

<decisions>
## Implementation Decisions

### Mouse position
- Pollable field, not move events — developer reads current mouse position on demand
- No mouse-move event flood in the input queue
- Position reported in cell coordinates

### Button events
- Left, middle, right buttons only — no scroll wheel
- Press and release as separate events (enables consumers to build drag detection)
- No modifier key state (no Ctrl+click, Shift+click)

### Integration
- Mouse events are a new event type in the existing TUI_Input system
- Same polling path as keyboard input — not a separate API

### Scope reduction
- Roadmap MOUS-03 (z-order hit testing), MOUS-04 (drag), MOUS-05 (hover consolidation) are deferred
- This phase delivers raw mouse primitives only
- Higher-level mouse logic is a consumer responsibility

### Claude's Discretion
- ncurses mouse mask configuration (mmask_t flags)
- Mouse event struct layout
- How position polling integrates with the input loop
- Whether to expose button state (currently held) alongside events

</decisions>

<specifics>
## Specific Ideas

- "We should just know the location of the mouse and then events for pressing mouse button, something else will do this logic later"
- Consistent with project philosophy: cels-ncurses is a low-level drawing primitive API, not a UI framework

</specifics>

<deferred>
## Deferred Ideas

- Layer-aware hit testing (MOUS-03) — consumer responsibility or future phase
- Drag detection with start/current/release positions (MOUS-04) — consumer responsibility
- Hover consolidation and enter/leave events (MOUS-05) — consumer responsibility
- Scroll wheel events — could add later if needed

</deferred>

---

*Phase: 06-mouse-input*
*Context gathered: 2026-02-20*
