# Phase 4: Layer Entities - Research

**Researched:** 2026-03-14
**Domain:** ECS-driven panel-backed layer management (C99, ncurses panels, CELS framework)
**Confidence:** HIGH

## Summary

Phase 4 transforms the existing monolithic TUI_Layer system (global array + direct function calls) into an entity-driven model where layers are CELS entities with component data. The developer creates entities with TUI_Layer + TUI_Renderable components; NCurses observes this combination and creates the underlying ncurses PANEL/WINDOW, attaching a TUI_DrawContext component so the developer can draw.

The primary technical challenge is mapping the developer's integer z-order values onto ncurses panels, which only support `top_panel()` and `bottom_panel()` -- there is no API for inserting a panel at an arbitrary position. This requires maintaining a sorted registry and rebuilding the panel stack order whenever z-order changes. The existing `tui_layer.c` already contains all the ncurses panel management code (create, destroy, show, hide, raise, lower, resize, move) and will be refactored to work with entity-driven lifecycle instead of direct calls.

The CELS framework patterns needed are well-established from prior phases: CEL_Lifecycle + CEL_Observe for entity lifecycle management, CEL_Component for new component types, cels_entity_set_component + cels_component_notify_change for attaching TUI_DrawContext to entities after panel creation, and CEL_System for per-frame z-order synchronization.

**Primary recommendation:** Define three new components (TUI_Renderable tag, TUI_Layer config, TUI_DrawContext opaque wrapper), one lifecycle with on_create/on_destroy observers, and a z-order synchronization system that sorts layers and rebuilds the ncurses panel stack when z-order values change.

## Standard Stack

### Core

| Library/API | Version | Purpose | Why Standard |
|-------------|---------|---------|--------------|
| ncurses (wide) | System | Terminal I/O, WINDOW creation | Already in use, sole terminal backend |
| ncurses panels | System (libpanelw) | Z-ordered window compositing | Already linked, provides panel stack management |
| CELS framework | v0.5.2 | ECS components, lifecycle, observers, systems | Already the architecture pattern for this project |

### ncurses Panel API (Relevant Functions)

| Function | Signature | Use Case |
|----------|-----------|----------|
| `new_panel` | `PANEL* new_panel(WINDOW* win)` | Create panel for new layer entity |
| `del_panel` | `int del_panel(PANEL* pan)` | Destroy panel on entity destruction |
| `top_panel` | `int top_panel(PANEL* pan)` | Move panel to top of stack |
| `bottom_panel` | `int bottom_panel(PANEL* pan)` | Move panel to bottom of stack |
| `show_panel` | `int show_panel(PANEL* pan)` | Make panel visible |
| `hide_panel` | `int hide_panel(PANEL* pan)` | Hide panel from compositing |
| `move_panel` | `int move_panel(PANEL* pan, int starty, int startx)` | Reposition panel |
| `replace_panel` | `int replace_panel(PANEL* pan, WINDOW* win)` | Update panel after wresize |
| `update_panels` | `void update_panels(void)` | Composite all visible panels |
| `set_panel_userptr` | `int set_panel_userptr(PANEL*, const void*)` | Store back-pointer to layer data |

**CRITICAL LIMITATION:** ncurses panels have NO function for inserting at an arbitrary z-position. Only `top_panel()` and `bottom_panel()` exist. To enforce developer-specified z-order values, we must sort layers and rebuild the stack by calling `bottom_panel()` on each panel in ascending z-order.

### CELS API (Relevant Patterns)

| API | Purpose | Established in Phase |
|-----|---------|---------------------|
| `CEL_Component(Name) { fields };` | Define component struct + registration | Phase 1 |
| `CEL_Lifecycle(Name)` | Define entity lifecycle | Phase 1.2 |
| `CEL_Observe(LC, on_create/on_destroy)` | React to entity lifecycle events | Phase 1.2 |
| `CEL_System(Name, .phase = Phase) { cel_run { body } }` | Define per-frame system | Phase 1.1 |
| `cels_entity_set_component(entity, comp_id, data, size)` | Set component on entity (no flecs.h) | Phase 1.2 |
| `cels_entity_get_component(entity, comp_id)` | Get component from entity (no flecs.h) | Phase 1.2 |
| `cels_component_notify_change(comp_id)` | Trigger reactivity after set | Phase 1.2 |
| `cels_lifecycle_bind_entity(lc_id, entity)` | Bind entity to lifecycle (fires on_create) | Phase 1.2 |
| `cel_has(Component, .field = value)` | Add component in composition body | Phase 1 |
| `cel_watch(entity, Component)` | Reactive read of entity component | Phase 1.2 |
| `cel_read(State)` | Non-reactive read of global state | Phase 3 |
| `cel_mutate(State) { ... }` | Mutate global state with auto-notify | Phase 3 |
| `CEL_Define(Name, fields)` / `CEL_Compose(Name)` | Cross-TU composition (header/source) | Phase 1 |

## Architecture Patterns

### New Component Types

```c
/*
 * Tag component -- marks an entity as something NCurses should manage.
 * Combined with TUI_Layer, triggers panel/WINDOW creation.
 * Empty struct (zero-size tag component in ECS parlance).
 */
CEL_Component(TUI_Renderable) {
    int _unused;  /* C99 requires at least one member */
};

/*
 * Layer configuration -- developer sets this on an entity.
 * NCurses reacts to TUI_Renderable + TUI_Layer combination.
 */
CEL_Component(TUI_Layer) {
    int z_order;        /* Higher = on top. Gaps allowed (0, 10, 100). */
    bool visible;       /* true = show_panel, false = hide_panel */
    int x, y;           /* Position (screen coordinates) */
    int width, height;  /* Dimensions (0,0 = fullscreen) */
};

/*
 * Attached by NCurses after panel creation -- developer reads this.
 * Opaque: developer uses tui_draw_* functions, never accesses WINDOW* directly.
 * Contains the DrawContext plus internal bookkeeping.
 */
CEL_Component(TUI_DrawContext_Component) {
    TUI_DrawContext ctx;   /* The drawable surface */
    bool dirty;            /* Auto-set on draw call, cleared at frame_begin */
    PANEL* panel;          /* Internal: ncurses panel (opaque to developer) */
    WINDOW* win;           /* Internal: ncurses window (opaque to developer) */
    TUI_SubCellBuffer* subcell_buf;  /* Internal: lazy-allocated sub-cell buffer */
};
```

**Design Note on TUI_DrawContext_Component vs TUI_DrawContext:** The existing `TUI_DrawContext` is a lightweight struct (WINDOW*, origin, clip) passed by value. The ECS component wraps it with ownership semantics (panel, window, subcell buffer, dirty flag) that the NCurses system manages internally. The developer accesses the inner `TUI_DrawContext ctx` field for drawing.

### Layer Entity Internal Registry

```c
/*
 * Internal tracking of layer entities for z-order sorting.
 * Replaces the global g_layers[] array from v1.0.
 */
#define TUI_LAYER_ENTITY_MAX 32

typedef struct LayerEntry {
    cels_entity_t entity;   /* The CELS entity */
    int z_order;            /* Cached z_order for sorting */
    PANEL* panel;           /* Direct pointer for fast panel ops */
} LayerEntry;

static LayerEntry g_layer_entries[TUI_LAYER_ENTITY_MAX];
static int g_layer_entry_count = 0;
static bool g_z_order_dirty = false;  /* Set when z_order changes */
```

### Lifecycle Pattern (Matching Window Entity)

```c
/* In ncurses_module.c or new tui_layer_entity.c */

CEL_Lifecycle(TUI_LayerLC);

CEL_Observe(TUI_LayerLC, on_create) {
    /* Read TUI_Layer config from entity */
    const TUI_Layer* config = cels_entity_get_component(entity, TUI_Layer_id);
    if (!config) return;

    /* Create ncurses WINDOW + PANEL */
    int w = config->width > 0 ? config->width : COLS;
    int h = config->height > 0 ? config->height : LINES;
    WINDOW* win = newwin(h, w, config->y, config->x);
    if (!win) return;
    PANEL* panel = new_panel(win);
    if (!panel) { delwin(win); return; }
    set_panel_userptr(panel, (void*)(uintptr_t)entity);

    /* Apply initial visibility */
    if (!config->visible) hide_panel(panel);

    /* Build TUI_DrawContext_Component and attach to entity */
    TUI_DrawContext_Component dc = {
        .ctx = tui_draw_context_create(win, 0, 0, w, h),
        .dirty = false,
        .panel = panel,
        .win = win,
        .subcell_buf = NULL,
    };
    dc.ctx.subcell_buf = &dc.subcell_buf;
    cels_entity_set_component(entity, TUI_DrawContext_Component_id,
                               &dc, sizeof(dc));
    cels_component_notify_change(TUI_DrawContext_Component_id);

    /* Register in internal layer entry list */
    layer_registry_add(entity, config->z_order, panel);
    g_z_order_dirty = true;
}

CEL_Observe(TUI_LayerLC, on_destroy) {
    /* Read DrawContext component to get panel/window */
    const TUI_DrawContext_Component* dc =
        cels_entity_get_component(entity, TUI_DrawContext_Component_id);
    if (!dc) return;

    /* Cleanup ncurses resources */
    if (dc->subcell_buf) tui_subcell_buffer_destroy(dc->subcell_buf);
    if (dc->panel) del_panel(dc->panel);
    if (dc->win) delwin(dc->win);

    /* Remove from registry */
    layer_registry_remove(entity);
}
```

### Z-Order Synchronization

```c
/*
 * Sort layer entries by z_order (ascending) and rebuild panel stack.
 * Called when g_z_order_dirty is set (creation, destruction, z_order change).
 *
 * Algorithm: sort entries, then call bottom_panel() on each in ascending
 * z_order. This places each subsequent panel above the previous one,
 * producing the desired stacking order.
 */
static void layer_sync_z_order(void) {
    if (!g_z_order_dirty || g_layer_entry_count == 0) return;
    g_z_order_dirty = false;

    /* Simple insertion sort (max 32 layers) */
    for (int i = 1; i < g_layer_entry_count; i++) {
        LayerEntry key = g_layer_entries[i];
        int j = i - 1;
        while (j >= 0 && g_layer_entries[j].z_order > key.z_order) {
            g_layer_entries[j + 1] = g_layer_entries[j];
            j--;
        }
        g_layer_entries[j + 1] = key;
    }

    /* Rebuild panel stack: bottom_panel first entry, then top_panel each next */
    for (int i = 0; i < g_layer_entry_count; i++) {
        top_panel(g_layer_entries[i].panel);
    }
}
```

### Composition Pattern for Developer API

```c
/* In cels_ncurses.h (public header) */
CEL_Define_Component(TUI_Renderable);
CEL_Define_Component(TUI_Layer);
CEL_Define_Component(TUI_DrawContext_Component);

CEL_Define(TUILayer, int z_order; bool visible; int x; int y; int width; int height;);
#define TUILayer(...) cel_init(TUILayer, __VA_ARGS__)

/* In ncurses_module.c or tui_layer_entity.c */
CEL_Compose(TUILayer) {
    cel_has(TUI_Renderable);
    cel_has(TUI_Layer,
        .z_order = cel.z_order,
        .visible = cel.visible,  /* default: false if unset -- need true default */
        .x = cel.x,
        .y = cel.y,
        .width = cel.width,
        .height = cel.height
    );
    cels_lifecycle_bind_entity(TUI_LayerLC_id, cels_get_current_entity());
}

/* Developer usage:
 *
 *   TUILayer(.z_order = 0, .visible = true, .width = 80, .height = 24) {
 *       const TUI_DrawContext_Component* dc = cel_watch(TUI_DrawContext_Component);
 *       if (!dc) return;
 *       TUI_DrawContext ctx = dc->ctx;
 *       tui_draw_text(&ctx, 0, 0, "Hello", style);
 *   }
 */
```

### Recommended Source File Organization

```
src/
  ncurses_module.c          # Module init -- registers new layer components + lifecycle
  layer/
    tui_layer.c             # REFACTOR: raw ncurses panel ops (keep for draw_test compat)
    tui_layer_entity.c      # NEW: lifecycle, observers, z-order sync, entity registry
  ...existing files unchanged...
```

### Anti-Patterns to Avoid

- **Direct WINDOW* exposure:** Developer must NEVER get a raw `WINDOW*` from a layer entity. DrawContext is the only interface. This is critical for future abstraction.
- **Global g_layers[] for entity layers:** The existing `g_layers[]` array and `g_layer_count` must NOT be used for entity-driven layers. Entity layers use a separate registry. The old API (`tui_layer_create` etc.) stays for `draw_test` backward compatibility but entity layers bypass it.
- **Lifecycle binding before cel_has:** `cels_lifecycle_bind_entity()` must come AFTER `cel_has(TUI_Layer)` in the composition, because the on_create observer reads the TUI_Layer component from the entity. This was already established in Phase 1.2 (window entity).
- **Setting component data in on_create without notify:** Every `cels_entity_set_component()` must be paired with `cels_component_notify_change()`. Established in Phase 1.2.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Panel z-ordering | Manual z-value tracking in each panel | Internal sorted registry + rebuild stack via `top_panel()` loop | ncurses panels have no insert-at-position API; must rebuild from sorted order |
| Layer WINDOW lifecycle | Manual newwin/delwin calls scattered in app code | CEL_Observe(on_create/on_destroy) handles all ncurses resource management | Entity lifecycle = layer lifecycle; observer pattern ensures cleanup |
| DrawContext creation per frame | Developer creates DrawContext each draw call | NCurses attaches TUI_DrawContext_Component once at entity creation | DrawContext is stable for the entity's lifetime; dirty flag tracks mutations |
| Visibility toggling | Direct show_panel/hide_panel calls | Set `TUI_Layer.visible` on entity; observer/system reacts | ECS-driven: developer changes data, system reacts |
| Dirty tracking per layer | Developer marks layers dirty manually | Auto-set dirty flag when developer calls any tui_draw_* function | Existing pattern from v1.0; dirty flag on the DrawContext component |

**Key insight:** The developer's mental model is "I create an entity with a TUI_Layer component and get back a TUI_DrawContext to draw into." ALL ncurses panel management (newwin, new_panel, del_panel, delwin, show_panel, hide_panel, top_panel, resize, move) is hidden inside NCurses observers and systems. The developer never touches ncurses directly.

## Common Pitfalls

### Pitfall 1: ncurses Panel Stack Has No Insert-at-Position

**What goes wrong:** Developer sets z_order=5 on one layer and z_order=10 on another. Without explicit stack management, panels end up in creation order rather than z-order.
**Why it happens:** ncurses panels only support `top_panel()` and `bottom_panel()`. There is no `set_panel_above()` or equivalent.
**How to avoid:** Maintain a sorted internal registry of (entity, z_order, panel). When any z_order changes or a layer is added/removed, sort the registry and call `top_panel()` on each entry in ascending z_order to rebuild the stack.
**Warning signs:** Layers render in wrong order despite having correct z_order values.

### Pitfall 2: INTERFACE Library Static Variable Scope

**What goes wrong:** Static variables in INTERFACE library sources are per-consumer translation unit, not per-library.
**Why it happens:** cels-ncurses is a CMake INTERFACE library -- its sources compile in the consumer's context. Each .c file is its own translation unit.
**How to avoid:** Use `extern` for shared state (layer registry, z_order dirty flag). Follow the established pattern: extern declaration in internal header, definition in one .c file.
**Warning signs:** Layer registry appears empty in one file but populated in another.

### Pitfall 3: Composition Default Values for bool

**What goes wrong:** `TUI_Layer.visible` defaults to `false` (zero-initialized) when developer writes `TUILayer(.z_order = 0)` without specifying `.visible = true`.
**Why it happens:** C99 designated initializers zero-fill unspecified fields. `false` is the zero value for bool.
**How to avoid:** Either: (a) default visible to true in the observer by treating visible=false + z_order=0 as "use defaults", (b) document that `.visible = true` is required, or (c) use a sentinel value. Recommended: the composition body sets `.visible = true` as default before applying `cel` overrides.
**Warning signs:** Newly created layers are invisible.

### Pitfall 4: Subcell Buffer Pointer After Component Set

**What goes wrong:** `TUI_DrawContext_Component` contains a `TUI_SubCellBuffer*` and the DrawContext has a `TUI_SubCellBuffer**` pointing to it. After `cels_entity_set_component()`, the component data is copied to the ECS store. The `subcell_buf` pointer inside `ctx` would point to the stack-local copy, not the ECS-stored copy.
**Why it happens:** `cels_entity_set_component` copies the struct by value. Pointer members that reference other members of the same struct become dangling.
**How to avoid:** Store the subcell buffer pointer separately in the internal registry, or use a separate allocation for the subcell buffer and store only the pointer in the component. After set_component, re-read the component to get the stored address.
**Warning signs:** Sub-cell drawing crashes or renders incorrectly.

### Pitfall 5: on_create Firing Before Component Data Available

**What goes wrong:** If lifecycle is bound before `cel_has(TUI_Layer)`, the on_create observer fires but `cels_entity_get_component(entity, TUI_Layer_id)` returns NULL.
**Why it happens:** CELS auto-bind fires on_create during `cels_begin_entity()`, before the composition body adds components. This was explicitly noted in ncurses_module.c.
**How to avoid:** Always call `cels_lifecycle_bind_entity()` AFTER `cel_has()` in the composition body. Already established in Phase 1.2.
**Warning signs:** Segfault in on_create observer when reading layer config.

### Pitfall 6: Dirty Flag Reset Timing

**What goes wrong:** Dirty flag is cleared before draw calls run, or not cleared at all, leading to unnecessary redraws or stale content.
**Why it happens:** Frame pipeline (Phase 5) clears dirty layers at frame_begin. If dirty tracking happens in the component, the frame system needs access to all layer entity components.
**How to avoid:** The frame system iterates the internal layer registry (not the ECS) to clear dirty flags. The dirty flag lives in the internal registry or DrawContext component.
**Warning signs:** Layers flicker (cleared every frame even when unchanged) or never clear (stale content accumulates).

## Code Examples

### Developer Creates a Layer Entity

```c
/* In developer's composition body */
CEL_Composition(World) {
    NCursesWindow(.title = "My App", .fps = 60) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        /* Create background layer (fullscreen) */
        TUILayer(.z_order = 0, .visible = true) {
            const TUI_DrawContext_Component* dc = cel_watch(TUI_DrawContext_Component);
            if (!dc) return;
            TUI_DrawContext ctx = dc->ctx;
            tui_draw_fill_rect(&ctx, (TUI_CellRect){0, 0, ctx.width, ctx.height},
                                ' ', bg_style);
        }

        /* Create overlay layer (fixed position/size) */
        TUILayer(.z_order = 10, .visible = true,
                 .x = 5, .y = 2, .width = 30, .height = 10) {
            const TUI_DrawContext_Component* dc = cel_watch(TUI_DrawContext_Component);
            if (!dc) return;
            TUI_DrawContext ctx = dc->ctx;
            tui_draw_border_rect(&ctx, (TUI_CellRect){0, 0, 30, 10},
                                  TUI_BORDER_SINGLE, border_style);
            tui_draw_text(&ctx, 2, 1, "Overlay", text_style);
        }
    }
}
```

### Module Registration (ncurses_module.c additions)

```c
/* In CEL_Module(NCurses, init) body */
CEL_Module(NCurses, init) {
    cels_register(NCurses_WindowState, NCurses_InputState,
                  NCursesWindowLC, NCurses_InputSystem, NCurses_WindowUpdateSystem,
                  /* Phase 4 additions: */
                  TUI_LayerLC);
    ncurses_register_frame_systems();
    ncurses_register_layer_systems();  /* NEW: z-order sync system */
}
```

### Z-Order Rebuild (verified pattern for ncurses panels)

```c
/*
 * Source: ncurses panel(3x) man page -- top_panel places panel at top of stack.
 * By calling top_panel in ascending z_order, each subsequent panel ends up
 * above the previous one, producing correct stacking.
 */
static void rebuild_panel_stack(void) {
    /* Sort entries by z_order (ascending), then by creation order for ties */
    sort_layer_entries();  /* insertion sort, max 32 entries */

    /* Rebuild: top_panel each in order -- last call = highest z */
    for (int i = 0; i < g_layer_entry_count; i++) {
        if (g_layer_entries[i].panel) {
            top_panel(g_layer_entries[i].panel);
        }
    }
}
```

## State of the Art

| Old Approach (v1.0) | Current Approach (v0.2.0 Phase 4) | Impact |
|---------------------|-----------------------------------|--------|
| `tui_layer_create("name", x, y, w, h)` returns `TUI_Layer*` | `TUILayer(.z_order=0, .visible=true)` composition | Developer uses entity API, never touches TUI_Layer* directly |
| Global `g_layers[32]` array with swap-remove | Internal entity registry with sorted z-order | Entity lifecycle drives cleanup; z-order is a data field not stack position |
| `tui_layer_get_draw_context(layer)` per frame | `cel_watch(TUI_DrawContext_Component)` once in composition | Reactive: composition re-runs only when DrawContext changes |
| `tui_layer_raise/lower(layer)` imperative calls | Set `TUI_Layer.z_order` on entity, system reacts | Data-driven z-ordering |
| `tui_layer_show/hide(layer)` imperative calls | Set `TUI_Layer.visible` on entity, system reacts | Data-driven visibility |
| Manual `del_panel` + `delwin` cleanup | Entity destruction fires on_destroy observer automatically | No leaked resources |

**Deprecated/outdated in this phase:**
- `g_layers[]` global array: Kept ONLY for draw_test backward compatibility. Entity layers use separate internal registry.
- `tui_layer_create()` / `tui_layer_destroy()`: Kept for draw_test. Entity layers use lifecycle observers.
- `tui_frame_get_background()`: The concept of a default background layer is removed. Developer creates all layers explicitly (per CONTEXT.md decision).

## Open Questions

1. **DrawContext Component Stability**
   - What we know: `cels_entity_set_component` copies the struct. The DrawContext contains a WINDOW* which is stable (ncurses manages it).
   - What's unclear: Whether `cel_watch(TUI_DrawContext_Component)` returns a pointer into the ECS store that remains valid across frames, or if it might be relocated by the ECS.
   - Recommendation: Treat the pointer as valid for the current composition execution. Re-read via `cel_watch` each time the composition runs. This matches the established pattern for `NCurses_WindowState`.

2. **Per-Layer Scissor Stack**
   - What we know: The existing scissor stack uses static globals in `tui_scissor.c` (per-TU). CONTEXT.md says "each layer has its own scissor stack."
   - What's unclear: Whether to embed the scissor stack in the DrawContext component (large struct) or manage it externally.
   - Recommendation: Keep the global scissor stack for now and reset it when switching layers. Per-layer scissor stacks can be deferred to Phase 5 (Frame Pipeline) or a later enhancement. The developer draws into one layer at a time within a composition body.

3. **Runtime Z-Order and Visibility Changes**
   - What we know: CONTEXT.md says z-order is changeable at runtime via observer on component change. Developer modifies TUI_Layer component data.
   - What's unclear: Whether `cels_entity_set_component` + `cels_component_notify_change` on TUI_Layer triggers the on_create observer again or if a separate mechanism is needed.
   - Recommendation: Use a per-frame system that reads all TUI_Layer components and compares against cached values in the internal registry. If z_order or visible changed, update the registry and mark z_order dirty. This avoids needing change-detection observers and is simpler to implement.

4. **Fullscreen Layer Resize on SIGWINCH**
   - What we know: CONTEXT.md says "fullscreen layers (no explicit size) auto-resize; fixed-size layers unchanged." Terminal resize is already handled in tui_input.c (KEY_RESIZE).
   - What's unclear: How to propagate resize to entity-managed layers. The existing `tui_layer_resize_all()` resizes all layers to terminal dimensions.
   - Recommendation: The z-order sync system checks terminal dimensions and resizes fullscreen layers (width=0, height=0 sentinel) each frame. Fixed-size layers are unchanged.

5. **Backward Compatibility: draw_test Target**
   - What we know: `draw_test` is a standalone target that uses the old `tui_layer_create()` API directly without CELS.
   - What's unclear: Whether Phase 4 changes will break draw_test compilation.
   - Recommendation: Keep the old `tui_layer.c` API unchanged. New entity-driven layer code goes in a new file `tui_layer_entity.c` compiled only when `CELS_HAS_ECS` is defined. draw_test does not define `CELS_HAS_ECS`.

## Sources

### Primary (HIGH confidence)
- `/home/cachy/workspaces/libs/cels-ncurses/src/layer/tui_layer.c` -- existing panel management code (create, destroy, show, hide, raise, lower, move, resize, get_draw_context)
- `/home/cachy/workspaces/libs/cels-ncurses/src/ncurses_module.c` -- established lifecycle/observer/composition pattern (NCursesWindowLC)
- `/home/cachy/workspaces/libs/cels-ncurses/include/cels_ncurses_draw.h` -- TUI_DrawContext struct definition, TUI_Layer struct, existing APIs
- `/home/cachy/workspaces/libs/cels/include/cels/cels.h` -- CEL_Component, CEL_Lifecycle, CEL_Observe, CEL_System, cel_has, cel_watch macros
- `/home/cachy/workspaces/libs/cels/include/cels/private/cels_runtime.h` -- cels_entity_set_component, cels_entity_get_component, cels_component_notify_change signatures
- [ncurses panel(3x) man page](https://invisible-island.net/ncurses/man/panel.3x.html) -- panel API reference confirming no insert-at-position function
- [Panel Library HOWTO](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/panels.html) -- panel z-order management patterns

### Secondary (MEDIUM confidence)
- `/home/cachy/workspaces/libs/cels-ncurses/.planning/phases/04-layer-entities/04-CONTEXT.md` -- user decisions constraining this phase
- `/home/cachy/workspaces/libs/cels-ncurses/.planning/STATE.md` -- accumulated decisions from prior phases
- `/home/cachy/workspaces/libs/cels-ncurses/src/frame/tui_frame.c` -- frame pipeline pattern (dirty tracking, werase, update_panels)

### Tertiary (LOW confidence)
- Web search results on ncurses panel arbitrary z-order sorting -- confirmed no native API exists, workaround via repeated `top_panel()` calls is the standard approach

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries already in use, no new dependencies
- Architecture: HIGH -- patterns directly derived from established Phase 1.2 lifecycle pattern and existing tui_layer.c code
- Pitfalls: HIGH -- identified from actual code review (INTERFACE library static scope, bool defaults, pointer-after-copy)
- Z-order sync: HIGH -- verified ncurses panel API limitations via official man page; rebuild-by-sort is the only approach
- DrawContext stability: MEDIUM -- depends on ECS internals (cel_watch pointer lifetime)

**Research date:** 2026-03-14
**Valid until:** 2026-04-14 (stable domain -- ncurses and CELS APIs are not changing)
