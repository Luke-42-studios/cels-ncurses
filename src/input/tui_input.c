/*
 * TUI Input Provider - ncurses Implementation
 *
 * Implements the TUI_Input provider as a standalone ECS system at OnLoad.
 * Reads ncurses getch() each frame and populates CELS_Input.
 *
 * Q key signals application quit via the window provider's running pointer.
 * All other keys map to CELS_Input fields for backend-agnostic consumption.
 *
 * NOTE: This system runs during Engine_Progress() at OnLoad phase.
 * ncurses must already be initialized by TUI_Window before this runs.
 */

#include "cels-ncurses/tui_input.h"
#include "cels-ncurses/tui_window.h"
#include "cels-ncurses/tui_layer.h"
#include "cels-ncurses/tui_frame.h"
#include <ncurses.h>
#include <flecs.h>
#include <string.h>

/* ============================================================================
 * Static State
 * ============================================================================ */

/* Pointer to g_running in tui_window.c -- used to signal quit on Q key */
static volatile int* g_tui_running_ptr = NULL;

/* ============================================================================
 * Input Reading
 * ============================================================================
 *
 * Reads a single key per frame via ncurses wgetch(stdscr).
 * nodelay(stdscr, TRUE) ensures non-blocking (returns ERR if no input).
 * keypad(stdscr, TRUE) ensures function keys arrive as KEY_* constants.
 */

static void tui_read_input_ncurses(void) {
    CELS_Context* ctx = cels_get_context();
    CELS_Input input;
    memset(&input, 0, sizeof(input));

    int ch = wgetch(stdscr);
    if (ch == ERR) {
        /* No input this frame */
        cels_input_set(ctx, &input);
        return;
    }

    switch (ch) {
        /* Arrow keys (decoded by keypad()) */
        case KEY_UP:    input.axis_left[1] = -1.0f; break;
        case KEY_DOWN:  input.axis_left[1] =  1.0f; break;
        case KEY_LEFT:  input.axis_left[0] = -1.0f; break;
        case KEY_RIGHT: input.axis_left[0] =  1.0f; break;

        /* WASD (raw character) */
        case 'w': case 'W': input.axis_left[1] = -1.0f; break;
        case 's': case 'S': input.axis_left[1] =  1.0f; break;
        case 'a': case 'A': input.axis_left[0] = -1.0f; break;
        case 'd': case 'D': input.axis_left[0] =  1.0f; break;

        /* Action buttons */
        case '\n': case '\r': case KEY_ENTER:
            input.button_accept = true; break;
        case 27:  /* Escape */
            input.button_cancel = true; break;

        /* Extended navigation */
        case '\t':
            input.key_tab = true; break;
        case KEY_BTAB:
            input.key_shift_tab = true; break;
        case KEY_HOME:
            input.key_home = true; break;
        case KEY_END:
            input.key_end = true; break;
        case KEY_PPAGE:
            input.key_page_up = true; break;
        case KEY_NPAGE:
            input.key_page_down = true; break;
        case KEY_BACKSPACE: case 127:
            input.key_backspace = true; break;
        case KEY_DC:
            input.key_delete = true; break;

        /* Quit key -- signals window to close */
        case 'q': case 'Q':
            if (g_tui_running_ptr) {
                *g_tui_running_ptr = 0;
            }
            return;  /* Do NOT set any input field -- immediate quit */

        /* Terminal resize -- resize layers FIRST, then notify observers */
        case KEY_RESIZE:
            tui_layer_resize_all(COLS, LINES);
            tui_frame_invalidate_all();
            TUI_WindowState.width = COLS;
            TUI_WindowState.height = LINES;
            TUI_WindowState.state = WINDOW_STATE_RESIZING;
            cels_state_notify_change(TUI_WindowStateID);
            return;

        /* Numbers, function keys, raw characters */
        default:
            if (ch >= '0' && ch <= '9') {
                input.key_number = ch - '0';
                input.has_number = true;
            } else if (ch >= KEY_F(1) && ch <= KEY_F(12)) {
                input.key_function = ch - KEY_F(0);
                input.has_function = true;
            } else {
                input.raw_key = ch;
                input.has_raw_key = true;
            }
            break;
    }

    cels_input_set(ctx, &input);
}

/* ============================================================================
 * ECS System Callback
 * ============================================================================
 *
 * Standalone system (no query terms) registered at OnLoad phase.
 * Runs once per frame inside Engine_Progress().
 */

static void tui_input_system_callback(ecs_iter_t* it) {
    (void)it;
    tui_read_input_ncurses();
}

/* ============================================================================
 * TUI_Input_use -- Provider Registration
 * ============================================================================
 *
 * Called by Use(TUI_Input) inside Build() block.
 * Gets the running pointer from TUI_Window and registers the ECS system.
 */

void TUI_Input_use(TUI_Input config) {
    (void)config;

    /* Get running pointer from window provider for quit signaling */
    g_tui_running_ptr = tui_window_get_running_ptr();

    /* Register ECS system at OnLoad phase
     * Same pattern as CELS_LifecycleSystem in cels.cpp */
    ecs_world_t* world = cels_get_world(cels_get_context());
    ecs_system_desc_t sys_desc = {0};
    ecs_entity_desc_t entity_desc = {0};
    entity_desc.name = "TUI_InputSystem";
    ecs_id_t phase_ids[3] = {
        ecs_pair(EcsDependsOn, EcsOnLoad),
        EcsOnLoad,
        0
    };
    entity_desc.add = phase_ids;
    sys_desc.entity = ecs_entity_init(world, &entity_desc);
    sys_desc.callback = tui_input_system_callback;
    ecs_system_init(world, &sys_desc);
}
