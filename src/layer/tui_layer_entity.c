/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * TUI Layer Entity - Entity-driven layer lifecycle, registry, and z-order
 *
 * Implements the ECS layer system for Phase 4. Developer creates layer
 * entities via TUILayer(.z_order = 0, .visible = true) {} composition.
 * NCurses manages the underlying ncurses PANEL + WINDOW resources:
 *
 *   - CEL_Lifecycle(TUI_LayerLC) with on_create/on_destroy observers
 *   - Internal registry of layer entities with cached z_order
 *   - Per-frame TUI_LayerSyncSystem detects z_order/visibility changes
 *   - CEL_Compose(TUILayer) wires components and lifecycle
 *
 * The old tui_layer.c API (tui_layer_create etc.) is NOT touched -- it
 * remains for draw_test backward compatibility. Entity layers use a
 * completely separate internal registry.
 *
 * NOTE: This file compiles only when CELS_HAS_ECS is defined (same
 * guard pattern as the ECS section of tui_frame.c). Without CELS_HAS_ECS,
 * the entire file is empty.
 *
 * NOTE: Static variables are safe here because all lifecycle/observer/
 * composition/system code lives in this single translation unit. Since
 * cels-ncurses is a CMake INTERFACE library, each .c file is its own TU
 * compiled in the consumer's context. The registry statics are shared
 * by all code within this file.
 */

#ifdef CELS_HAS_ECS

#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>
#include "../tui_internal.h"
#include <ncurses.h>
#include <panel.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Internal Layer Entity Registry
 * ============================================================================
 *
 * Tracks layer entities for z-order sorting. Separate from the v1.0
 * g_layers[] global array. Max 32 entity layers (same cap as v1.0).
 */

#define TUI_LAYER_ENTITY_MAX 32

typedef struct LayerEntry {
    cels_entity_t entity;
    int z_order;
    PANEL* panel;
} LayerEntry;

static LayerEntry g_layer_entries[TUI_LAYER_ENTITY_MAX];
static int g_layer_entry_count = 0;
static bool g_z_order_dirty = false;

/* ============================================================================
 * Registry Helper Functions
 * ============================================================================ */

/*
 * Add a layer entry to the registry. Marks z_order as dirty so the
 * panel stack is rebuilt on the next sync.
 */
static void layer_registry_add(cels_entity_t entity, int z_order, PANEL* panel) {
    if (g_layer_entry_count >= TUI_LAYER_ENTITY_MAX) return;
    g_layer_entries[g_layer_entry_count] = (LayerEntry){
        .entity = entity,
        .z_order = z_order,
        .panel = panel,
    };
    g_layer_entry_count++;
    g_z_order_dirty = true;
}

/*
 * Remove a layer entry from the registry (swap-remove with last).
 * Marks z_order as dirty.
 */
static void layer_registry_remove(cels_entity_t entity) {
    for (int i = 0; i < g_layer_entry_count; i++) {
        if (g_layer_entries[i].entity == entity) {
            /* Swap-remove: replace with last entry */
            if (i < g_layer_entry_count - 1) {
                g_layer_entries[i] = g_layer_entries[g_layer_entry_count - 1];
            }
            g_layer_entry_count--;
            g_z_order_dirty = true;
            return;
        }
    }
}

/*
 * Find a layer entry by entity. Returns pointer or NULL.
 */
static LayerEntry* layer_registry_find(cels_entity_t entity) {
    for (int i = 0; i < g_layer_entry_count; i++) {
        if (g_layer_entries[i].entity == entity) {
            return &g_layer_entries[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Z-Order Synchronization
 * ============================================================================
 *
 * Sorts layer entries by z_order ascending, then rebuilds the ncurses
 * panel stack via top_panel() calls. Insertion sort is used because
 * max 32 entries and the array is usually nearly-sorted.
 *
 * After sorting, each panel is raised to the top in ascending z_order.
 * The last top_panel() call ends up on top, producing correct stacking.
 */
static void layer_sync_z_order(void) {
    if (!g_z_order_dirty || g_layer_entry_count == 0) return;
    g_z_order_dirty = false;

    /* Insertion sort by z_order ascending (max 32 entries) */
    for (int i = 1; i < g_layer_entry_count; i++) {
        LayerEntry key = g_layer_entries[i];
        int j = i - 1;
        while (j >= 0 && g_layer_entries[j].z_order > key.z_order) {
            g_layer_entries[j + 1] = g_layer_entries[j];
            j--;
        }
        g_layer_entries[j + 1] = key;
    }

    /* Rebuild panel stack: top_panel each in ascending z_order */
    for (int i = 0; i < g_layer_entry_count; i++) {
        if (g_layer_entries[i].panel) {
            top_panel(g_layer_entries[i].panel);
        }
    }
}

/* ============================================================================
 * Layer Lifecycle -- CEL_Lifecycle + CEL_Observe
 * ============================================================================
 *
 * TUI_LayerLC lifecycle: driven by entity creation/destruction.
 *
 * on_create observer:
 *   - Reads TUI_LayerConfig from entity
 *   - Creates ncurses WINDOW + PANEL
 *   - Builds TUI_DrawContext_Component and attaches to entity
 *   - Registers in internal layer registry
 *
 * on_destroy observer:
 *   - Cleans up subcell buffer, panel, window
 *   - Removes from internal registry
 */

CEL_Lifecycle(TUI_LayerLC);

CEL_Observe(TUI_LayerLC, on_create) {
    const TUI_LayerConfig* config = cels_entity_get_component(
        entity, TUI_LayerConfig_id);
    if (!config) return;

    /* Determine dimensions: 0 = fullscreen sentinel */
    int w = config->width > 0 ? config->width : COLS;
    int h = config->height > 0 ? config->height : LINES;

    /* Create ncurses WINDOW + PANEL */
    WINDOW* win = newwin(h, w, config->y, config->x);
    if (!win) return;

    PANEL* panel = new_panel(win);
    if (!panel) {
        delwin(win);
        return;
    }

    /* Store entity ID in panel userptr for reverse lookup */
    set_panel_userptr(panel, (void*)(uintptr_t)entity);

    /* Apply initial visibility */
    if (!config->visible) {
        hide_panel(panel);
    }

    /* Build TUI_DrawContext_Component and attach to entity.
     *
     * ctx.subcell_buf is set to NULL initially. The subcell_buf pointer
     * inside DrawContext would be dangling after cels_entity_set_component
     * copies the struct (Pitfall 4). Sub-cell buffer allocation is lazy
     * and happens in draw functions when needed. */
    TUI_DrawContext_Component dc = {
        .ctx = tui_draw_context_create(win, 0, 0, w, h),
        .dirty = false,
        .panel = panel,
        .win = win,
        .subcell_buf = NULL,
    };

    cels_entity_set_component(entity, TUI_DrawContext_Component_id,
                               &dc, sizeof(dc));
    cels_component_notify_change(TUI_DrawContext_Component_id);

    /* Register in internal layer entry list */
    layer_registry_add(entity, config->z_order, panel);
}

CEL_Observe(TUI_LayerLC, on_destroy) {
    const TUI_DrawContext_Component* dc = cels_entity_get_component(
        entity, TUI_DrawContext_Component_id);
    if (!dc) return;

    /* Cleanup subcell buffer if allocated */
    if (dc->subcell_buf) {
        tui_subcell_buffer_destroy(dc->subcell_buf);
    }

    /* Free panel first, then window (correct order per ncurses docs) */
    if (dc->panel) del_panel(dc->panel);
    if (dc->win) delwin(dc->win);

    /* Remove from registry */
    layer_registry_remove(entity);
}

/* ============================================================================
 * Per-Frame Z-Order Sync System
 * ============================================================================
 *
 * Runs at PreRender to detect z_order and visibility changes on layer
 * entities. Compares each registry entry's cached z_order against the
 * current TUI_LayerConfig component value. Also synchronizes panel
 * visibility with the TUI_LayerConfig.visible field.
 *
 * Uses panel_hidden() from ncurses panels API to compare against the
 * TUI_LayerConfig.visible field -- avoids needing a cached visible
 * flag in the registry.
 */

CEL_System(TUI_LayerSyncSystem, .phase = PreRender) {
    cel_run {
        /* Check each registry entry against current TUI_LayerConfig component */
        for (int i = 0; i < g_layer_entry_count; i++) {
            const TUI_LayerConfig* layer = cels_entity_get_component(
                g_layer_entries[i].entity, TUI_LayerConfig_id);
            if (!layer) continue;

            /* Detect z_order change */
            if (layer->z_order != g_layer_entries[i].z_order) {
                g_layer_entries[i].z_order = layer->z_order;
                g_z_order_dirty = true;
            }

            /* Detect visibility change */
            const TUI_DrawContext_Component* dc = cels_entity_get_component(
                g_layer_entries[i].entity, TUI_DrawContext_Component_id);
            if (dc && dc->panel) {
                if (layer->visible && panel_hidden(dc->panel)) {
                    show_panel(dc->panel);
                } else if (!layer->visible && !panel_hidden(dc->panel)) {
                    hide_panel(dc->panel);
                }
            }
        }

        layer_sync_z_order();
    }
}

/* ============================================================================
 * TUILayer Composition
 * ============================================================================
 *
 * Wires TUI_Renderable + TUI_LayerConfig components onto the entity,
 * then binds TUI_LayerLC lifecycle which fires on_create.
 *
 * ORDERING: cel_has() calls MUST come before cels_lifecycle_bind_entity()
 * because the on_create observer reads TUI_LayerConfig from the entity.
 *
 * NOTE: .visible defaults to false (C99 zero-init) when the developer
 * does not specify it. The developer must pass .visible = true explicitly
 * for the layer to be visible.
 */

CEL_Compose(TUILayer) {
    cel_has(TUI_Renderable, ._unused = 0);
    cel_has(TUI_LayerConfig,
        .z_order = cel.z_order,
        .visible = cel.visible,
        .x = cel.x,
        .y = cel.y,
        .width = cel.width,
        .height = cel.height
    );
    /* Bind lifecycle after components -- fires on_create which reads config */
    cels_lifecycle_bind_entity(TUI_LayerLC_id, cels_get_current_entity());
}

/* ============================================================================
 * Layer Systems Registration
 * ============================================================================
 *
 * Called by CEL_Module(NCurses) init body in ncurses_module.c.
 * Registers the per-frame z-order synchronization system.
 */

void ncurses_register_layer_systems(void) {
    TUI_LayerSyncSystem_register();
}

#endif /* CELS_HAS_ECS */
