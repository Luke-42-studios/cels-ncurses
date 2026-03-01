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
 * TUI Input Provider - ncurses Implementation
 *
 * Implements the TUI_Input provider as a standalone ECS system at OnLoad.
 * Reads ncurses getch() each frame and populates TUI_InputState (g_input_state).
 *
 * Keyboard: maps ncurses key codes to TUI_InputState fields.
 * Mouse: enables ncurses mouse events via mousemask(), handles KEY_MOUSE
 * with a drain loop (multiple events per frame, final position + last
 * button event win). Tracks persistent held state per button.
 *
 * Q key signals application quit via the window provider's running pointer.
 * All other keys map to TUI_InputState fields for backend-agnostic consumption.
 *
 * NOTE: This system runs during Engine_Progress() at OnLoad phase.
 * ncurses must already be initialized by TUI_Window before this runs.
 */

#include "cels-ncurses/tui_input.h"
#include "cels-ncurses/tui_window.h"
#include "cels-ncurses/tui_layer.h"
#include "cels-ncurses/tui_frame.h"
#include <ncurses.h>
#include <cels/cels.h>
#include <stdio.h>
#include <string.h>

/* Custom key codes for Ctrl+Arrow (above KEY_MAX, below INT_MAX) */
#define CELS_KEY_CTRL_UP    600
#define CELS_KEY_CTRL_DOWN  601
#define CELS_KEY_CTRL_RIGHT 602
#define CELS_KEY_CTRL_LEFT  603

/* Custom key codes for Shift+Arrow (text selection support) */
#define CELS_KEY_SHIFT_LEFT  604
#define CELS_KEY_SHIFT_RIGHT 605

/* ============================================================================
 * Static State
 * ============================================================================ */

/* Global input state (extern declaration in tui_input.h) */
TUI_InputState g_input_state = {0};

/* Mouse initialization flag */
static int g_mouse_initialized = 0;

/* Pointer to g_running in tui_window.c -- used to signal quit on Q key */
static volatile int* g_tui_running_ptr = NULL;

/* Quit guard callback -- when set and returns true, 'q'/'Q' passes through
 * as raw_key instead of triggering application quit. Used by cels-widgets
 * to suppress quit when text input is active. */
static bool (*g_quit_guard_fn)(void) = NULL;

/* ============================================================================
 * Pause Mode -- F1 freezes the frame loop for text selection/copy
 *
 * Keeps ncurses active (preserves the screen) but switches getch to
 * blocking mode so the frame loop stalls.  The last rendered frame stays
 * on screen and the user can select/copy text with their terminal
 * (mouse select, Ctrl+Shift+C in Kitty, etc.).  Any key resumes.
 * ============================================================================ */

static void tui_pause(void) {
    /* Switch to blocking input -- frame loop stalls here */
    nodelay(stdscr, FALSE);
    wgetch(stdscr);  /* Block until any key */
    nodelay(stdscr, TRUE);
}

/* ============================================================================
 * Input State Getters
 * ============================================================================ */

const TUI_InputState* tui_input_get_state(void) {
    return &g_input_state;
}

void tui_input_get_mouse_position(int *x, int *y) {
    if (x) *x = g_input_state.mouse_x;
    if (y) *y = g_input_state.mouse_y;
}

/* ============================================================================
 * Input Reading
 * ============================================================================
 *
 * Reads input per frame via ncurses wgetch(stdscr).
 * nodelay(stdscr, TRUE) ensures non-blocking (returns ERR if no input).
 * keypad(stdscr, TRUE) ensures function keys arrive as KEY_* constants.
 *
 * Per-frame fields are reset at the start; persistent state (mouse position,
 * held buttons) is preserved across frames.
 */

static void tui_read_input_ncurses(void) {
    /* Reset per-frame fields (preserve persistent state like mouse_x/y and held buttons) */
    g_input_state.axis_left[0] = 0.0f;
    g_input_state.axis_left[1] = 0.0f;
    g_input_state.button_accept = false;
    g_input_state.button_cancel = false;
    g_input_state.key_tab = false;
    g_input_state.key_shift_tab = false;
    g_input_state.key_home = false;
    g_input_state.key_end = false;
    g_input_state.key_page_up = false;
    g_input_state.key_page_down = false;
    g_input_state.key_backspace = false;
    g_input_state.key_delete = false;
    g_input_state.has_number = false;
    g_input_state.has_function = false;
    g_input_state.has_raw_key = false;
    g_input_state.mouse_button = TUI_MOUSE_NONE;
    g_input_state.mouse_pressed = false;
    g_input_state.mouse_released = false;
    /* NOTE: mouse_x, mouse_y, mouse_*_held are NOT reset -- they persist across frames */

    int ch = wgetch(stdscr);
    if (ch == ERR) {
        /* No input this frame */
        return;
    }

    switch (ch) {
        /* Arrow keys (decoded by keypad()) */
        case KEY_UP:    g_input_state.axis_left[1] = -1.0f; break;
        case KEY_DOWN:  g_input_state.axis_left[1] =  1.0f; break;
        case KEY_LEFT:  g_input_state.axis_left[0] = -1.0f; break;
        case KEY_RIGHT: g_input_state.axis_left[0] =  1.0f; break;

        /* WASD (raw character + axis) */
        case 'w': case 'W': g_input_state.axis_left[1] = -1.0f; g_input_state.raw_key = ch; g_input_state.has_raw_key = true; break;
        case 's': case 'S': g_input_state.axis_left[1] =  1.0f; g_input_state.raw_key = ch; g_input_state.has_raw_key = true; break;
        case 'a': case 'A': g_input_state.axis_left[0] = -1.0f; g_input_state.raw_key = ch; g_input_state.has_raw_key = true; break;
        case 'd': case 'D': g_input_state.axis_left[0] =  1.0f; g_input_state.raw_key = ch; g_input_state.has_raw_key = true; break;

        /* Action buttons */
        case '\n': case '\r': case KEY_ENTER:
            g_input_state.button_accept = true; break;
        case 27:  /* Escape */
            g_input_state.button_cancel = true;
            g_input_state.raw_key = 27; g_input_state.has_raw_key = true; break;

        /* Extended navigation */
        case '\t':
            g_input_state.key_tab = true; break;
        case KEY_BTAB:
            g_input_state.key_shift_tab = true; break;
        case KEY_HOME:
            g_input_state.key_home = true; break;
        case KEY_END:
            g_input_state.key_end = true; break;
        case KEY_PPAGE:
            g_input_state.key_page_up = true; break;
        case KEY_NPAGE:
            g_input_state.key_page_down = true; break;
        case KEY_BACKSPACE: case 127:
            g_input_state.key_backspace = true; break;
        case KEY_DC:
            g_input_state.key_delete = true; break;

        /* Quit key -- signals window to close (unless quit guard suppresses) */
        case 'q': case 'Q':
            if (g_quit_guard_fn && g_quit_guard_fn()) {
                /* Quit suppressed (e.g., text input active) -- pass as raw_key */
                g_input_state.raw_key = ch;
                g_input_state.has_raw_key = true;
                break;
            }
            if (g_tui_running_ptr) {
                *g_tui_running_ptr = 0;
            }
            cels_request_quit();
            return;  /* Do NOT set any input field -- immediate quit */

        /* Mouse events -- drain all queued events per frame */
        case KEY_MOUSE: {
            /* Drain ALL queued mouse events -- keep final position, last button event.
             * This prevents position lag when mouse moves fast (many events per frame). */
            MEVENT event;
            do {
                if (getmouse(&event) != OK) break;

                /* Always update position (screen-relative cell coordinates) */
                g_input_state.mouse_x = event.x;
                g_input_state.mouse_y = event.y;

                /* Decode button press/release events */
                if (event.bstate & BUTTON1_PRESSED) {
                    g_input_state.mouse_button = TUI_MOUSE_LEFT;
                    g_input_state.mouse_pressed = true;
                    g_input_state.mouse_released = false;
                    g_input_state.mouse_left_held = true;
                } else if (event.bstate & BUTTON1_RELEASED) {
                    g_input_state.mouse_button = TUI_MOUSE_LEFT;
                    g_input_state.mouse_pressed = false;
                    g_input_state.mouse_released = true;
                    g_input_state.mouse_left_held = false;
                } else if (event.bstate & BUTTON2_PRESSED) {
                    g_input_state.mouse_button = TUI_MOUSE_MIDDLE;
                    g_input_state.mouse_pressed = true;
                    g_input_state.mouse_released = false;
                    g_input_state.mouse_middle_held = true;
                } else if (event.bstate & BUTTON2_RELEASED) {
                    g_input_state.mouse_button = TUI_MOUSE_MIDDLE;
                    g_input_state.mouse_pressed = false;
                    g_input_state.mouse_released = true;
                    g_input_state.mouse_middle_held = false;
                } else if (event.bstate & BUTTON3_PRESSED) {
                    g_input_state.mouse_button = TUI_MOUSE_RIGHT;
                    g_input_state.mouse_pressed = true;
                    g_input_state.mouse_released = false;
                    g_input_state.mouse_right_held = true;
                } else if (event.bstate & BUTTON3_RELEASED) {
                    g_input_state.mouse_button = TUI_MOUSE_RIGHT;
                    g_input_state.mouse_pressed = false;
                    g_input_state.mouse_released = true;
                    g_input_state.mouse_right_held = false;
                }
                /* else: motion-only event (position already updated above) */

            } while ((ch = wgetch(stdscr)) == KEY_MOUSE);

            /* Put back any non-mouse key for the next frame */
            if (ch != ERR) ungetch(ch);
            return;  /* Mouse events fully processed */
        }

        /* Terminal resize -- drain queued resize events (window manager sends
         * many during a drag), then process only the final dimensions. */
        case KEY_RESIZE: {
            /* Drain additional KEY_RESIZE events queued during rapid drag */
            int next;
            while ((next = wgetch(stdscr)) == KEY_RESIZE) { /* consume */ }
            if (next != ERR) ungetch(next);  /* put back non-resize key */

            /* Skip if dimensions are invalid (can happen mid-animation) */
            if (COLS < 4 || LINES < 4) {
                return;
            }

            fprintf(stderr, "[resize] COLS=%d LINES=%d\n", COLS, LINES);

            /* Backend-specific cleanup: resize ncurses layers and invalidate.
             * Dimension state (Engine_WindowState, CEL_Window) is NOT updated
             * here -- tui_hook_frame_begin() is the single authority for
             * dimension updates. This keeps the pattern portable: each
             * backend's frame_begin polls dimensions and updates CEL_Window. */
            tui_layer_resize_all(COLS, LINES);
            tui_frame_invalidate_all();
            clearok(curscr, TRUE);
            return;
        }

        /* Ctrl+Arrow keys (custom key codes via define_key) */
        case CELS_KEY_CTRL_UP:    g_input_state.raw_key = CELS_KEY_CTRL_UP;    g_input_state.has_raw_key = true; break;
        case CELS_KEY_CTRL_DOWN:  g_input_state.raw_key = CELS_KEY_CTRL_DOWN;  g_input_state.has_raw_key = true; break;
        case CELS_KEY_CTRL_LEFT:  g_input_state.raw_key = CELS_KEY_CTRL_LEFT;  g_input_state.has_raw_key = true; break;
        case CELS_KEY_CTRL_RIGHT: g_input_state.raw_key = CELS_KEY_CTRL_RIGHT; g_input_state.has_raw_key = true; break;

        /* Shift+Arrow keys (for future text selection) */
        case CELS_KEY_SHIFT_LEFT:  g_input_state.raw_key = CELS_KEY_SHIFT_LEFT;  g_input_state.has_raw_key = true; break;
        case CELS_KEY_SHIFT_RIGHT: g_input_state.raw_key = CELS_KEY_SHIFT_RIGHT; g_input_state.has_raw_key = true; break;

        /* F1 -- Pause for text selection/copy */
        case KEY_F(1):
            tui_pause();
            return;  /* Clean frame after resume */

        /* Numbers, function keys, raw characters */
        default:
            if (ch >= '0' && ch <= '9') {
                g_input_state.key_number = ch - '0';
                g_input_state.has_number = true;
            } else if (ch >= KEY_F(2) && ch <= KEY_F(12)) {
                g_input_state.key_function = ch - KEY_F(0);
                g_input_state.has_function = true;
            } else {
                g_input_state.raw_key = ch;
                g_input_state.has_raw_key = true;
            }
            break;
    }
}

/* ============================================================================
 * ECS System Definition -- replaces raw ecs_system_init boilerplate
 * ============================================================================
 *
 * CEL_System generates NCurses_InputSystem_register() with external linkage.
 * The system runs once per frame at OnLoad phase (before rendering).
 */

CEL_System(NCurses_InputSystem, .phase = OnLoad) {
    cel_run {
        tui_read_input_ncurses();
    }
}

/* ============================================================================
 * ncurses_register_input_system -- Module entry point
 * ============================================================================
 *
 * Called by CEL_Module(NCurses) init body. Sets up the running pointer
 * bridge, registers custom key sequences, initializes mouse handling,
 * and registers the ECS input system at OnLoad phase.
 *
 * Overrides the weak stub in ncurses_module.c.
 *
 * NOTE: Custom key definitions (define_key) and mouse init (mousemask)
 * require ncurses to be active. However, the module registers this at
 * init time before the window observer fires. These calls are deferred
 * in practice because the ECS system callback only runs after the
 * pipeline starts, at which point ncurses is active. The define_key
 * and mousemask calls here are safe because ncurses tolerates them
 * before initscr -- they take effect once initscr runs.
 */

void ncurses_register_input_system(void) {
    /* Get running pointer from window provider for quit signaling */
    g_tui_running_ptr = tui_window_get_running_ptr();

    /* Register xterm-compatible Ctrl+Arrow escape sequences */
    define_key("\033[1;5A", CELS_KEY_CTRL_UP);
    define_key("\033[1;5B", CELS_KEY_CTRL_DOWN);
    define_key("\033[1;5C", CELS_KEY_CTRL_RIGHT);
    define_key("\033[1;5D", CELS_KEY_CTRL_LEFT);

    /* Register xterm-compatible Shift+Arrow escape sequences */
    define_key("\033[1;2D", CELS_KEY_SHIFT_LEFT);
    define_key("\033[1;2C", CELS_KEY_SHIFT_RIGHT);

    /* Initialize mouse position to "no position yet" */
    g_input_state.mouse_x = -1;
    g_input_state.mouse_y = -1;

    /* Enable mouse events: buttons 1-3 press/release + position tracking */
    mmask_t desired = BUTTON1_PRESSED | BUTTON1_RELEASED
                    | BUTTON2_PRESSED | BUTTON2_RELEASED
                    | BUTTON3_PRESSED | BUTTON3_RELEASED
                    | REPORT_MOUSE_POSITION;
    mousemask(desired, NULL);
    mouseinterval(0);  /* Disable click detection -- raw press/release only */
    g_mouse_initialized = 1;

    /* Register ECS system at OnLoad phase via CELS macro */
    NCurses_InputSystem_register();
}

/* ============================================================================
 * Quit Guard API
 * ============================================================================ */

void tui_input_set_quit_guard(bool (*guard_fn)(void)) {
    g_quit_guard_fn = guard_fn;
}
