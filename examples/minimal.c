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
 * Minimal CELS NCurses Example
 *
 * Demonstrates the entity-component API:
 *   - cels_register(NCurses) to load the NCurses module
 *   - NCursesWindow() call macro to create a window entity
 *   - CEL_System with cel_query(NCurses_InputState) for raw input
 *   - cels_session / cels_running / cels_step for the main loop
 *
 * Opens a terminal window at 30fps showing raw input events.
 * Press 'q' to exit.
 */

#include <cels/cels.h>
#include <cels-ncurses/ncurses.h>

/* Composition: just creates the window entity */
CEL_Composition(World) {
    NCursesWindow(.title = "Input Test", .fps = 30) {}
}

/* System: reads raw input each frame */
CEL_System(GameInput, .phase = OnUpdate) {
    cel_query(NCurses_InputState);
    cel_each(NCurses_InputState) {
        for (int i = 0; i < NCurses_InputState->key_count; i++) {
            int key = NCurses_InputState->keys[i];

            if (key == 'q' || key == 'Q') {
                cels_request_quit();
                return;
            }

            if (key >= 32 && key < 127)
                ncurses_console_log("Key: '%c' (%d)\n", (char)key, key);
            else
                ncurses_console_log("Key: (%d)\n", key);
        }

        if (NCurses_InputState->mouse_pressed || NCurses_InputState->mouse_released) {
            ncurses_console_log("Mouse: %d,%d btn=%d %s%s\n",
                     NCurses_InputState->mouse_x, NCurses_InputState->mouse_y,
                     NCurses_InputState->mouse_button,
                     NCurses_InputState->mouse_pressed ? "PRESS" : "",
                     NCurses_InputState->mouse_released ? "RELEASE" : "");
        }
    }
}

cels_main() {
    cels_register(NCurses);
    cels_register(GameInput);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
