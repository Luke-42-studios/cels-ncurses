/*
 * TUI Widgets - Standard widget components and ncurses renderers
 *
 * Provides reusable TUI widget components (TabBar, TabContent, StatusBar)
 * with ncurses renderers that draw into the frame pipeline's background layer.
 *
 * Apps define compositions using these components; the backend handles rendering.
 * Call tui_widgets_register() during CEL_Build to wire up the renderers.
 *
 * Usage:
 *   #include <cels-ncurses/tui_widgets.h>
 *
 *   CEL_Build(App) {
 *       TUI_Engine_use(...);
 *       tui_widgets_register();
 *   }
 *
 *   CEL_Composition(MyTabBar) {
 *       CEL_Has(TUI_TabBar, .active = 0, .count = 4, .labels = my_labels);
 *   }
 */

#ifndef CELS_NCURSES_TUI_WIDGETS_H
#define CELS_NCURSES_TUI_WIDGETS_H

#include <cels/cels.h>

/* ============================================================================
 * Widget Components
 * ============================================================================ */

/* Tab bar: horizontal tab strip with numbered labels */
CEL_Define(TUI_TabBar, {
    int active;             /* Index of the currently active tab */
    int count;              /* Total number of tabs */
    const char** labels;    /* Array of tab label strings (count elements) */
});

/* Tab content: placeholder content area for a tab */
CEL_Define(TUI_TabContent, {
    const char* text;       /* Main placeholder text (centered) */
    const char* hint;       /* Secondary hint text (centered, below main) */
});

/* Status bar: bottom status line with left and right sections */
CEL_Define(TUI_StatusBar, {
    const char* left;       /* Left-aligned text (e.g., "app v1.0 | /path") */
    const char* right;      /* Right-aligned text (e.g., "q:quit  1-4:tab") */
});

/* ============================================================================
 * Registration API
 * ============================================================================ */

/*
 * Register ncurses renderers for all standard widget components.
 * Call once during CEL_Build, after TUI_Engine_use().
 *
 * Registers:
 *   - TUI_TabBar renderer (draws at row 0)
 *   - TUI_TabContent renderer (draws centered)
 *   - TUI_StatusBar renderer (draws at bottom row)
 *
 * All renderers draw into the background layer via the frame pipeline.
 */
extern void tui_widgets_register(void);

#endif /* CELS_NCURSES_TUI_WIDGETS_H */
