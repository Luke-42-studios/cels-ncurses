/*
 * TUI Component Renderers - ncurses widget rendering for CELS
 *
 * Static inline widget renderers using ncurses color pairs and attributes.
 * Replaces the printf+ANSI approach from examples/tui_renderer/*.h.
 *
 * All functions use mvprintw() for positioned output. wnoutrefresh(stdscr)
 * is called at the end of a full render pass (not per widget).
 * doupdate() happens in the TUI_Window frame loop.
 *
 * Usage:
 *   #include <cels-ncurses/tui_components.h>
 *
 *   tui_render_reset_row();
 *   tui_render_canvas("CELS DEMO");
 *   tui_render_button("Play", true);
 */

#ifndef CELS_NCURSES_TUI_COMPONENTS_H
#define CELS_NCURSES_TUI_COMPONENTS_H

#include <ncurses.h>
#include <string.h>
#include "tui_renderer.h"

/* ============================================================================
 * Render Row Tracking
 * ============================================================================
 *
 * g_render_row tracks the current vertical position for sequential rendering.
 * Reset at the start of each frame with tui_render_reset_row().
 */

static int g_render_row = 1;

/* Reset render row to starting position (call at start of each render frame) */
static inline void tui_render_reset_row(void) {
    g_render_row = 1;
}

/* ============================================================================
 * Canvas Renderer
 * ============================================================================
 *
 * Renders a canvas header with title in a box:
 *   +-----------------------------------------+
 *   |              CELS DEMO                  |
 *   +-----------------------------------------+
 */

#define TUI_CANVAS_WIDTH 43

static inline void tui_render_canvas(const char* title) {
    int title_len = (int)strlen(title);
    int inner_width = TUI_CANVAS_WIDTH - 2;
    int padding_left = (inner_width - title_len) / 2;
    int padding_right = inner_width - title_len - padding_left;

    /* Top border */
    mvprintw(g_render_row++, 0, "+-----------------------------------------+");

    /* Title row with color */
    mvprintw(g_render_row, 0, "|");
    mvprintw(g_render_row, 1, "%*s", padding_left, "");
    attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
    printw("%s", title);
    attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
    printw("%*s|", padding_right, "");
    g_render_row++;

    /* Bottom border */
    mvprintw(g_render_row++, 0, "+-----------------------------------------+");

    /* Blank line after canvas header */
    g_render_row++;
}

/* ============================================================================
 * Button Renderer
 * ============================================================================
 *
 * Selected:     > Play Game            <
 * Not selected:     Play Game
 */

static inline void tui_render_button(const char* label, bool selected) {
    if (selected) {
        attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvprintw(g_render_row, 2, "> %-20s <", label);
        attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
    } else {
        mvprintw(g_render_row, 4, "%-20s", label);
    }
    g_render_row++;
}

/* ============================================================================
 * Cycle Slider Renderer
 * ============================================================================
 *
 * Shows left/right arrows around a cycleable value:
 *   Resolution   [<] 1920 x 1080     [>]
 */

static inline void tui_render_slider_cycle(const char* label, const char* value, bool selected) {
    if (selected) {
        attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvprintw(g_render_row, 2, "%-12s ", label);
        attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);

        printw("[");
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        printw("<");
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
        printw("] ");

        attron(COLOR_PAIR(CP_NORMAL) | A_BOLD);
        printw("%-15s", value);
        attroff(COLOR_PAIR(CP_NORMAL) | A_BOLD);

        printw(" [");
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        printw(">");
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);
        printw("]");
    } else {
        mvprintw(g_render_row, 2, "%-12s ", label);

        attron(A_DIM);
        printw("[<]");
        attroff(A_DIM);

        printw(" ");
        attron(COLOR_PAIR(CP_NORMAL) | A_BOLD);
        printw("%-15s", value);
        attroff(COLOR_PAIR(CP_NORMAL) | A_BOLD);

        printw(" ");
        attron(A_DIM);
        printw("[>]");
        attroff(A_DIM);
    }
    g_render_row++;
}

/* ============================================================================
 * Toggle Slider Renderer
 * ============================================================================
 *
 * Shows ON/OFF states with the active one highlighted:
 *   ON:   [ OFF ][= ON =]     (green)
 *   OFF:  [= OFF =][ ON ]     (red)
 */

static inline void tui_render_slider_toggle(const char* label, bool value, bool selected) {
    /* Label */
    if (selected) {
        attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
        mvprintw(g_render_row, 2, "%-12s ", label);
        attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
    } else {
        mvprintw(g_render_row, 2, "%-12s ", label);
    }

    /* State indicator */
    if (value) {
        /* ON state: dim OFF, bright green ON */
        attron(A_DIM);
        printw("[ OFF ]");
        attroff(A_DIM);
        attron(COLOR_PAIR(CP_ON) | A_BOLD);
        printw("[= ON =]");
        attroff(COLOR_PAIR(CP_ON) | A_BOLD);
    } else {
        /* OFF state: bright red OFF, dim ON */
        attron(COLOR_PAIR(CP_OFF) | A_BOLD);
        printw("[= OFF =]");
        attroff(COLOR_PAIR(CP_OFF) | A_BOLD);
        attron(A_DIM);
        printw("[ ON ]");
        attroff(A_DIM);
    }
    g_render_row++;
}

/* ============================================================================
 * Hint Renderer
 * ============================================================================
 *
 * Renders a controls hint at the bottom with dim text.
 */

static inline void tui_render_hint(const char* text) {
    g_render_row++;  /* blank line before hint */
    attron(A_DIM);
    mvprintw(g_render_row, 0, "%s", text);
    attroff(A_DIM);
    g_render_row++;
}

/* ============================================================================
 * Info Box Renderer
 * ============================================================================
 *
 * Renders a bordered box with window information:
 *   +-----------------------------------------+
 *   |  Title: CELS Demo       Version: 0.1.0  |
 *   +-----------------------------------------+
 */

static inline void tui_render_info_box(const char* title, const char* version) {
    g_render_row++;  /* blank line before box */

    /* Top border */
    mvprintw(g_render_row++, 0, "+-----------------------------------------+");

    /* Info row */
    mvprintw(g_render_row, 0, "| ");
    attron(A_DIM);
    printw("Title: ");
    attroff(A_DIM);
    attron(COLOR_PAIR(CP_HEADER));
    printw("%-16s", title ? title : "Untitled");
    attroff(COLOR_PAIR(CP_HEADER));
    attron(A_DIM);
    printw("Version: ");
    attroff(A_DIM);
    attron(COLOR_PAIR(CP_NORMAL));
    printw("%-5s", version ? version : "?");
    attroff(COLOR_PAIR(CP_NORMAL));
    printw(" |");
    g_render_row++;

    /* Bottom border */
    mvprintw(g_render_row++, 0, "+-----------------------------------------+");
}

#endif /* CELS_NCURSES_TUI_COMPONENTS_H */
