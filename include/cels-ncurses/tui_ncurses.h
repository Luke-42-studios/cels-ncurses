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
 * NCurses Module - Public Component Types and Module Declaration
 *
 * Defines the entity-component API surface for the NCurses module:
 *   - NCurses_WindowConfig: developer sets on entity to configure terminal
 *   - NCurses_WindowState: NCurses fills after terminal init (width, height, etc.)
 *   - CEL_Define(NCursesWindow): composition for natural entity creation
 *   - NCursesWindow() call macro: developer writes NCursesWindow(.title = "X") {}
 *
 * Usage:
 *   #include <cels/cels.h>
 *   #include <cels-ncurses/ncurses.h>
 *
 *   CEL_Composition(World) {
 *       NCursesWindow(.title = "My App", .fps = 60) {}
 *   }
 *
 *   cels_main() {
 *       cels_register(NCurses);
 *       cels_session(World) {
 *           while (cels_running()) cels_step(0);
 *       }
 *   }
 */

#ifndef CELS_NCURSES_TUI_NCURSES_H
#define CELS_NCURSES_TUI_NCURSES_H

#include <cels/cels.h>
#include <stdbool.h>

/* ============================================================================
 * Module Declaration
 * ============================================================================
 *
 * NCurses module entity and init function. The CEL_Module(NCurses) macro
 * in ncurses_module.c defines these symbols. Consumer TUs use the extern
 * declarations here.
 *
 * NCurses_register() is provided as a per-TU static inline so that
 * cels_register(NCurses) works in any translation unit. It delegates to
 * the externally-linked NCurses_init().
 */
extern cels_entity_t NCurses;
extern void NCurses_init(void);

/* Per-TU register function so cels_register(NCurses) works in consumer TUs.
 * Skipped in ncurses_module.c where CEL_Module(NCurses) provides its own. */
#ifndef _NCURSES_MODULE_IMPL
static inline void NCurses_register(void) { NCurses_init(); }
#endif

/* ============================================================================
 * Component Types
 * ============================================================================ */

/*
 * Developer sets this on an entity to configure the terminal window.
 * An observer reacts to this component being added and initializes ncurses.
 *
 *   NCursesWindow(.title = "My App", .fps = 60, .color_mode = 0) {}
 *
 * color_mode: 0=auto, 1=256-color, 2=palette-redef, 3=direct-RGB
 */
CEL_Component(NCurses_WindowConfig) {
    const char* title;
    int fps;
    int color_mode;
};

/*
 * NCurses system attaches this to the window entity after terminal init.
 * Developer reads via cel_watch(entity, NCurses_WindowState) for reactive
 * updates (e.g. terminal resize triggers recomposition).
 */
CEL_Component(NCurses_WindowState) {
    int width;
    int height;
    bool running;
    float actual_fps;
    float delta_time;
};

/*
 * Per-frame input state. NCurses input system fills this each frame at OnLoad.
 * Developer reads via cel_watch(entity, NCurses_InputState) or
 * cels_entity_get_component(entity, NCurses_InputState_id).
 *
 * Per-frame fields reset each frame. Persistent fields (mouse position,
 * held buttons) carry across frames.
 */
CEL_Component(NCurses_InputState) {
    /* Keyboard: directional input (-1.0/0.0/1.0 for x/y) */
    float axis_x;
    float axis_y;

    /* Keyboard: action buttons (per-frame) */
    bool accept;        /* Enter/Return */
    bool cancel;        /* Escape */

    /* Keyboard: navigation (per-frame) */
    bool tab;
    bool shift_tab;
    bool home;
    bool end;
    bool page_up;
    bool page_down;
    bool backspace;
    bool delete_key;

    /* Keyboard: number keys (per-frame) */
    int  number;
    bool has_number;

    /* Keyboard: function keys (per-frame) */
    int  function_key;
    bool has_function;

    /* Keyboard: raw key passthrough (per-frame) */
    int  raw_key;
    bool has_raw_key;

    /* Mouse: position (persistent across frames) */
    int mouse_x;
    int mouse_y;

    /* Mouse: button event (per-frame) */
    int  mouse_button;   /* 0=none, 1=left, 2=middle, 3=right */
    bool mouse_pressed;
    bool mouse_released;

    /* Mouse: held state (persistent across frames) */
    bool mouse_left_held;
    bool mouse_middle_held;
    bool mouse_right_held;
};

/*
 * Override CEL_Component's per-TU static IDs with extern globals.
 * The actual definitions live in ncurses_module.c. This ensures all
 * translation units (including the consumer's) share the same IDs
 * registered during NCurses_init().
 *
 * CEL_Component creates `static cels_entity_t Name_id = 0` per TU.
 * We redirect the symbol via macro to an extern global so all TUs
 * share the same registered ID.
 */
extern cels_entity_t _ncurses_WindowConfig_id;
extern cels_entity_t _ncurses_WindowState_id;
extern cels_entity_t _ncurses_InputState_id;
#define NCurses_WindowConfig_id _ncurses_WindowConfig_id
#define NCurses_WindowState_id  _ncurses_WindowState_id
#define NCurses_InputState_id   _ncurses_InputState_id

/* ============================================================================
 * Composition: NCursesWindow
 * ============================================================================
 *
 * Public composition exported via CEL_Define. Developer creates window
 * entities with natural syntax:
 *
 *   NCursesWindow(.title = "My App", .fps = 60) {}
 *
 * Implementation in ncurses_module.c via CEL_Compose(NCursesWindow).
 */
CEL_Define(NCursesWindow, const char* title; int fps; int color_mode;);

/* Call macro for natural syntax */
#define NCursesWindow(...) cel_init(NCursesWindow, __VA_ARGS__)

/* ============================================================================
 * Console Logging
 * ============================================================================
 *
 * Sends printf-style messages from the ncurses child process back to the
 * parent process (visible in IDE consoles like CLion). Messages are piped
 * via a file descriptor created before fork.
 *
 * No-op when not running inside a spawned terminal.
 */
extern void ncurses_console_log(const char* fmt, ...);

#endif /* CELS_NCURSES_TUI_NCURSES_H */
