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
 * TUI Input System - Raw ncurses Input
 *
 * ECS system at OnLoad that drains the ncurses getch() queue each frame
 * and writes raw key codes + mouse state to the NCursesInput entity
 * via cel_query / cel_each / cel_update.
 *
 * No semantic interpretation -- keys are raw ncurses codes (KEY_UP, '\n',
 * 'q', KEY_F(1), etc.). The developer's systems decide what each key means.
 *
 * Mouse: drain loop captures all queued events per frame. Final position
 * and last button event are kept. Held state persists across frames.
 *
 * KEY_RESIZE: handled internally (layer resize + frame invalidation).
 * F1: handled internally (pause mode for text selection/copy).
 * All other keys go into the keys[] array for the developer to read.
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

/* Max keys per frame (matches NCurses_InputState.keys[16]) */
#define MAX_KEYS_PER_FRAME 16

/* ============================================================================
 * Pause Mode -- F1 freezes the frame loop for text selection/copy
 * ============================================================================ */

static void tui_pause(void) {
    nodelay(stdscr, FALSE);
    wgetch(stdscr);  /* Block until any key */
    nodelay(stdscr, TRUE);
}

/* ============================================================================
 * ECS System -- drains getch() queue, writes via cel_update
 * ============================================================================
 *
 * Queries the NCursesInput entity by its NCurses_InputState component.
 * Each frame: saves persistent fields, zeros per-frame fields, drains
 * the ncurses input queue, writes back via cel_update.
 */

CEL_System(NCurses_InputSystem, .phase = OnLoad) {
    cel_query(NCurses_InputState);
    cel_each(NCurses_InputState) {
        /* Save persistent fields from previous frame */
        int mx = NCurses_InputState->mouse_x;
        int my = NCurses_InputState->mouse_y;
        bool lh = NCurses_InputState->mouse_left_held;
        bool mh = NCurses_InputState->mouse_middle_held;
        bool rh = NCurses_InputState->mouse_right_held;

        cel_update(NCurses_InputState) {
            /* Reset per-frame fields, restore persistent */
            *NCurses_InputState = (struct NCurses_InputState){0};
            NCurses_InputState->mouse_x = mx;
            NCurses_InputState->mouse_y = my;
            NCurses_InputState->mouse_left_held = lh;
            NCurses_InputState->mouse_middle_held = mh;
            NCurses_InputState->mouse_right_held = rh;

            /* Drain all queued keys */
            int ch;
            while ((ch = wgetch(stdscr)) != ERR) {

                /* Internal: terminal resize */
                if (ch == KEY_RESIZE) {
                    int next;
                    while ((next = wgetch(stdscr)) == KEY_RESIZE) { /* consume */ }
                    if (next != ERR) ungetch(next);

                    if (COLS >= 4 && LINES >= 4) {
                        ncurses_console_log("[resize] COLS=%d LINES=%d\n", COLS, LINES);
                        tui_layer_resize_all(COLS, LINES);
                        tui_frame_invalidate_all();
                        clearok(curscr, TRUE);
                    }
                    continue;
                }

                /* Internal: F1 pause mode */
                if (ch == KEY_F(1)) {
                    tui_pause();
                    continue;
                }

                /* Mouse events -- drain all queued, keep final state */
                if (ch == KEY_MOUSE) {
                    MEVENT event;
                    do {
                        if (getmouse(&event) != OK) break;

                        NCurses_InputState->mouse_x = event.x;
                        NCurses_InputState->mouse_y = event.y;

                        if (event.bstate & BUTTON1_PRESSED) {
                            NCurses_InputState->mouse_button = 1;
                            NCurses_InputState->mouse_pressed = true;
                            NCurses_InputState->mouse_released = false;
                            NCurses_InputState->mouse_left_held = true;
                        } else if (event.bstate & BUTTON1_RELEASED) {
                            NCurses_InputState->mouse_button = 1;
                            NCurses_InputState->mouse_pressed = false;
                            NCurses_InputState->mouse_released = true;
                            NCurses_InputState->mouse_left_held = false;
                        } else if (event.bstate & BUTTON2_PRESSED) {
                            NCurses_InputState->mouse_button = 2;
                            NCurses_InputState->mouse_pressed = true;
                            NCurses_InputState->mouse_released = false;
                            NCurses_InputState->mouse_middle_held = true;
                        } else if (event.bstate & BUTTON2_RELEASED) {
                            NCurses_InputState->mouse_button = 2;
                            NCurses_InputState->mouse_pressed = false;
                            NCurses_InputState->mouse_released = true;
                            NCurses_InputState->mouse_middle_held = false;
                        } else if (event.bstate & BUTTON3_PRESSED) {
                            NCurses_InputState->mouse_button = 3;
                            NCurses_InputState->mouse_pressed = true;
                            NCurses_InputState->mouse_released = false;
                            NCurses_InputState->mouse_right_held = true;
                        } else if (event.bstate & BUTTON3_RELEASED) {
                            NCurses_InputState->mouse_button = 3;
                            NCurses_InputState->mouse_pressed = false;
                            NCurses_InputState->mouse_released = true;
                            NCurses_InputState->mouse_right_held = false;
                        }
                    } while ((ch = wgetch(stdscr)) == KEY_MOUSE);

                    if (ch != ERR) ungetch(ch);
                    continue;
                }

                /* All other keys: add to keys[] array */
                if (NCurses_InputState->key_count < MAX_KEYS_PER_FRAME) {
                    NCurses_InputState->keys[NCurses_InputState->key_count++] = ch;
                }
            }
        }
    }
}

/* ============================================================================
 * Terminal Configuration -- called after initscr()
 * ============================================================================ */

void ncurses_input_configure_terminal(void) {
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
    mouseinterval(0);
}
