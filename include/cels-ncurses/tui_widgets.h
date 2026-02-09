/*
 * TUI Widgets - ncurses renderer registration for cels-widgets components
 *
 * Widget component definitions (W_TabBar, W_Button, W_StatusBar, etc.) live
 * in cels-widgets/widgets.h. This header provides the registration function
 * that wires up ncurses renderers for those components.
 *
 * Usage:
 *   #include <cels-widgets/widgets.h>
 *   #include <cels-ncurses/tui_widgets.h>
 *
 *   CEL_Build(App) {
 *       Widgets_init();
 *       TUI_Engine_use(...);
 *       tui_widgets_register();
 *   }
 */

#ifndef CELS_NCURSES_TUI_WIDGETS_H
#define CELS_NCURSES_TUI_WIDGETS_H

#include <cels-widgets/widgets.h>

/* ============================================================================
 * Registration API
 * ============================================================================ */

/*
 * Register ncurses renderers for all standard widget components.
 * Call once during CEL_Build, after TUI_Engine_use() and Widgets_init().
 *
 * Registers renderers for:
 *   W_TabBar, W_TabContent, W_StatusBar, W_Button, W_Slider,
 *   W_Text, W_InfoBox, W_Canvas, W_Hint,
 *   W_Toggle, W_Cycle, W_RadioButton, W_RadioGroup,
 *   W_ListView, W_ListItem
 *
 * All renderers draw into the background layer via the frame pipeline.
 */
extern void tui_widgets_register(void);

#endif /* CELS_NCURSES_TUI_WIDGETS_H */
