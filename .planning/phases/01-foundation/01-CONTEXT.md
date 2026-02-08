# Phase 1: Foundation - Context

**Gathered:** 2026-02-07
**Status:** Ready for planning

<domain>
## Phase Boundary

Type system, color pair management, draw context, and locale setup. Delivers the complete type vocabulary and color system needed by all subsequent drawing code. Does not include drawing primitives, layers, or frame pipeline.

</domain>

<decisions>
## Implementation Decisions

### Style struct design
- TUI_Style stores separate .fg and .bg color fields (not a pre-resolved pair index)
- Color pairs are resolved internally when applying a style — developer never manages pairs directly
- Text attributes specified via bitmask flags (uint32 with TUI_ATTR_BOLD | TUI_ATTR_UNDERLINE pattern)
- Colors specified as RGB values; system maps to nearest available terminal color
- RGB-to-nearest-color mapping happens eagerly at color creation time (not lazily at draw)
- No named color presets — all colors are RGB only

### Draw context ownership
- TUI_DrawContext borrows (does not own) the WINDOW* it wraps — layers/callers manage WINDOW lifetime
- Stack-allocated / value type — caller declares on stack, no malloc/free churn per frame
- Draw functions take TUI_DrawContext as explicit first parameter (no global current context)

### Rect and coordinate mapping
- TUI_Rect stores float values with tui_rect_to_cells() conversion using floorf for position, ceilf for dimensions — matches Clay's float layout model
- Negative x/y coordinates are allowed — rect can be partially or fully off-screen, clipping handles visibility
- Provides helper functions: tui_rect_intersect(), tui_rect_contains(), expand/shrink utilities

### Color pair allocation
- Dynamic pool with LRU recycling when full — developer never sees pair management, system silently evicts oldest unused pair
- Pair pool persists across frames with LRU eviction only when full (not reset each frame) — reduces init_pair churn

### Claude's Discretion
- Default/inherit sentinel for fg/bg colors (whether to support TUI_COLOR_DEFAULT = -1 for partial style overrides)
- Clipping state management on TUI_DrawContext (embedded clip rect vs separate clip stack — informed by Phase 2 scissor needs)
- TUI_Rect representation (x/y/w/h vs x1/y1/x2/y2 — pick what's best for Clay compatibility and intersection math)
- Color pool sizing and initial capacity
- Whether color pool reset-per-frame or persist-across-frames is better for performance

</decisions>

<specifics>
## Specific Ideas

- API should feel appropriate for a game engine frame loop — stack-allocated contexts, no hidden global state, minimal per-frame allocation
- "What's the best for a game engine" was the guiding principle for allocation strategy decisions

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-foundation*
*Context gathered: 2026-02-07*
