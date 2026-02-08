/*
 * TUI Renderer - ncurses rendering utilities for CELS
 *
 * Provides color pair constants and the render provider initialization
 * function. Color pairs are initialized by TUI_Window on startup.
 *
 * Usage:
 *   #include <cels-ncurses/tui_renderer.h>
 *
 *   attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
 *   mvprintw(row, col, "> %s <", label);
 *   attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
 */

#ifndef CELS_NCURSES_TUI_RENDERER_H
#define CELS_NCURSES_TUI_RENDERER_H

#include <ncurses.h>

/* ============================================================================
 * Color Pair Constants
 * ============================================================================
 *
 * These match the init_pair() calls in tui_window.c startup.
 * Use with COLOR_PAIR(CP_X) and attron/attroff.
 */
enum {
    CP_SELECTED = 1,   /* Yellow on default -- selected items */
    CP_HEADER   = 2,   /* Cyan on default -- headers/titles */
    CP_ON       = 3,   /* Green on default -- ON state */
    CP_OFF      = 4,   /* Red on default -- OFF state */
    CP_NORMAL   = 5,   /* White on default -- normal bright text */
    CP_DIM      = 6    /* Default on default -- dim text (use A_DIM) */
};

/* Initialize the TUI render provider (Feature/Provides registration) */
extern void tui_renderer_init(void);

#endif /* CELS_NCURSES_TUI_RENDERER_H */
