/*
 * TUI Color - Color types, style struct, and attribute flags
 *
 * Provides the foundational color and style system for the TUI graphics API.
 * TUI_Color wraps an xterm-256 color index (resolved eagerly from RGB at
 * creation time). TUI_Style combines foreground/background colors with
 * attribute flags for bold, dim, underline, and reverse.
 *
 * Color pairs are managed invisibly by ncurses alloc_pair() -- the developer
 * never sees pair numbers. tui_style_apply() sets attributes and color pair
 * atomically via wattr_set(), replacing the old attron/attroff pattern.
 *
 * Usage:
 *   #include <cels-ncurses/tui_color.h>
 *
 *   TUI_Color red = tui_color_rgb(255, 0, 0);
 *   TUI_Color bg  = TUI_COLOR_DEFAULT;
 *
 *   TUI_Style style = {
 *       .fg = red,
 *       .bg = bg,
 *       .attrs = TUI_ATTR_BOLD | TUI_ATTR_UNDERLINE
 *   };
 *
 *   tui_style_apply(my_window, style);
 *   mvwprintw(my_window, 0, 0, "Styled text");
 */

#ifndef CELS_NCURSES_TUI_COLOR_H
#define CELS_NCURSES_TUI_COLOR_H

#include <ncurses.h>
#include <stdint.h>

/* ============================================================================
 * Color Type
 * ============================================================================
 *
 * Wraps an xterm-256 color index. Created from RGB via tui_color_rgb() which
 * eagerly resolves to the nearest color index at creation time.
 * Use TUI_COLOR_DEFAULT for the terminal's default foreground or background.
 */

typedef struct TUI_Color {
    int index;  /* -1 = terminal default, 0-255 = xterm-256 color index */
} TUI_Color;

#define TUI_COLOR_DEFAULT ((TUI_Color){ .index = -1 })

/* ============================================================================
 * Attribute Flags
 * ============================================================================
 *
 * Bitflags for text attributes. Combine with bitwise OR in TUI_Style.attrs.
 * Converted to ncurses attr_t internally by tui_style_apply().
 */

#define TUI_ATTR_NORMAL    0x00
#define TUI_ATTR_BOLD      0x01
#define TUI_ATTR_DIM       0x02
#define TUI_ATTR_UNDERLINE 0x04
#define TUI_ATTR_REVERSE   0x08
#define TUI_ATTR_ITALIC    0x10

/* ============================================================================
 * Style Type
 * ============================================================================
 *
 * Combines foreground color, background color, and attribute flags into a
 * single value. Stack-allocated, passed by value to tui_style_apply().
 */

typedef struct TUI_Style {
    TUI_Color fg;
    TUI_Color bg;
    uint32_t attrs;
} TUI_Style;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/* Create a color from RGB values (0-255 per channel).
 * Eagerly maps to nearest xterm-256 color index at creation time. */
extern TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Apply a style atomically to an ncurses WINDOW.
 * Uses alloc_pair for color pair resolution and wattr_set for
 * atomic attribute application. Never uses attron/attroff. */
extern void tui_style_apply(WINDOW* win, TUI_Style style);

#endif /* CELS_NCURSES_TUI_COLOR_H */
