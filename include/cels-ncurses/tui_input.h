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
 * TUI Input Provider - Header
 *
 * TUI Input Provider -- Registers an ECS system at OnLoad that reads
 * ncurses getch() each frame and populates TUI_InputState.
 * Requires TUI_Window to be initialized first (ncurses session must be active).
 *
 * Usage:
 *   #include <cels-ncurses/tui_input.h>
 *
 *   CEL_Build(App) {
 *       CEL_Use(TUI_Window, .title = "My App", .fps = 60);
 *       CEL_Use(TUI_Input);  // Registers input system (zero config)
 *   }
 *
 * Key mappings:
 *   Arrow keys, WASD -> axis_left
 *   Enter            -> button_accept
 *   Escape           -> button_cancel
 *   Tab, Shift+Tab   -> key_tab, key_shift_tab
 *   Home, End, PgUp, PgDn -> navigation keys
 *   F1-F12           -> key_function
 *   0-9              -> key_number
 *   Q                -> quit application (sets g_running = 0)
 *
 * Mouse:
 *   Position         -> mouse_x, mouse_y (pollable, screen-relative cells)
 *   Left/Mid/Right   -> mouse_button + mouse_pressed/mouse_released
 *   Held state       -> mouse_left_held, mouse_middle_held, mouse_right_held
 */

#ifndef CELS_NCURSES_TUI_INPUT_H
#define CELS_NCURSES_TUI_INPUT_H

#include <cels/cels.h>
#include <stdbool.h>

/* ============================================================================
 * TUI_MouseButton -- Which mouse button had an event this frame
 * ============================================================================ */
typedef enum TUI_MouseButton {
    TUI_MOUSE_NONE   = 0,
    TUI_MOUSE_LEFT   = 1,
    TUI_MOUSE_MIDDLE = 2,
    TUI_MOUSE_RIGHT  = 3
} TUI_MouseButton;

/* ============================================================================
 * TUI_InputState -- Per-frame input state with keyboard and mouse fields
 * ============================================================================
 *
 * Global input state with extern linkage (INTERFACE library pattern).
 * Declared here, defined in tui_input.c.
 *
 * Keyboard fields are reset every frame. Mouse position and held state
 * persist across frames; button events (mouse_pressed/mouse_released)
 * are per-frame.
 */
typedef struct TUI_InputState {
    /* Keyboard: directional input (-1.0/0.0/1.0 for x/y) */
    float axis_left[2];

    /* Keyboard: action buttons */
    bool button_accept;     /* Enter/Return */
    bool button_cancel;     /* Escape */

    /* Keyboard: extended navigation */
    bool key_tab;
    bool key_shift_tab;
    bool key_home;
    bool key_end;
    bool key_page_up;
    bool key_page_down;
    bool key_backspace;
    bool key_delete;

    /* Keyboard: number keys */
    int  key_number;
    bool has_number;

    /* Keyboard: function keys */
    int  key_function;
    bool has_function;

    /* Keyboard: raw key passthrough */
    int  raw_key;
    bool has_raw_key;

    /* Mouse: current position in screen-relative cell coordinates
     * (-1 = no position yet; persists across frames) */
    int mouse_x;
    int mouse_y;

    /* Mouse: button event this frame (NONE if no button event) */
    TUI_MouseButton mouse_button;
    bool mouse_pressed;     /* true if mouse_button was pressed this frame */
    bool mouse_released;    /* true if mouse_button was released this frame */

    /* Mouse: persistent held state (set on press, cleared on release) */
    bool mouse_left_held;
    bool mouse_middle_held;
    bool mouse_right_held;
} TUI_InputState;

extern TUI_InputState g_input_state;

/* ============================================================================
 * Input State Getters
 * ============================================================================ */

/* Returns read-only pointer to the global input state */
extern const TUI_InputState* tui_input_get_state(void);

/* Convenience getter for mouse position (NULL-safe for either parameter) */
extern void tui_input_get_mouse_position(int *x, int *y);

/* ============================================================================
 * TUI_Input Provider Config
 * ============================================================================
 *
 * Zero-config provider. C requires at least one struct field.
 */
typedef struct TUI_Input {
    int _placeholder;
} TUI_Input;

/* Provider registration function (called by Use() macro) */
extern void TUI_Input_use(TUI_Input config);

/*
 * Quit guard: when set, 'q'/'Q' calls the guard function before quitting.
 * If guard returns true, the key is passed through as raw_key instead of
 * triggering quit. Used by cels-widgets to suppress quit when text input
 * is active.
 */
extern void tui_input_set_quit_guard(bool (*guard_fn)(void));

/* ============================================================================
 * Backward Compatibility
 * ============================================================================
 *
 * CELS_Input was removed from the cels core library (Phase 26 refactoring).
 * These shims allow consumers to migrate gradually.
 */

/* Consumers using CELS_Input can migrate to TUI_InputState */
typedef TUI_InputState CELS_Input;

/* Consumers using cels_input_get() -- redirects to module-local global */
static inline const CELS_Input* cels_input_get(void* ctx) {
    (void)ctx;
    return &g_input_state;
}

#endif /* CELS_NCURSES_TUI_INPUT_H */
