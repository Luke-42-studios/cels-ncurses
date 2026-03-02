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
 * NCurses Module - CEL_Module(NCurses) Definition
 *
 * Single module entry point replacing the old Engine + CelsNcurses facade.
 * Registers all NCurses component types, lifecycle, and systems.
 *
 * CEL_Module(NCurses) expands to define:
 *   - cels_entity_t NCurses = 0;   (module entity, file scope)
 *   - void NCurses_init(void)      (idempotent init function)
 *   - NCurses_init_body(void)      (user-provided init body below)
 *
 * CEL_Lifecycle(NCursesWindowLC) defines the window lifecycle.
 * CEL_Observe hooks handle terminal init/shutdown.
 * CEL_Compose(NCursesWindow) declares both components and binds lifecycle
 * manually AFTER cel_has() so on_create can read config from the entity.
 *
 * NOTE: Lifecycle binding must be manual (not via .lifecycle prop) because
 * auto-bind fires on_create during cels_begin_entity(), before the
 * composition body adds components via cel_has().
 *
 * NOTE: Lifecycle, observers, and composition are in the SAME translation
 * unit because CEL_Lifecycle creates static variables that the observers
 * reference.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * The CEL_Module macro generates non-static globals, so this must be the
 * ONLY file that expands CEL_Module(NCurses). The public header uses
 * extern declarations only.
 */

/* Prevent tui_ncurses.h from defining NCurses_register() --
 * CEL_Module(NCurses) below generates its own. */
#define _NCURSES_MODULE_IMPL
#include "cels-ncurses/tui_ncurses.h"

/* Extern global component IDs shared across all TUs (declared in tui_ncurses.h) */
cels_entity_t _ncurses_WindowConfig_id = 0;
cels_entity_t _ncurses_WindowState_id = 0;
cels_entity_t _ncurses_InputState_id = 0;

#include "cels-ncurses/tui_internal.h"
#include <ncurses.h>

/* ============================================================================
 * Window Lifecycle -- Condition, Lifecycle, Observers
 * ============================================================================
 *
 * NCursesActive condition: returns true when ncurses terminal is active.
 * NCursesWindowLC lifecycle: driven by NCursesActive condition.
 * on_create observer: initializes terminal (does NOT set components).
 * on_destroy observer: shuts down terminal.
 */

CEL_Lifecycle(NCursesWindowLC);

CEL_Observe(NCursesWindowLC, on_create) {
    if (ncurses_window_get_entity() != 0)
        return;
    ncurses_window_set_entity(entity);

    const NCurses_WindowConfig* config = cel_watch(entity, NCurses_WindowConfig);
    if (!config) {
        ncurses_window_set_entity(0);
        return;
    }

    ncurses_terminal_init((NCurses_WindowConfig*)config);
}

CEL_Observe(NCursesWindowLC, on_destroy) {
    (void)entity;
    ncurses_terminal_shutdown();
    ncurses_window_set_entity(0);
}

/* ============================================================================
 * CEL_Module(NCurses) -- Module Init Body
 * ============================================================================ */

CEL_Module(NCurses) {
    cels_register(NCursesWindowLC, NCurses_InputSystem, NCurses_WindowUpdateSystem);
    ncurses_register_frame_systems();
}

/* ============================================================================
 * NCursesInput Composition -- standalone input entity
 * ============================================================================
 *
 * Internal composition created by NCursesWindow. The input entity holds
 * NCurses_InputState which the input system updates each frame via cel_update.
 * cel_has() handles component registration automatically.
 *
 * CEL_Composition (not CEL_Compose) because this is file-local, not exported.
 */
CEL_Composition(NCursesInput) {
    cel_has(NCurses_InputState, .mouse_x = -1, .mouse_y = -1);
}

/* ============================================================================
 * NCursesWindow Composition
 * ============================================================================
 *
 * NCursesWindow(.title = "My App", .fps = 60) {}
 *
 * The call macro in tui_ncurses.h expands to cel_init(NCursesWindow, ...),
 * which calls NCursesWindow_factory -> NCursesWindow_impl.
 *
 * The composition body receives `cel` of type NCursesWindow_props with
 * fields: .title, .fps, .color_mode.
 *
 * Declares BOTH NCurses_WindowConfig and NCurses_WindowState via cel_has,
 * then binds the entity to NCursesWindowLC lifecycle which fires on_create.
 * Also creates the standalone NCursesInput entity for raw input handling.
 *
 * ORDERING: cel_has() MUST come before cels_lifecycle_bind_entity() because
 * the on_create observer reads WindowConfig from the entity.
 */
CEL_Compose(NCursesWindow) {
    cel_has(NCurses_WindowConfig,
        .title = cel.title,
        .fps = cel.fps,
        .color_mode = cel.color_mode
    );
    cel_has(NCurses_WindowState,
        .width = 0,
        .height = 0,
        .running = true,
        .actual_fps = 0,
        .delta_time = 0
    );
    /* Bind lifecycle after components -- fires on_create which reads config. */
    cels_lifecycle_bind_entity(NCursesWindowLC_id, cels_get_current_entity());

    /* Create the standalone input entity */
    cel_init(NCursesInput) {}
}

