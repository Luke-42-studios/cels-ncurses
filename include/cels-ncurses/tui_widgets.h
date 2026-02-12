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
