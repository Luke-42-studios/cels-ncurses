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
 * TUI Color - Three-tier color system with style and attribute flags
 *
 * Provides the foundational color and style system for the TUI graphics API.
 * TUI_Color wraps a color index whose meaning depends on the detected color
 * mode. tui_color_rgb() resolves RGB values based on terminal capabilities:
 *
 *   - Direct color mode:  index = packed 0xRRGGBB (exact 24-bit color)
 *   - Palette mode:       index = palette slot 16-255 (redefined to exact RGB)
 *   - 256-color mode:     index = nearest xterm-256 color index (fallback)
 *
 * Color mode is auto-detected at startup (after start_color()) and can be
 * overridden via TUI_Window.color_mode or Engine_Config.color_mode.
 *
 * TUI_Style combines foreground/background colors with attribute flags for
 * bold, dim, underline, reverse, and italic. Color pairs are managed
 * invisibly by ncurses alloc_pair() -- the developer never sees pair numbers.
 * tui_style_apply() sets attributes and color pair atomically via wattr_set()
 * with extended pair support (opts pointer for pairs > 255).
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
 * Color Mode
 * ============================================================================
 *
 * Detected at startup, determines how tui_color_rgb() resolves RGB values.
 * Can be overridden via TUI_Window.color_mode or Engine_Config.color_mode.
 */

typedef enum TUI_ColorMode {
    TUI_COLOR_MODE_256,        /* Default: nearest xterm-256 mapping */
    TUI_COLOR_MODE_PALETTE,    /* can_change_color: redefine palette slots 16-255 */
    TUI_COLOR_MODE_DIRECT      /* RGB flag: packed 0xRRGGBB values */
} TUI_ColorMode;

/* ============================================================================
 * Color Type
 * ============================================================================
 *
 * Wraps a color index. The meaning of index depends on the active color mode:
 *   -1            = terminal default (all modes)
 *   0-255         = xterm-256 color index (256-color mode)
 *   16-255        = palette slot redefined to exact RGB (palette mode)
 *   0x000000-0xFFFFFF = packed RGB value (direct color mode)
 *
 * Created from RGB via tui_color_rgb() which resolves based on detected mode.
 * Use TUI_COLOR_DEFAULT for the terminal's default foreground or background.
 */

typedef struct TUI_Color {
    int index;  /* -1 = terminal default, interpretation depends on color mode */
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
 * Resolves based on detected color mode:
 *   Direct:  packed 0xRRGGBB index
 *   Palette: allocates palette slot, redefines to exact RGB
 *   256:     nearest xterm-256 color index (fallback) */
extern TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Apply a style atomically to an ncurses WINDOW.
 * Uses alloc_pair for color pair resolution and wattr_set with opts
 * pointer for extended pair support. Never uses attron/attroff. */
extern void tui_style_apply(WINDOW* win, TUI_Style style);

/* Initialize color system. Call once after start_color().
 * Detects terminal color capabilities and sets the resolution mode.
 * Pass -1 for auto-detection, or a TUI_ColorMode value to force a mode. */
extern void tui_color_init(int mode_override);

/* Query the active color mode (returns TUI_ColorMode value). */
extern TUI_ColorMode tui_color_get_mode(void);

/* Human-readable name for a color mode ("256", "palette", "direct"). */
extern const char* tui_color_mode_name(TUI_ColorMode mode);

#endif /* CELS_NCURSES_TUI_COLOR_H */
