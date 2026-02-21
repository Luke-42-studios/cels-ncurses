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
 * TUI Widgets - DEPRECATED
 *
 * Widget rendering has moved to cels-widgets with Clay layout integration.
 * Widgets now describe themselves through Clay primitives (RECTANGLE, TEXT,
 * BORDER) and the generic clay_ncurses_renderer renders all Clay commands.
 *
 * Migration:
 *   OLD: #include <cels-ncurses/tui_widgets.h>
 *        tui_widgets_register();
 *
 *   NEW: #include <cels-widgets/compositions.h>
 *        Widgets_init();
 *        Clay_Engine_use(NULL);
 *        clay_ncurses_renderer_init(NULL);
 *
 * The tui_widgets_register() function is kept as a no-op for backward
 * compatibility during migration. It will be removed in a future release.
 */

#ifndef CELS_NCURSES_TUI_WIDGETS_H
#define CELS_NCURSES_TUI_WIDGETS_H

#include <cels-widgets/widgets.h>

/* Legacy registration (no-op -- Clay handles rendering now) */
static inline void tui_widgets_register(void) {
    /* Widget rendering is now handled by Clay layout functions
     * in cels-widgets/layouts.c + the generic clay_ncurses_renderer.
     * This function is a no-op kept for backward compatibility. */
}

#endif /* CELS_NCURSES_TUI_WIDGETS_H */
