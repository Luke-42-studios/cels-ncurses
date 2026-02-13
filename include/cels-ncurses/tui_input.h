/*
 * TUI Input Provider - Header
 *
 * TUI Input Provider -- Registers an ECS system at OnLoad that reads
 * ncurses getch() each frame and populates CELS_Input.
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
 */

#ifndef CELS_NCURSES_TUI_INPUT_H
#define CELS_NCURSES_TUI_INPUT_H

#include <cels/cels.h>

/* ===========================================
 * TUI_Input Provider Config
 * ===========================================
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

#endif /* CELS_NCURSES_TUI_INPUT_H */
