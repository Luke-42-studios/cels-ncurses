/*
 * TUI Color - Color pool implementation and RGB-to-nearest-256 mapping
 *
 * Implements the color and style system for the TUI graphics API.
 * RGB values are eagerly mapped to the nearest xterm-256 color index
 * at creation time. Style application uses alloc_pair() for dynamic
 * color pair management and wattr_set() for atomic attribute setting.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * Static variables are per-consumer translation unit.
 */

#include "cels-ncurses/tui_color.h"
#include <ncurses.h>
#include <stdint.h>

/* ============================================================================
 * xterm-256 Color Mapping
 * ============================================================================
 *
 * The xterm 256-color palette is arranged as:
 *   0-15:    Standard + bright terminal colors (theme-dependent)
 *   16-231:  6x6x6 color cube with levels [0, 95, 135, 175, 215, 255]
 *   232-255: Greyscale ramp from 8 to 238 in steps of 10
 *
 * rgb_to_nearest_256 maps an arbitrary RGB value to the closest index
 * by comparing squared Euclidean distance against both the color cube
 * and greyscale ramp, returning whichever is closer.
 */

static const uint8_t CUBE_LEVELS[6] = { 0, 95, 135, 175, 215, 255 };

/* Map a single 0-255 component to nearest cube index (0-5) */
static int nearest_cube_index(uint8_t val) {
    if (val < 48)  return 0;  /* midpoint(0, 95)   = 47.5 */
    if (val < 115) return 1;  /* midpoint(95, 135)  = 115  */
    if (val < 155) return 2;  /* midpoint(135, 175) = 155  */
    if (val < 195) return 3;  /* midpoint(175, 215) = 195  */
    if (val < 235) return 4;  /* midpoint(215, 255) = 235  */
    return 5;
}

/* Map RGB (0-255 per channel) to nearest xterm-256 color index */
static int rgb_to_nearest_256(uint8_t r, uint8_t g, uint8_t b) {
    /* Try color cube (16-231) */
    int ci = nearest_cube_index(r);
    int gi = nearest_cube_index(g);
    int bi = nearest_cube_index(b);
    int cube_color = 16 + (ci * 36) + (gi * 6) + bi;
    uint8_t cr = CUBE_LEVELS[ci], cg = CUBE_LEVELS[gi], cb = CUBE_LEVELS[bi];
    int cube_dist = (r - cr) * (r - cr) + (g - cg) * (g - cg) + (b - cb) * (b - cb);

    /* Try greyscale ramp (232-255) */
    int grey_avg = (r + g + b) / 3;
    int grey_idx = (grey_avg - 8) / 10;
    if (grey_idx < 0) grey_idx = 0;
    if (grey_idx > 23) grey_idx = 23;
    int grey_val = 8 + grey_idx * 10;
    int grey_dist = (r - grey_val) * (r - grey_val)
                  + (g - grey_val) * (g - grey_val)
                  + (b - grey_val) * (b - grey_val);

    return (grey_dist < cube_dist) ? (232 + grey_idx) : cube_color;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (TUI_Color){ .index = rgb_to_nearest_256(r, g, b) };
}

/* Convert TUI_ATTR flags to ncurses attr_t */
static attr_t tui_attrs_to_ncurses(uint32_t flags) {
    attr_t a = A_NORMAL;
    if (flags & TUI_ATTR_BOLD)      a |= A_BOLD;
    if (flags & TUI_ATTR_DIM)       a |= A_DIM;
    if (flags & TUI_ATTR_UNDERLINE) a |= A_UNDERLINE;
    if (flags & TUI_ATTR_REVERSE)   a |= A_REVERSE;
    return a;
}

void tui_style_apply(WINDOW* win, TUI_Style style) {
    int pair;
    if (style.fg.index == -1 && style.bg.index == -1) {
        pair = 0;  /* Default pair -- no alloc needed */
    } else {
        pair = alloc_pair(style.fg.index, style.bg.index);
    }
    attr_t attrs = tui_attrs_to_ncurses(style.attrs);
    wattr_set(win, attrs, (short)pair, NULL);
}
