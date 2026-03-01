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
 * NCurses ECS Bridge - Internal Header
 *
 * Declares the bridge API for observer registration and component-set
 * operations. The implementation in ncurses_ecs_bridge.c is the ONLY
 * file that includes <flecs.h> for these operations.
 *
 * All other source files use these bridge functions instead of raw
 * flecs API calls.
 */

#ifndef CELS_NCURSES_ECS_BRIDGE_H
#define CELS_NCURSES_ECS_BRIDGE_H

#include <cels/cels.h>
#include <stddef.h>

/* Event types for observer registration */
typedef enum {
    NCURSES_EVENT_ON_SET = 0,
    NCURSES_EVENT_ON_REMOVE = 1
} ncurses_event_type_t;

/* Set a component on an arbitrary entity (wraps ecs_set_id).
 * Use this instead of raw ecs_set_id() -- this is the ONLY
 * code path that calls into flecs for component mutation. */
void ncurses_component_set(cels_entity_t entity,
                            cels_entity_t component_id,
                            const void* data,
                            size_t size);

#endif /* CELS_NCURSES_ECS_BRIDGE_H */
