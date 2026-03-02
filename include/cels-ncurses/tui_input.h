/* TUI Input - Internal Header
 *
 * Internal-only declarations for the input subsystem.
 * Public input type (NCurses_InputState) is in tui_ncurses.h.
 * Consumers read input via cels_entity_get_component() or cel_watch().
 */
#ifndef CELS_NCURSES_TUI_INPUT_H
#define CELS_NCURSES_TUI_INPUT_H

#include <stdbool.h>

/* Quit guard: when set, 'q'/'Q' calls guard before quitting.
 * If guard returns true, key passes as raw_key instead of quit.
 * Used by cels-widgets to suppress quit during text input. */
extern void tui_input_set_quit_guard(bool (*guard_fn)(void));

#endif /* CELS_NCURSES_TUI_INPUT_H */
