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
#include <ncurses.h>

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

            mvprintw(0, 0, "Keys this frame: %d  ", NCurses_InputState->key_count);

            if (key >= 32 && key < 127)
                mvprintw(1, 0, "Key: '%c' (%d)       ", (char)key, key);
            else
                mvprintw(1, 0, "Key: (%d)            ", key);
        }

        mvprintw(2, 0, "Mouse: %d,%d  btn=%d  %s%s     ",
                 NCurses_InputState->mouse_x, NCurses_InputState->mouse_y,
                 NCurses_InputState->mouse_button,
                 NCurses_InputState->mouse_pressed ? "PRESS " : "",
                 NCurses_InputState->mouse_released ? "RELEASE " : "");

        mvprintw(3, 0, "Held: L=%d M=%d R=%d",
                 NCurses_InputState->mouse_left_held,
                 NCurses_InputState->mouse_middle_held,
                 NCurses_InputState->mouse_right_held);
    }
}

cels_main() {
    cels_register(NCurses);
    cels_register(GameInput);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
