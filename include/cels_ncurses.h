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
 * CELS NCurses - Developer API
 *
 * Single include for application developers using the NCurses module:
 *
 *   #include <cels/cels.h>
 *   #include <cels_ncurses.h>
 *
 * Provides:
 *   - NCurses module registration (cels_register(NCurses))
 *   - Component types (NCurses_WindowConfig)
 *   - State singletons (NCurses_WindowState, NCurses_InputState)
 *   - Composition (NCursesWindow call macro)
 *   - Console logging (ncurses_console_log)
 *
 * Does NOT re-export <cels/cels.h> -- include that separately.
 * For drawing primitives, include <cels_ncurses_draw.h>.
 */

#ifndef CELS_NCURSES_H
#define CELS_NCURSES_H

#include <cels/cels.h>
#include <stdbool.h>

CEL_Module(NCurses);

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

/* ============================================================================
 * State Singletons
 * ============================================================================
 *
 * CEL_State types with cross-TU pointer registry.
 * Writer TUs (tui_window.c / tui_input.c) own the data.
 * Consumers read via cel_read(NCurses_WindowState) / cel_read(NCurses_InputState).
 */

/*
 * Window state singleton. Updated each frame by the NCurses window system.
 * Read via cel_read(NCurses_WindowState) in consumer systems.
 */
CEL_Define_State(NCurses_WindowState) {
    int width;
    int height;
    bool running;
    float actual_fps;
    float delta_time;
};

/*
 * Per-frame raw input state singleton. Updated each frame at OnLoad.
 * Read via cel_read(NCurses_InputState) in consumer systems.
 *
 * Raw keys only -- no semantic interpretation. The developer decides what
 * each key means (quit, accept, navigate, etc.) in their own systems.
 *
 * Per-frame fields reset each frame. Mouse position and held state persist.
 */
CEL_Define_State(NCurses_InputState) {
    /* Keyboard: all keys pressed this frame (per-frame, drained from getch queue) */
    int  keys[16];
    int  key_count;

    /* Mouse: position (persistent across frames) */
    int  mouse_x;
    int  mouse_y;

    /* Mouse: button event (per-frame) */
    int  mouse_button;      /* 0=none, 1=left, 2=middle, 3=right */
    bool mouse_pressed;
    bool mouse_released;

    /* Mouse: held state (persistent across frames) */
    bool mouse_left_held;
    bool mouse_middle_held;
    bool mouse_right_held;
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

/* ============================================================================
 * Console Logging
 * ============================================================================
 *
 * Printf-style logging to stderr (visible in IDE consoles like CLion).
 * Works in all modes: PTY-spawned terminal, CELS_NCURSES_TERMINAL=none,
 * or current terminal fallback.
 */
extern void ncurses_console_log(const char* fmt, ...);

#endif /* CELS_NCURSES_H */
