/*
 * TUI Window Provider - Header
 *
 * ncurses-based window provider for CELS framework.
 * Owns terminal initialization (initscr/endwin) and the main frame loop.
 *
 * Usage:
 *   #include <cels-ncurses/tui_window.h>
 *
 *   static TUI_WindowState_t* windowState;
 *
 *   CEL_Build(App) {
 *       windowState = CEL_Use(TUI_Window, .title = "My App", .fps = 60);
 *   }
 *
 *   CEL_Root(AppUI) {
 *       TUI_WindowState_t* win = CEL_Watch(TUI_WindowState);
 *       if (win->state == WINDOW_STATE_READY) {
 *           CEL_Call(MainMenu) {}
 *       }
 *   }
 */

#ifndef CELS_NCURSES_TUI_WINDOW_H
#define CELS_NCURSES_TUI_WINDOW_H

#include <cels/cels.h>

/* ===========================================
 * Window State Machine (Vulkan-aligned)
 * ===========================================
 *
 * TUI transitions:   NONE -> READY (fast-track) -> CLOSING -> CLOSED
 * Vulkan transitions: NONE -> CREATED -> SURFACE_READY -> READY -> ...
 */
typedef enum WindowState {
    WINDOW_STATE_NONE = 0,
    WINDOW_STATE_CREATED,
    WINDOW_STATE_SURFACE_READY,
    WINDOW_STATE_READY,
    WINDOW_STATE_RESIZING,
    WINDOW_STATE_MINIMIZED,
    WINDOW_STATE_CLOSING,
    WINDOW_STATE_CLOSED
} WindowState;

/* ===========================================
 * TUI_WindowState -- Observable provider state
 * ===========================================
 *
 * Use CEL_Watch(TUI_WindowState) in CEL_Root to react to window changes.
 * The pointer returned by CEL_Use(TUI_Window, ...) points to this same state.
 */
typedef struct TUI_WindowState_t {
    WindowState state;
    int width;
    int height;
    const char* title;
    const char* version;
    float target_fps;
    float delta_time;
} TUI_WindowState_t;

extern TUI_WindowState_t TUI_WindowState;
extern cels_entity_t TUI_WindowStateID;
extern void TUI_WindowState_ensure(void);

/* ===========================================
 * TUI_Window Provider Config
 * ===========================================
 *
 * Configuration struct for the TUI window provider.
 * Passed to CEL_Use(TUI_Window, .title = "X", .fps = 60).
 */
typedef struct TUI_Window {
    const char* title;
    const char* version;
    int fps;
    int width;
    int height;
} TUI_Window;

/* Provider registration function (called by Use() macro)
 * Returns pointer to TUI_WindowState */
extern TUI_WindowState_t* TUI_Window_use(TUI_Window config);

/* Get pointer to g_running flag for quit signaling from input provider */
extern volatile int* tui_window_get_running_ptr(void);

#endif /* CELS_NCURSES_TUI_WINDOW_H */
