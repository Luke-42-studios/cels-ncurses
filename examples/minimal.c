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
 *   - cel_watch() for reactive state reading (window + input)
 *   - cels_session / cels_running / cels_step for the main loop
 *
 * Opens a terminal window at 30fps showing window state and input events.
 * Press 'q' or Ctrl+C to exit.
 */

#include <cels/cels.h>
#include <cels-ncurses/ncurses.h>
#include <ncurses.h>

CEL_Composition(World) {
    cels_entity_t win = 0;
    NCursesWindow(.title = "Phase 3 Input Test", .fps = 30) {
        win = cels_get_current_entity();
    }

    /* Reactive state reads -- recomposes when state changes */
    const NCurses_WindowState* state = cel_watch(win, NCurses_WindowState);
    const NCurses_InputState* input = cel_watch(win, NCurses_InputState);

    if (state && input) {
        mvprintw(0, 0, "Window: %dx%d  fps: %.1f  dt: %.4f",
                 state->width, state->height,
                 state->actual_fps, state->delta_time);

        mvprintw(1, 0, "Axis: %.0f, %.0f  Mouse: %d,%d",
                 input->axis_x, input->axis_y,
                 input->mouse_x, input->mouse_y);

        if (input->accept)
            mvprintw(2, 0, "ENTER pressed!");
        if (input->cancel)
            mvprintw(2, 20, "ESC pressed!");
        if (input->has_raw_key)
            mvprintw(3, 0, "Raw key: %d ('%c')",
                     input->raw_key,
                     (input->raw_key >= 32 && input->raw_key < 127)
                         ? (char)input->raw_key : '?');
    }
}

cels_main() {
    cels_register(NCurses);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
