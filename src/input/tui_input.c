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

        /* WASD (raw character + axis) */
        case 'w': case 'W': input.axis_left[1] = -1.0f; input.raw_key = ch; input.has_raw_key = true; break;
        case 's': case 'S': input.axis_left[1] =  1.0f; input.raw_key = ch; input.has_raw_key = true; break;
        case 'a': case 'A': input.axis_left[0] = -1.0f; input.raw_key = ch; input.has_raw_key = true; break;
        case 'd': case 'D': input.axis_left[0] =  1.0f; input.raw_key = ch; input.has_raw_key = true; break;

        /* Action buttons */
        case '\n': case '\r': case KEY_ENTER:
            input.button_accept = true; break;
        case 27:  /* Escape */
            input.button_cancel = true;
            input.raw_key = 27; input.has_raw_key = true; break;

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

        /* Quit key -- signals window to close (unless quit guard suppresses) */
        case 'q': case 'Q':
            if (g_quit_guard_fn && g_quit_guard_fn()) {
                /* Quit suppressed (e.g., text input active) -- pass as raw_key */
                input.raw_key = ch;
                input.has_raw_key = true;
                break;
            }
            if (g_tui_running_ptr) {
                *g_tui_running_ptr = 0;
            }
            return;  /* Do NOT set any input field -- immediate quit */

        /* Terminal resize -- drain queued resize events (window manager sends
         * many during a drag), then process only the final dimensions. */
        case KEY_RESIZE: {
            /* Drain additional KEY_RESIZE events queued during rapid drag */
            int next;
            while ((next = wgetch(stdscr)) == KEY_RESIZE) { /* consume */ }
            if (next != ERR) ungetch(next);  /* put back non-resize key */

            /* Skip if dimensions are invalid (can happen mid-animation) */
            if (COLS < 4 || LINES < 4) {
                cels_input_set(ctx, &input);
                return;
            }

            fprintf(stderr, "[resize] COLS=%d LINES=%d (was %dx%d)\n",
                    COLS, LINES, TUI_WindowState.width, TUI_WindowState.height);

            /* Backend-specific cleanup: resize ncurses layers and invalidate.
             * Dimension state (Engine_WindowState, CEL_Window) is NOT updated
             * here -- tui_hook_frame_begin() is the single authority for
             * dimension updates. This keeps the pattern portable: each
             * backend's frame_begin polls dimensions and updates CEL_Window. */
            tui_layer_resize_all(COLS, LINES);
            tui_frame_invalidate_all();
            clearok(curscr, TRUE);
            cels_input_set(ctx, &input);  /* Set clean (zeroed) input */
            return;
        }

        /* Ctrl+Arrow keys (custom key codes via define_key) */
        case CELS_KEY_CTRL_UP:    input.raw_key = CELS_KEY_CTRL_UP;    input.has_raw_key = true; break;
        case CELS_KEY_CTRL_DOWN:  input.raw_key = CELS_KEY_CTRL_DOWN;  input.has_raw_key = true; break;
        case CELS_KEY_CTRL_LEFT:  input.raw_key = CELS_KEY_CTRL_LEFT;  input.has_raw_key = true; break;
        case CELS_KEY_CTRL_RIGHT: input.raw_key = CELS_KEY_CTRL_RIGHT; input.has_raw_key = true; break;

        /* Shift+Arrow keys (for future text selection) */
        case CELS_KEY_SHIFT_LEFT:  input.raw_key = CELS_KEY_SHIFT_LEFT;  input.has_raw_key = true; break;
        case CELS_KEY_SHIFT_RIGHT: input.raw_key = CELS_KEY_SHIFT_RIGHT; input.has_raw_key = true; break;

        /* F1 -- Pause for text selection/copy */
        case KEY_F(1):
            tui_pause();
            cels_input_set(ctx, &input);  /* Clean frame after resume */
            return;

        /* Numbers, function keys, raw characters */
        default:
            if (ch >= '0' && ch <= '9') {
                input.key_number = ch - '0';
                input.has_number = true;
            } else if (ch >= KEY_F(2) && ch <= KEY_F(12)) {
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

    /* Register xterm-compatible Ctrl+Arrow escape sequences */
    define_key("\033[1;5A", CELS_KEY_CTRL_UP);
    define_key("\033[1;5B", CELS_KEY_CTRL_DOWN);
    define_key("\033[1;5C", CELS_KEY_CTRL_RIGHT);
    define_key("\033[1;5D", CELS_KEY_CTRL_LEFT);

    /* Register xterm-compatible Shift+Arrow escape sequences */
    define_key("\033[1;2D", CELS_KEY_SHIFT_LEFT);
    define_key("\033[1;2C", CELS_KEY_SHIFT_RIGHT);

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

/* ============================================================================
 * Quit Guard API
 * ============================================================================ */

void tui_input_set_quit_guard(bool (*guard_fn)(void)) {
    g_quit_guard_fn = guard_fn;
}
