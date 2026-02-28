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
 * Registers all NCurses component types and calls registration stubs for
 * observers and systems.
 *
 * CEL_Module(NCurses) expands to define:
 *   - cels_entity_t NCurses = 0;   (module entity, file scope)
 *   - void NCurses_init(void)      (idempotent init function)
 *   - NCurses_init_body(void)      (user-provided init body below)
 *
 * CEL_Compose(NCursesWindow) provides the composition body for the
 * NCursesWindow call macro defined in tui_ncurses.h.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * The CEL_Module macro generates non-static globals, so this must be the
 * ONLY file that expands CEL_Module(NCurses). The public header uses
 * extern declarations only.
 */

#include "cels-ncurses/tui_ncurses.h"
#include "cels-ncurses/tui_internal.h"

CEL_Module(NCurses) {
    /* Register component types -- cels_ensure_component deduplicates by name */
    cels_ensure_component(&NCurses_WindowConfig_id, "NCurses_WindowConfig",
                          sizeof(NCurses_WindowConfig), CELS_ALIGNOF(NCurses_WindowConfig));
    cels_ensure_component(&NCurses_WindowState_id, "NCurses_WindowState",
                          sizeof(NCurses_WindowState), CELS_ALIGNOF(NCurses_WindowState));

    /* Register observers (reactive initialization) */
    ncurses_register_window_observer();        /* EcsOnSet NCurses_WindowConfig -> init terminal */
    ncurses_register_window_remove_observer(); /* EcsOnRemove -> clean shutdown */

    /* Register systems (frame pipeline) */
    ncurses_register_input_system();           /* OnLoad: read getch() each frame */
    ncurses_register_frame_systems();          /* PreStore: frame_begin, PostFrame: frame_end */
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
 * fields: .title, .fps, .color_mode. It attaches NCurses_WindowConfig
 * to the current entity. The window observer reacts to this component
 * being set and initializes the ncurses terminal.
 */
CEL_Compose(NCursesWindow) {
    cel_has(NCurses_WindowConfig,
        .title = cel.title,
        .fps = cel.fps,
        .color_mode = cel.color_mode
    );
}

/* ============================================================================
 * Stubs -- replaced by real implementations in later plans/phases
 * ============================================================================
 *
 * These weak symbols allow the module to compile before the real
 * observer/system implementations are wired in. When tui_window.c (Plan 02),
 * tui_input.c (Phase 3), and tui_frame.c provide strong definitions,
 * the linker picks those instead.
 */

/* Window observer stub -- Plan 02 provides real implementation in tui_window.c */
__attribute__((weak))
void ncurses_register_window_observer(void) {}

/* Window remove observer stub -- Plan 02 provides real implementation */
__attribute__((weak))
void ncurses_register_window_remove_observer(void) {}

/* Input system stub -- Phase 3 wires tui_input.c's existing system */
__attribute__((weak))
void ncurses_register_input_system(void) {}

/* Frame systems stub -- Plan 02 wires tui_frame.c's existing systems */
__attribute__((weak))
void ncurses_register_frame_systems(void) {}
