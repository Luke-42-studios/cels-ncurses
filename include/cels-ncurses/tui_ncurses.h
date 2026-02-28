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

static inline void NCurses_register(void) { NCurses_init(); }

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

#endif /* CELS_NCURSES_TUI_NCURSES_H */
