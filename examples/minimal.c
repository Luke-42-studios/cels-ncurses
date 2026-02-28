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
 * Demonstrates the Phase 1 entity-component API:
 *   - cels_register(NCurses) to load the NCurses module
 *   - NCursesWindow() call macro to create a window entity
 *   - cel_watch() for reactive state reading (recomposes on resize)
 *   - cels_session / cels_running / cels_step for the main loop
 *
 * Opens a blank terminal window at 30fps. Press 'q' or Ctrl+C to exit.
 */

#include <cels/cels.h>
#include <cels-ncurses/ncurses.h>

CEL_Composition(World) {
    cel_entity_t win = NCursesWindow(.title = "Phase 1 Test", .fps = 30) {}

    /* Reactive state read -- recomposes on terminal resize (SIGWINCH) */
    const NCurses_WindowState* state = cel_watch(win, NCurses_WindowState);
    if (state) {
        /* Window dimensions available after observer init */
        (void)state;  /* Phase 1: just proves cel_watch works */
    }
}

cels_main() {
    cels_register(NCurses);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
