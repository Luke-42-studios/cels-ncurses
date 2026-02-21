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
 * TUI Window Provider - Header
 *
 * ncurses-based window provider for CELS framework.
 * Owns terminal initialization (initscr/endwin) and the main frame loop.
 *
 * Usage:
 *   #include <cels-ncurses/tui_window.h>
 *
 *   static Engine_WindowState_t* windowState;
 *
 *   CEL_Build(App) {
 *       windowState = CEL_Use(TUI_Window, .title = "My App", .fps = 60);
 *   }
 *
 *   CEL_Root(AppUI) {
 *       Engine_WindowState_t* win = CEL_Watch(Engine_WindowState);
 *       if (win->state == WINDOW_STATE_READY) {
 *           CEL_Init(MainMenu) {}
 *       }
 *   }
 */

#ifndef CELS_NCURSES_TUI_WINDOW_H
#define CELS_NCURSES_TUI_WINDOW_H

#include <cels/cels.h>
#include <cels/backend.h>

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
 * Engine_WindowState -- Observable provider state
 * ===========================================
 *
 * Use CEL_Watch(Engine_WindowState) in CEL_Root to react to window changes.
 * The pointer returned by CEL_Use(TUI_Window, ...) points to this same state.
 */
typedef struct Engine_WindowState_t {
    WindowState state;
    int width;
    int height;
    const char* title;
    const char* version;
    float target_fps;
    float delta_time;
} Engine_WindowState_t;

extern Engine_WindowState_t Engine_WindowState;
extern cels_entity_t Engine_WindowState_id;
extern void Engine_WindowState_register(void);

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
    int color_mode;    /* 0=auto (default), 1=256-color, 2=palette-redef, 3=direct-RGB */
} TUI_Window;

/* Provider registration function (called by Use() macro)
 * Returns pointer to Engine_WindowState */
extern Engine_WindowState_t* TUI_Window_use(TUI_Window config);

/* Get pointer to g_running flag for quit signaling from input provider */
extern volatile int* tui_window_get_running_ptr(void);

/* Access the standard CELS_WindowState for this backend */
extern CELS_WindowState* tui_window_get_standard_state(void);

/* ============================================================================
 * Backward Compatibility (v0.2 -> v0.3)
 * ============================================================================ */
typedef Engine_WindowState_t TUI_WindowState_t;
#define TUI_WindowState Engine_WindowState
#define TUI_WindowStateID Engine_WindowState_id
#define TUI_WindowState_register Engine_WindowState_register

#endif /* CELS_NCURSES_TUI_WINDOW_H */
