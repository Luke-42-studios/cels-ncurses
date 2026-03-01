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
 * CEL_Lifecycle(NCursesWindowLC) defines the window lifecycle with an
 * active condition. CEL_Observe hooks handle terminal init/shutdown.
 * CEL_Compose(NCursesWindow) declares both components and binds lifecycle.
 *
 * NOTE: Lifecycle, observers, and composition are in the SAME translation
 * unit because CEL_Lifecycle creates static variables (NCursesWindowLC_id)
 * that CEL_Compose references via cels_lifecycle_bind_entity().
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
#include "cels-ncurses/tui_internal.h"
#include <ncurses.h>
#include <stdio.h>

/* ============================================================================
 * Window Lifecycle -- Condition, Lifecycle, Observers
 * ============================================================================
 *
 * NCursesActive condition: returns true when ncurses terminal is active.
 * NCursesWindowLC lifecycle: driven by NCursesActive condition.
 * on_create observer: initializes terminal (does NOT set components).
 * on_destroy observer: shuts down terminal.
 */

CEL_Condition(NCursesActive) {
    return ncurses_window_is_active();
}

CEL_Lifecycle(NCursesWindowLC, .active = &NCursesActive);

CEL_Observe(NCursesWindowLC, on_create) {
    if (ncurses_window_get_entity() != 0) {
        fprintf(stderr, "[NCurses] Warning: only one window entity allowed\n");
        return;
    }
    ncurses_window_set_entity(entity);

    /* Read config from entity using CELS public API */
    const NCurses_WindowConfig* config =
        (const NCurses_WindowConfig*)cels_entity_get_component(entity, NCurses_WindowConfig_id);
    if (!config) {
        fprintf(stderr, "[NCurses] Error: on_create fired but no WindowConfig on entity\n");
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
    /* Register component types */
    cels_ensure_component(&NCurses_WindowConfig_id, "NCurses_WindowConfig",
                          sizeof(NCurses_WindowConfig), CELS_ALIGNOF(NCurses_WindowConfig));
    cels_ensure_component(&NCurses_WindowState_id, "NCurses_WindowState",
                          sizeof(NCurses_WindowState), CELS_ALIGNOF(NCurses_WindowState));

    /* Register lifecycle (observers auto-register via constructor attribute) */
    NCursesWindowLC_register();

    /* Register systems (frame pipeline) */
    ncurses_register_input_system();
    ncurses_register_frame_systems();
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
 *
 * ORDERING: cel_has(WindowConfig) and cel_has(WindowState) MUST come before
 * cels_lifecycle_bind_entity() because on_create reads config from entity.
 */
CEL_Compose(NCursesWindow) {
    cel_has(NCurses_WindowConfig,
        .title = cel.title,
        .fps = cel.fps,
        .color_mode = cel.color_mode
    );
    /* SC5: Declare initial placeholder WindowState in composition.
     * Real values (COLS/LINES) are set by ncurses_window_frame_update() on first frame. */
    cel_has(NCurses_WindowState,
        .width = 0,
        .height = 0,
        .running = true,
        .actual_fps = 0,
        .delta_time = 0
    );
    /* Bind entity to window lifecycle -- fires on_create observer.
     * Uses cels_lifecycle_bind_entity() because cel_lifecycle() does not exist
     * as a CELS composition verb. */
    cels_lifecycle_bind_entity(NCursesWindowLC_id, cels_get_current_entity());
}

/* ============================================================================
 * Stubs -- replaced by real implementations when linked
 * ============================================================================
 *
 * These weak symbols allow the module to compile before the real
 * system implementations are wired in. When tui_input.c and tui_frame.c
 * provide strong definitions, the linker picks those instead.
 */

/* Input system stub -- real implementation in tui_input.c */
__attribute__((weak))
void ncurses_register_input_system(void) {}

/* Frame systems stub -- real implementation in tui_frame.c */
__attribute__((weak))
void ncurses_register_frame_systems(void) {}
