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
 * NCurses Module - Internal Function Declarations
 *
 * Forward declarations for observer/system registration functions called
 * by the CEL_Module(NCurses) init body. These are implemented in:
 *   - tui_window.c  (window observer, Plan 02)
 *   - tui_input.c   (input system, Phase 3)
 *   - tui_frame.c   (frame pipeline systems)
 *
 * This header is NOT included by consumers -- only by ncurses_module.c.
 */

#ifndef CELS_NCURSES_TUI_INTERNAL_H
#define CELS_NCURSES_TUI_INTERNAL_H

/* Window observer: reacts to NCurses_WindowConfig being added to an entity.
 * Initializes ncurses terminal and attaches NCurses_WindowState. */
extern void ncurses_register_window_observer(void);

/* Window remove observer: reacts to NCurses_WindowConfig being removed.
 * Shuts down ncurses terminal cleanly. */
extern void ncurses_register_window_remove_observer(void);

/* Input system: runs at OnLoad phase, reads getch() each frame,
 * populates input state. */
extern void ncurses_register_input_system(void);

/* Frame pipeline systems: registers frame_begin (PreStore) and
 * frame_end (PostFrame) systems. */
extern void ncurses_register_frame_systems(void);

#endif /* CELS_NCURSES_TUI_INTERNAL_H */
