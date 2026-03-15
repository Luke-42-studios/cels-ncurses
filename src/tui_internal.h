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
 * Forward declarations for lifecycle accessors, system registration,
 * and layer panel helpers called by ncurses_module.c.
 *
 * This header is NOT included by consumers -- only by ncurses_module.c
 * and internal source files.
 */

#ifndef CELS_NCURSES_TUI_INTERNAL_H
#define CELS_NCURSES_TUI_INTERNAL_H

#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>
#include <stdbool.h>

/* Forward declarations for ncurses types used in layer helpers */
typedef struct panel PANEL;

/* Window lifecycle accessors -- called by observers in ncurses_module.c */
extern void ncurses_terminal_init(NCurses_WindowConfig* config);
extern void ncurses_terminal_shutdown(void);
extern void ncurses_window_set_entity(cels_entity_t entity);
extern cels_entity_t ncurses_window_get_entity(void);
extern bool ncurses_window_is_active(void);

/* ECS systems -- defined in source files, registered by module */
CEL_Define_System(NCurses_InputSystem);
CEL_Define_System(NCurses_WindowUpdateSystem);

/* Input terminal config (key sequences, mouse) -- called after initscr/newterm */
extern void ncurses_input_configure_terminal(void);

/* Frame pipeline systems registration */
extern void ncurses_register_frame_systems(void);

/* Layer panel operations -- defined in layer/tui_layer_panel.c */
extern TUI_DrawContext_Component ncurses_layer_panel_create(
    const TUI_LayerConfig* config, cels_entity_t entity);
extern void ncurses_layer_panel_destroy(const TUI_DrawContext_Component* dc);
extern void ncurses_layer_panel_resize(TUI_DrawContext_Component* dc, int new_w, int new_h);
extern void ncurses_layer_sync_visibility(bool visible, PANEL* panel);
extern void ncurses_layer_clear_window(WINDOW* win, TUI_SubCellBuffer* subcell_buf);
extern void ncurses_layer_sort_and_stack(PANEL** panels, int* z_orders, int count);

/* Terminal spawn: kill child terminal emulator on shutdown */
extern void ncurses_kill_terminal(void);

#endif /* CELS_NCURSES_TUI_INTERNAL_H */
