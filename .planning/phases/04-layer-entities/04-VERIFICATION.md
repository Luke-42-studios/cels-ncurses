---
phase: 04-layer-entities
verified: 2026-03-14T17:38:20Z
status: passed
score: 4/4 must-haves verified
---

# Phase 4: Layer Entities Verification Report

**Phase Goal:** Developer can create drawable layers by adding TUI_LayerConfig components to entities, with NCurses managing panels internally
**Verified:** 2026-03-14T17:38:20Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Developer creates an entity with TUI_LayerConfig (z_order, visible, dimensions) and a panel/WINDOW is created internally by NCurses | VERIFIED | CEL_Component(TUI_LayerConfig) defined in cels_ncurses.h:82-87 with z_order, visible, x, y, width, height fields. TUILayer composition macro at line 186 creates entities. CEL_Observe(TUI_LayerLC, on_create) in tui_layer_entity.c:176-223 reads config, calls newwin() + new_panel() to create ncurses resources internally. |
| 2 | NCurses attaches a TUI_DrawContext_Component to each layer entity so the developer can retrieve it | VERIFIED | on_create observer builds TUI_DrawContext_Component (tui_layer_entity.c:209-215) with ctx from tui_draw_context_create, panel, win, and subcell_buf=NULL. Calls cels_entity_set_component at line 217-218 AND cels_component_notify_change at line 219. struct TUI_DrawContext_Component defined in cels_ncurses_draw.h:334-340 with ctx, dirty, panel, win, subcell_buf fields. |
| 3 | Developer can get TUI_DrawContext_Component from a layer entity and draw into it using existing draw primitives | VERIFIED | minimal.c uses cel_watch(TUI_DrawContext_Component) at lines 46 and 64 to retrieve the component. Extracts ctx = dc->ctx and calls tui_draw_fill_rect, tui_draw_text, tui_draw_border_rect -- all real draw primitives from cels_ncurses_draw.h. Two layer entities demonstrate this: background (z=0 fullscreen) and overlay (z=10 positioned). |
| 4 | Multiple layer entities stack correctly according to z_order | VERIFIED | Internal registry (g_layer_entries, max 32) in tui_layer_entity.c:70-72 tracks entities with cached z_order. layer_sync_z_order() at lines 134-155 performs insertion sort by z_order ascending then calls top_panel() in order, rebuilding the ncurses panel stack. TUI_LayerSyncSystem (CEL_System at PreRender, lines 257-285) detects runtime z_order and visibility changes each frame. minimal.c creates layers at z=0 and z=10 proving the stacking. |

**Score:** 4/4 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/cels_ncurses.h` | CEL_Component for TUI_Renderable, TUI_LayerConfig, TUI_DrawContext_Component; CEL_Define_Composition + call macro for TUILayer | VERIFIED (198 lines) | TUI_Renderable at line 70, TUI_LayerConfig at line 82, TUI_DrawContext_Component forward-declared at line 97, CEL_Define_Composition(TUILayer) at line 183, #define TUILayer macro at line 186. No stubs. |
| `include/cels_ncurses_draw.h` | struct TUI_DrawContext_Component with ctx, dirty, panel, win, subcell_buf | VERIFIED (609 lines) | struct TUI_DrawContext_Component at lines 334-340 with all five fields. Existing v1.0 TUI_Layer typedef preserved at lines 549-558 -- no collision. |
| `src/layer/tui_layer_entity.c` | Lifecycle, observers, registry, z-order sync, composition, dirty clearing | VERIFIED (355 lines) | CEL_Lifecycle(TUI_LayerLC) at line 174. CEL_Observe on_create at line 176 (creates WINDOW+PANEL, attaches DrawContext). CEL_Observe on_destroy at line 225 (cleanup). LayerEntry registry with add/remove/find helpers. layer_sync_z_order() with insertion sort + top_panel. CEL_System(TUI_LayerSyncSystem) at PreRender. ncurses_layer_entity_clear_dirty() for frame integration. CEL_Composition(TUILayer) wires components + lifecycle. ncurses_register_layer_systems() registration function. Entire file guarded by #ifdef CELS_HAS_ECS. |
| `src/tui_internal.h` | Forward declarations for TUI_LayerLC, TUI_LayerSyncSystem, registration functions | VERIFIED (63 lines) | CEL_Define_Lifecycle(TUI_LayerLC) at line 55. CEL_Define_System(TUI_LayerSyncSystem) at line 56. extern ncurses_register_layer_systems at line 57. extern ncurses_layer_entity_clear_dirty at line 58. |
| `src/ncurses_module.c` | TUI_LayerLC + TUI_LayerSyncSystem registered; ncurses_register_layer_systems() called | VERIFIED (132 lines) | cels_register call at lines 98-100 includes TUI_LayerLC and TUI_LayerSyncSystem. ncurses_register_layer_systems() called at line 102. |
| `src/frame/tui_frame.c` | Entity layer dirty clearing in tui_frame_begin | VERIFIED (244 lines) | ncurses_layer_entity_clear_dirty() called at line 148 inside #ifdef CELS_HAS_ECS guard, after the existing g_layers[] clearing loop. tui_internal.h included at line 44. |
| `CMakeLists.txt` | tui_layer_entity.c in INTERFACE sources | VERIFIED (117 lines) | Line 75: tui_layer_entity.c in target_sources INTERFACE block. draw_test target at lines 108-116 does NOT include tui_layer_entity.c (correct isolation). |
| `examples/minimal.c` | Two TUILayer entities at different z-orders with drawing | VERIFIED (112 lines) | Background layer at z=0 (fullscreen, fill_rect + text) at lines 45-59. Overlay layer at z=10 (positioned 40x12, fill_rect + border_rect + text) at lines 62-88. Input handling via CEL_System(GameInput) with cels_request_quit. Full cels_main with session loop. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| tui_layer_entity.c | cels_ncurses_draw.h | #include for TUI_DrawContext types | WIRED | Line 47: includes cels_ncurses_draw.h. Uses tui_draw_context_create, TUI_DrawContext_Component struct, tui_subcell_buffer_destroy, tui_subcell_buffer_clear. |
| tui_layer_entity.c | cels_ncurses.h | #include for component IDs | WIRED | Line 46: includes cels_ncurses.h. Uses TUI_LayerConfig_id, TUI_DrawContext_Component_id, TUI_Renderable throughout. |
| on_create observer | cels_entity_set_component | Attaches TUI_DrawContext_Component | WIRED | Line 217-218: cels_entity_set_component(entity, TUI_DrawContext_Component_id, &dc, sizeof(dc)). Paired with notify at line 219. |
| CEL_Composition(TUILayer) | cels_lifecycle_bind_entity | Fires on_create after cel_has | WIRED | Line 340: cels_lifecycle_bind_entity(TUI_LayerLC_id, cels_get_current_entity()) comes AFTER both cel_has calls at lines 330-338. Correct ordering. |
| ncurses_module.c | tui_layer_entity.c | Registration call in module init | WIRED | Line 102: ncurses_register_layer_systems() in CEL_Module(NCurses, init) body. |
| tui_frame.c | tui_layer_entity.c | Entity dirty clearing at frame_begin | WIRED | Line 148: ncurses_layer_entity_clear_dirty() inside CELS_HAS_ECS guard. |
| minimal.c | cels_ncurses.h + cels_ncurses_draw.h | TUILayer composition + cel_watch + draw calls | WIRED | Lines 45,62: TUILayer macros. Lines 46,64: cel_watch(TUI_DrawContext_Component). Lines 55-58,73-87: tui_draw_* function calls with real styles and content. |

### Requirements Coverage

| Requirement | Status | Supporting Evidence |
|-------------|--------|---------------------|
| LAYR-01: Developer creates entity with TUI_LayerConfig (z_order, visible, dimensions) | SATISFIED | TUI_LayerConfig CEL_Component defined with all fields. TUILayer composition macro creates entities. on_create observer reacts to create panel/WINDOW. |
| LAYR-02: NCurses creates panel/WINDOW internally and attaches TUI_DrawContext_Component | SATISFIED | on_create observer in tui_layer_entity.c calls newwin(), new_panel(), builds TUI_DrawContext_Component, and calls cels_entity_set_component + cels_component_notify_change. |
| LAYR-03: Developer gets TUI_DrawContext and draws with existing primitives | SATISFIED | minimal.c demonstrates cel_watch(TUI_DrawContext_Component) -> dc->ctx -> tui_draw_fill_rect, tui_draw_text, tui_draw_border_rect. |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | - | - | - | - |

No TODO, FIXME, placeholder, stub, or empty return patterns found in any phase 4 artifact. No #include <flecs.h> in any new file. No references to TUI_Layer_id (the v1.0 struct name) -- all ECS code correctly uses TUI_LayerConfig_id. Old src/layer/tui_layer.c is unmodified (verified via git diff).

### Human Verification Required

### 1. Visual Layer Stacking

**Test:** Build and run the `minimal` target. Observe the terminal output.
**Expected:** A fullscreen background layer filled with '.' characters at z=0, with an overlay box (single-line border, 40x12) positioned at (5,3) rendering on top at z=10 with text "Overlay Layer (z=10)" and "This layer is on top!". Press 'q' to exit cleanly.
**Why human:** Cannot verify visual rendering, correct z-order stacking appearance, or clean exit programmatically.

### 2. Build Verification

**Test:** Run `cmake --build /tmp/cels-ncurses-build --target minimal` and `cmake --build /tmp/cels-ncurses-build --target draw_test`.
**Expected:** Both targets build with zero errors and zero warnings.
**Why human:** Build depends on system state (cels v0.5.3 availability, ncurses-dev installed, CMake cache). Summary claims both built successfully in commit aa699e2.

### Gaps Summary

No gaps found. All four observable truths are verified through code inspection. All artifacts exist, are substantive (well above minimum line counts), and are fully wired into the system. The implementation follows established patterns (CEL_Lifecycle, CEL_Observe, CEL_System, CEL_Composition) consistent with Phase 1.2 window lifecycle. The naming constraint (TUI_LayerConfig instead of TUI_Layer) is correctly applied throughout with no collisions. Frame pipeline integration and module registration are complete. The minimal.c example exercises the full developer workflow.

---
*Verified: 2026-03-14T17:38:20Z*
*Verifier: Claude (gsd-verifier)*
