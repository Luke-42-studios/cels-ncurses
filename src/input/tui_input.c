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
 * TUI Input System - ncurses Implementation
 *
 * ECS system at OnLoad that reads ncurses getch() each frame and writes
 * NCurses_InputState onto the window entity via cels_entity_set_component().
 *
 * Keyboard: maps ncurses key codes to NCurses_InputState fields.
 * Mouse: enables ncurses mouse events via mousemask(), handles KEY_MOUSE
 * with a drain loop (multiple events per frame, final position + last
 * button event win). Tracks persistent held state per button.
 *
 * Q key signals application quit via cels_request_quit().
 * All other keys map to NCurses_InputState fields for backend-agnostic consumption.
 *
 * NOTE: This system runs at OnLoad phase (before OnUpdate where user systems run).
 * ncurses must already be initialized by the window lifecycle before this runs.
 */

#include "cels-ncurses/tui_ncurses.h"
#include "cels-ncurses/tui_internal.h"
#include "cels-ncurses/tui_layer.h"
#include "cels-ncurses/tui_frame.h"
#include <ncurses.h>
#include <cels/cels.h>

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
 * Input Reading
 * ============================================================================
 *
 * Reads input per frame via ncurses wgetch(stdscr).
 * nodelay(stdscr, TRUE) ensures non-blocking (returns ERR if no input).
 * keypad(stdscr, TRUE) ensures function keys arrive as KEY_* constants.
 *
 * Builds a fresh NCurses_InputState on the stack each frame.
 * Persistent fields (mouse position, held buttons) are copied from the
 * previous frame's component state. Per-frame fields start zeroed.
 *
 * After processing, writes the state to the window entity via
 * cels_entity_set_component + cels_component_notify_change.
 */

static void tui_read_input_ncurses(void) {
    cels_entity_t win = ncurses_window_get_entity();
    if (win == 0) return;

    /* Read previous state for persistent fields (mouse pos, held buttons) */
    const NCurses_InputState* prev =
        (const NCurses_InputState*)cels_entity_get_component(win, NCurses_InputState_id);

    /* Build fresh state (all per-frame fields zeroed) */
    NCurses_InputState state = {0};

    /* Copy persistent fields from previous frame */
    if (prev) {
        state.mouse_x = prev->mouse_x;
        state.mouse_y = prev->mouse_y;
        state.mouse_left_held = prev->mouse_left_held;
        state.mouse_middle_held = prev->mouse_middle_held;
        state.mouse_right_held = prev->mouse_right_held;
    }

    int ch = wgetch(stdscr);
    if (ch == ERR) {
        /* No input this frame -- still write the zeroed-but-persistent state */
        cels_entity_set_component(win, NCurses_InputState_id,
                                   &state, sizeof(NCurses_InputState));
        cels_component_notify_change(NCurses_InputState_id);
        return;
    }

    switch (ch) {
        /* Arrow keys (decoded by keypad()) */
        case KEY_UP:    state.axis_y = -1.0f; break;
        case KEY_DOWN:  state.axis_y =  1.0f; break;
        case KEY_LEFT:  state.axis_x = -1.0f; break;
        case KEY_RIGHT: state.axis_x =  1.0f; break;

        /* WASD (raw character + axis) */
        case 'w': case 'W': state.axis_y = -1.0f; state.raw_key = ch; state.has_raw_key = true; break;
        case 's': case 'S': state.axis_y =  1.0f; state.raw_key = ch; state.has_raw_key = true; break;
        case 'a': case 'A': state.axis_x = -1.0f; state.raw_key = ch; state.has_raw_key = true; break;
        case 'd': case 'D': state.axis_x =  1.0f; state.raw_key = ch; state.has_raw_key = true; break;

        /* Action buttons */
        case '\n': case '\r': case KEY_ENTER:
            state.accept = true; break;
        case 27:  /* Escape */
            state.cancel = true;
            state.raw_key = 27; state.has_raw_key = true; break;

        /* Extended navigation */
        case '\t':
            state.tab = true; break;
        case KEY_BTAB:
            state.shift_tab = true; break;
        case KEY_HOME:
            state.home = true; break;
        case KEY_END:
            state.end = true; break;
        case KEY_PPAGE:
            state.page_up = true; break;
        case KEY_NPAGE:
            state.page_down = true; break;
        case KEY_BACKSPACE: case 127:
            state.backspace = true; break;
        case KEY_DC:
            state.delete_key = true; break;

        /* Quit key -- signals application quit (unless quit guard suppresses) */
        case 'q': case 'Q':
            if (g_quit_guard_fn && g_quit_guard_fn()) {
                /* Quit suppressed (e.g., text input active) -- pass as raw_key */
                state.raw_key = ch;
                state.has_raw_key = true;
                break;
            }
            cels_request_quit();
            return;  /* Do NOT write state -- immediate quit */

        /* Mouse events -- drain all queued events per frame */
        case KEY_MOUSE: {
            /* Drain ALL queued mouse events -- keep final position, last button event.
             * This prevents position lag when mouse moves fast (many events per frame). */
            MEVENT event;
            do {
                if (getmouse(&event) != OK) break;

                /* Always update position (screen-relative cell coordinates) */
                state.mouse_x = event.x;
                state.mouse_y = event.y;

                /* Decode button press/release events */
                if (event.bstate & BUTTON1_PRESSED) {
                    state.mouse_button = 1;
                    state.mouse_pressed = true;
                    state.mouse_released = false;
                    state.mouse_left_held = true;
                } else if (event.bstate & BUTTON1_RELEASED) {
                    state.mouse_button = 1;
                    state.mouse_pressed = false;
                    state.mouse_released = true;
                    state.mouse_left_held = false;
                } else if (event.bstate & BUTTON2_PRESSED) {
                    state.mouse_button = 2;
                    state.mouse_pressed = true;
                    state.mouse_released = false;
                    state.mouse_middle_held = true;
                } else if (event.bstate & BUTTON2_RELEASED) {
                    state.mouse_button = 2;
                    state.mouse_pressed = false;
                    state.mouse_released = true;
                    state.mouse_middle_held = false;
                } else if (event.bstate & BUTTON3_PRESSED) {
                    state.mouse_button = 3;
                    state.mouse_pressed = true;
                    state.mouse_released = false;
                    state.mouse_right_held = true;
                } else if (event.bstate & BUTTON3_RELEASED) {
                    state.mouse_button = 3;
                    state.mouse_pressed = false;
                    state.mouse_released = true;
                    state.mouse_right_held = false;
                }
                /* else: motion-only event (position already updated above) */

            } while ((ch = wgetch(stdscr)) == KEY_MOUSE);

            /* Put back any non-mouse key for the next frame */
            if (ch != ERR) ungetch(ch);
            break;  /* Fall through to entity write */
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

            ncurses_console_log("[resize] COLS=%d LINES=%d\n", COLS, LINES);

            /* Backend-specific cleanup: resize ncurses layers and invalidate.
             * Dimension state (NCurses_WindowState) is NOT updated here --
             * ncurses_window_frame_update() is the single authority for
             * dimension updates. This keeps the pattern portable. */
            tui_layer_resize_all(COLS, LINES);
            tui_frame_invalidate_all();
            clearok(curscr, TRUE);
            return;
        }

        /* Ctrl+Arrow keys (custom key codes via define_key) */
        case CELS_KEY_CTRL_UP:    state.raw_key = CELS_KEY_CTRL_UP;    state.has_raw_key = true; break;
        case CELS_KEY_CTRL_DOWN:  state.raw_key = CELS_KEY_CTRL_DOWN;  state.has_raw_key = true; break;
        case CELS_KEY_CTRL_LEFT:  state.raw_key = CELS_KEY_CTRL_LEFT;  state.has_raw_key = true; break;
        case CELS_KEY_CTRL_RIGHT: state.raw_key = CELS_KEY_CTRL_RIGHT; state.has_raw_key = true; break;

        /* Shift+Arrow keys (for future text selection) */
        case CELS_KEY_SHIFT_LEFT:  state.raw_key = CELS_KEY_SHIFT_LEFT;  state.has_raw_key = true; break;
        case CELS_KEY_SHIFT_RIGHT: state.raw_key = CELS_KEY_SHIFT_RIGHT; state.has_raw_key = true; break;

        /* F1 -- Pause for text selection/copy */
        case KEY_F(1):
            tui_pause();
            return;  /* Clean frame after resume */

        /* Numbers, function keys, raw characters */
        default:
            if (ch >= '0' && ch <= '9') {
                state.number = ch - '0';
                state.has_number = true;
            } else if (ch >= KEY_F(2) && ch <= KEY_F(12)) {
                state.function_key = ch - KEY_F(0);
                state.has_function = true;
            } else {
                state.raw_key = ch;
                state.has_raw_key = true;
            }
            break;
    }

    /* Write input state to the window entity */
    cels_entity_set_component(win, NCurses_InputState_id,
                               &state, sizeof(NCurses_InputState));
    cels_component_notify_change(NCurses_InputState_id);
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
 * Called by CEL_Module(NCurses) init body. Registers custom key sequences,
 * initializes mouse handling, and registers the ECS input system at OnLoad.
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
    /* Register xterm-compatible Ctrl+Arrow escape sequences */
    define_key("\033[1;5A", CELS_KEY_CTRL_UP);
    define_key("\033[1;5B", CELS_KEY_CTRL_DOWN);
    define_key("\033[1;5C", CELS_KEY_CTRL_RIGHT);
    define_key("\033[1;5D", CELS_KEY_CTRL_LEFT);

    /* Register xterm-compatible Shift+Arrow escape sequences */
    define_key("\033[1;2D", CELS_KEY_SHIFT_LEFT);
    define_key("\033[1;2C", CELS_KEY_SHIFT_RIGHT);

    /* Enable mouse events: buttons 1-3 press/release + position tracking */
    mmask_t desired = BUTTON1_PRESSED | BUTTON1_RELEASED
                    | BUTTON2_PRESSED | BUTTON2_RELEASED
                    | BUTTON3_PRESSED | BUTTON3_RELEASED
                    | REPORT_MOUSE_POSITION;
    mousemask(desired, NULL);
    mouseinterval(0);  /* Disable click detection -- raw press/release only */

    /* Register ECS system at OnLoad phase via CELS macro */
    NCurses_InputSystem_register();
}

/* ============================================================================
 * Quit Guard API
 * ============================================================================ */

void tui_input_set_quit_guard(bool (*guard_fn)(void)) {
    g_quit_guard_fn = guard_fn;
}
