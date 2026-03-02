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
 * and writes raw key codes + mouse state to a standalone input entity.
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
 * Static State
 * ============================================================================ */

/* The standalone input entity (created during system registration) */
static cels_entity_t g_input_entity = 0;

/* ============================================================================
 * Pause Mode -- F1 freezes the frame loop for text selection/copy
 * ============================================================================ */

static void tui_pause(void) {
    nodelay(stdscr, FALSE);
    wgetch(stdscr);  /* Block until any key */
    nodelay(stdscr, TRUE);
}

/* ============================================================================
 * Input Reading
 * ============================================================================
 *
 * Drains ALL queued keys from getch() into keys[] array (up to 16 per frame).
 * Mouse events are drained separately via the KEY_MOUSE drain loop.
 * KEY_RESIZE and F1 are handled internally and not passed to the developer.
 *
 * Persistent fields (mouse position, held buttons) are copied from the
 * previous frame. Per-frame fields (keys, mouse press/release) start zeroed.
 */

static void tui_read_input_ncurses(void) {
    if (g_input_entity == 0) return;

    /* Read previous state for persistent fields */
    const NCurses_InputState* prev =
        (const NCurses_InputState*)cels_entity_get_component(
            g_input_entity, NCurses_InputState_id);

    /* Build fresh state */
    NCurses_InputState state = {0};

    /* Copy persistent fields from previous frame */
    if (prev) {
        state.mouse_x = prev->mouse_x;
        state.mouse_y = prev->mouse_y;
        state.mouse_left_held = prev->mouse_left_held;
        state.mouse_middle_held = prev->mouse_middle_held;
        state.mouse_right_held = prev->mouse_right_held;
    }

    /* Drain all queued keys */
    int ch;
    while ((ch = wgetch(stdscr)) != ERR) {

        /* Internal: terminal resize */
        if (ch == KEY_RESIZE) {
            /* Drain additional resize events */
            int next;
            while ((next = wgetch(stdscr)) == KEY_RESIZE) { /* consume */ }
            if (next != ERR) ungetch(next);

            if (COLS >= 4 && LINES >= 4) {
                ncurses_console_log("[resize] COLS=%d LINES=%d\n", COLS, LINES);
                tui_layer_resize_all(COLS, LINES);
                tui_frame_invalidate_all();
                clearok(curscr, TRUE);
            }
            continue;  /* Don't put resize in keys[] */
        }

        /* Internal: F1 pause mode */
        if (ch == KEY_F(1)) {
            tui_pause();
            continue;  /* Don't put F1 in keys[] */
        }

        /* Mouse events -- drain all queued, keep final state */
        if (ch == KEY_MOUSE) {
            MEVENT event;
            do {
                if (getmouse(&event) != OK) break;

                state.mouse_x = event.x;
                state.mouse_y = event.y;

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
            } while ((ch = wgetch(stdscr)) == KEY_MOUSE);

            if (ch != ERR) ungetch(ch);
            continue;  /* Mouse doesn't go in keys[] */
        }

        /* All other keys: add to keys[] array */
        if (state.key_count < MAX_KEYS_PER_FRAME) {
            state.keys[state.key_count++] = ch;
        }
    }

    /* Write to the input entity */
    cels_entity_set_component(g_input_entity, NCurses_InputState_id,
                               &state, sizeof(NCurses_InputState));
    cels_component_notify_change(NCurses_InputState_id);
}

/* ============================================================================
 * ECS System Definition
 * ============================================================================ */

CEL_System(NCurses_InputSystem, .phase = OnLoad) {
    cel_run {
        tui_read_input_ncurses();
    }
}

/* ============================================================================
 * ncurses_register_input_system -- Module entry point
 * ============================================================================
 *
 * Called by CEL_Module(NCurses) init body. Creates the standalone input entity,
 * registers custom key sequences, initializes mouse, registers the ECS system.
 */

void ncurses_register_input_system(void) {
    /* Create standalone input entity */
    cels_begin_entity("NCursesInput");
    g_input_entity = cels_get_current_entity();
    NCurses_InputState initial = {0};
    initial.mouse_x = -1;
    initial.mouse_y = -1;
    cels_entity_set_component(g_input_entity, NCurses_InputState_id,
                               &initial, sizeof(NCurses_InputState));
    cels_end_entity();

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

    /* Register ECS system at OnLoad phase */
    NCurses_InputSystem_register();
}
