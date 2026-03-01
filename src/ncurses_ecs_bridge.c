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
 * NCurses ECS Bridge - Sole flecs.h Contact Point
 *
 * This file is the ONLY source file in cels-ncurses that includes <flecs.h>
 * for observer/component-set functionality. It provides:
 *   - Observer callbacks (on_window_config_set, on_window_config_removed)
 *   - Observer registration (ncurses_register_window_observer, _remove_observer)
 *   - Component-set helper (ncurses_component_set)
 *
 * All other source files use CELS abstractions exclusively.
 */

#include "ncurses_ecs_bridge.h"
#include <cels-ncurses/tui_ncurses.h>
#include <cels-ncurses/tui_window.h>
#include <cels/cels.h>
#include <flecs.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>

/* ============================================================================
 * Bridge Access Functions (defined in tui_window.c)
 * ============================================================================
 *
 * tui_window.c owns g_window_entity (static). The bridge accesses it
 * through these extern accessor functions rather than exposing the static.
 */
extern cels_entity_t ncurses_bridge_get_window_entity(void);
extern void ncurses_bridge_set_window_entity(cels_entity_t entity);
extern void ncurses_terminal_init(NCurses_WindowConfig* config);
extern void ncurses_terminal_shutdown(void);

/* ============================================================================
 * Component Set Helper
 * ============================================================================
 *
 * Wraps ecs_set_id() so no other file needs to include <flecs.h> for
 * component mutation.
 */

void ncurses_component_set(cels_entity_t entity,
                            cels_entity_t component_id,
                            const void* data,
                            size_t size) {
    ecs_world_t* world = cels_get_world(cels_get_context());
    ecs_set_id(world, (ecs_entity_t)entity, (ecs_id_t)component_id, size, data);
}

/* ============================================================================
 * Observer: on_window_config_set (EcsOnSet NCurses_WindowConfig)
 * ============================================================================
 *
 * When a developer creates an entity with NCurses_WindowConfig, this observer
 * fires and initializes the ncurses terminal. It attaches NCurses_WindowState
 * to the same entity with initial dimensions and running state.
 *
 * Single window entity enforced: if g_window_entity is already set, log
 * warning and ignore.
 */

static void on_window_config_set(ecs_iter_t* it) {
    if (ncurses_bridge_get_window_entity() != 0) {
        fprintf(stderr, "[NCurses] Warning: only one window entity allowed, ignoring\n");
        return;
    }

    NCurses_WindowConfig* configs = ecs_field(it, NCurses_WindowConfig, 0);
    for (int i = 0; i < it->count; i++) {
        ncurses_bridge_set_window_entity(it->entities[i]);
        ncurses_terminal_init(&configs[i]);

        NCurses_WindowState state = {
            .width = COLS,
            .height = LINES,
            .running = true,
            .actual_fps = (float)configs[i].fps,
            .delta_time = 1.0f / (configs[i].fps > 0 ? (float)configs[i].fps : 60.0f)
        };
        ncurses_component_set(it->entities[i], NCurses_WindowState_id,
                               &state, sizeof(NCurses_WindowState));
    }
}

/* ============================================================================
 * Observer: on_window_config_removed (EcsOnRemove NCurses_WindowConfig)
 * ============================================================================ */

static void on_window_config_removed(ecs_iter_t* it) {
    (void)it;
    ncurses_terminal_shutdown();
    ncurses_bridge_set_window_entity(0);
}

/* ============================================================================
 * Observer Registration Functions
 * ============================================================================
 *
 * These override the weak stubs in ncurses_module.c.
 * Called from CEL_Module(NCurses) init body in ncurses_module.c.
 */

void ncurses_register_window_observer(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    /* Ensure the component ID is populated in this TU */
    cels_ensure_component(&NCurses_WindowConfig_id, "NCurses_WindowConfig",
                          sizeof(NCurses_WindowConfig), CELS_ALIGNOF(NCurses_WindowConfig));

    ecs_observer_desc_t desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "NCurses_WindowConfigObserver";
    desc.entity = ecs_entity_init(world, &entity_desc);
    desc.query.terms[0].id = NCurses_WindowConfig_id;
    desc.events[0] = EcsOnSet;
    desc.callback = on_window_config_set;
    ecs_observer_init(world, &desc);
}

void ncurses_register_window_remove_observer(void) {
    ecs_world_t* world = cels_get_world(cels_get_context());

    ecs_observer_desc_t desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "NCurses_WindowConfigRemoveObserver";
    desc.entity = ecs_entity_init(world, &entity_desc);
    desc.query.terms[0].id = NCurses_WindowConfig_id;
    desc.events[0] = EcsOnRemove;
    desc.callback = on_window_config_removed;
    ecs_observer_init(world, &desc);
}
