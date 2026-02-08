# Phase 5: Integration and Migration - Context

**Gathered:** 2026-02-08
**Status:** Ready for planning

<domain>
## Phase Boundary

Extract all app-specific rendering code (widgets, renderer provider, color constants) from the cels-ncurses module into the example app. Migrate all drawing from stdscr to DrawContext via the Phase 1-4 graphics API. The module becomes a pure graphics backend with no app-specific code.

</domain>

<decisions>
## Implementation Decisions

### Widget extraction strategy
- Per-widget folder structure: `examples/widgets/button.h`, `examples/widgets/slider.h`, etc.
- Each widget file is a full self-contained unit: ECS component definitions + render function + composition helper
- Widget render functions take explicit (x, y) coordinates from the caller -- caller manages layout
- Explicit positioning supports future animation systems that can modify x/y/z-index
- Compositions (Button(), Slider()) bundled in the widget file, not left in app.c

### Color migration
- Old CP_* constants and init_pair() calls removed entirely from the module -- clean break
- Example app defines TUI_Style constants using tui_color_rgb() and the Phase 1 color system
- Shared theme.h file in the example app holds all style constants
- Semantic style names: theme_highlight, theme_active, theme_inactive, theme_muted, theme_accent (not widget-specific names)
- Widgets reference shared theme styles

### Renderer provider removal
- tui_renderer.c and tui_renderer.h removed entirely from the module
- tui_components.h removed entirely from the module
- The example app registers its own CELS render provider -- module no longer provides one
- TUI_Engine_use() no longer calls tui_renderer_init() -- app wires its own renderer
- Module provides: window management, input, frame pipeline. App provides: rendering
- App redraws every frame (no dirty optimization in app code) -- frame pipeline layer-level dirty tracking is sufficient

### stdscr teardown
- stdscr retained only for input: keypad(stdscr), nodelay(stdscr), wgetch(stdscr)
- All drawing goes through DrawContext on layer WINDOWs
- overwrite(stdscr, bg->win) bridge removed completely
- werase(stdscr) in renderer removed completely
- Runtime assert in debug builds to catch stdscr writes after initialization -- safety net for regressions

### Claude's Discretion
- Header-only vs header+source (.h/.c) file pattern for widget files
- Layout positioning approach in example app (inline computation vs layout helper struct)
- Exact widget file naming conventions
- How the debug stdscr-write assert is implemented (macro wrapper, LD_PRELOAD, or other technique)

</decisions>

<specifics>
## Specific Ideas

- Per-widget folder structure chosen with future cels-widget module in mind -- each widget file migrates cleanly to a module later
- Theme semantic names (highlight, active, muted) align with CSS/Compose theming patterns for eventual style inheritance system
- Widget render functions accepting explicit (x, y) prepares for Clay integration -- Clay computes positions via layout engine, passes BoundingBox coordinates to render functions
- Input stays as raw as possible on stdscr -- keyboard, mouse, controller signals captured and sent to CELS system phases where developers build their logic

</specifics>

<deferred>
## Deferred Ideas

- **cels-widget module** -- Cross-engine widget definitions that support per-engine render functions (ncurses, Godot, SDL). Widgets as a reusable abstraction layer across rendering backends. Own module and milestone.
- **Style inheritance system** -- Styles set at surface/canvas level, children inherit unless overridden. Research CSS cascade, Android Compose MaterialTheme patterns. Integrates with ECS/CELS. Candidate for cels-widget module or dedicated phase.
- **Animation components** -- Future system that can modify widget x/y/z-index for animations. Widget explicit positioning decision supports this path.

</deferred>

---

*Phase: 05-integration-and-migration*
*Context gathered: 2026-02-08*
