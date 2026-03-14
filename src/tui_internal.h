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
 * Forward declarations for lifecycle accessors and system registration
 * functions called by ncurses_module.c. Implementations live in:
 *   - tui_window.c  (terminal init/shutdown, window accessors)
 *   - tui_input.c   (input system registration)
 *   - tui_frame.c   (frame pipeline systems registration)
 *
 * This header is NOT included by consumers -- only by ncurses_module.c.
 */

#ifndef CELS_NCURSES_TUI_INTERNAL_H
#define CELS_NCURSES_TUI_INTERNAL_H

#include <cels_ncurses.h>
#include <stdbool.h>

/* Cross-TU ID overrides are in cels_ncurses.h (after CEL_State expansions) */

/* Window lifecycle accessors -- called by observers in ncurses_module.c */
extern void ncurses_terminal_init(NCurses_WindowConfig* config);
extern void ncurses_terminal_shutdown(void);
extern void ncurses_window_set_entity(cels_entity_t entity);
extern cels_entity_t ncurses_window_get_entity(void);
extern bool ncurses_window_is_active(void);

/* ECS systems -- defined in source files, registered by module */
CEL_Define(NCurses_InputSystem);
CEL_Define(NCurses_WindowUpdateSystem);

/* Input terminal config (key sequences, mouse) -- called after initscr/newterm */
extern void ncurses_input_configure_terminal(void);

/* Frame pipeline systems registration */
extern void ncurses_register_frame_systems(void);

/* Layer entity lifecycle and systems -- defined in layer/tui_layer_entity.c */
CEL_Define_Lifecycle(TUI_LayerLC);
CEL_Define_System(TUI_LayerSyncSystem);
extern void ncurses_register_layer_systems(void);

/* Terminal spawn: kill child terminal emulator on shutdown */
extern void ncurses_kill_terminal(void);

#endif /* CELS_NCURSES_TUI_INTERNAL_H */
