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
 * TUI Color - Three-tier color system implementation
 *
 * Implements the color and style system for the TUI graphics API.
 * Color resolution depends on the detected terminal color mode:
 *
 *   Direct color mode:  tui_color_rgb() returns packed 0xRRGGBB index
 *   Palette mode:       tui_color_rgb() allocates palette slot, redefines to exact RGB
 *   256-color mode:     tui_color_rgb() maps to nearest xterm-256 index (fallback)
 *
 * Mode is detected once at startup via tui_color_init(), which checks
 * tigetflag("RGB") > COLORTERM env > can_change_color() > 256-color fallback.
 *
 * Style application uses alloc_pair() for dynamic color pair management
 * and wattr_set() with opts pointer for extended pair support (pairs > 255).
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * Static variables are per-consumer translation unit.
 */

#include "cels-ncurses/tui_color.h"
#include <ncurses.h>
#include <stdint.h>
#include <string.h>  /* strcmp */
#include <stdlib.h>  /* getenv */
#include <term.h>    /* tigetflag */

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

static TUI_ColorMode g_color_mode = TUI_COLOR_MODE_256;

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
 * Palette Slot Allocator (TUI_COLOR_MODE_PALETTE only)
 * ============================================================================
 *
 * Manages palette slots 16-255 (240 usable). Slots 0-15 are the user's
 * terminal colorscheme and must not be touched. Uses LRU eviction when
 * all slots are occupied.
 */

#define PALETTE_FIRST 16
#define PALETTE_LAST  255
#define PALETTE_SLOTS (PALETTE_LAST - PALETTE_FIRST + 1)  /* 240 */

typedef struct {
    uint8_t r, g, b;
    int slot;           /* palette index 16-255 */
    int lru_counter;    /* for eviction */
} PaletteEntry;

static PaletteEntry g_palette[PALETTE_SLOTS];
static int g_palette_count = 0;
static int g_lru_clock = 0;

static TUI_Color tui_color_palette_alloc(uint8_t r, uint8_t g, uint8_t b) {
    /* 1. Check if this exact RGB is already allocated */
    for (int i = 0; i < g_palette_count; i++) {
        if (g_palette[i].r == r && g_palette[i].g == g && g_palette[i].b == b) {
            g_palette[i].lru_counter = ++g_lru_clock;
            return (TUI_Color){ .index = g_palette[i].slot };
        }
    }

    /* 2. Allocate new slot or evict LRU */
    int target;
    if (g_palette_count < PALETTE_SLOTS) {
        target = g_palette_count++;
    } else {
        /* Find least recently used */
        target = 0;
        for (int i = 1; i < PALETTE_SLOTS; i++) {
            if (g_palette[i].lru_counter < g_palette[target].lru_counter)
                target = i;
        }
    }

    int slot = PALETTE_FIRST + target;
    /* init_extended_color uses 0-1000 scale, NOT 0-255 */
    init_extended_color(slot, r * 1000 / 255, g * 1000 / 255, b * 1000 / 255);

    g_palette[target] = (PaletteEntry){
        .r = r, .g = g, .b = b,
        .slot = slot,
        .lru_counter = ++g_lru_clock
    };
    return (TUI_Color){ .index = slot };
}

/* ============================================================================
 * Public API
 * ============================================================================ */

TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    switch (g_color_mode) {
    case TUI_COLOR_MODE_DIRECT:
        /* Pack RGB into a single int -- ncurses treats as color index */
        return (TUI_Color){ .index = (r << 16) | (g << 8) | b };

    case TUI_COLOR_MODE_PALETTE:
        /* Allocate palette slot and redefine to exact RGB */
        return tui_color_palette_alloc(r, g, b);

    case TUI_COLOR_MODE_256:
    default:
        /* Existing behavior: nearest xterm-256 index */
        return (TUI_Color){ .index = rgb_to_nearest_256(r, g, b) };
    }
}

/* Convert TUI_ATTR flags to ncurses attr_t */
static attr_t tui_attrs_to_ncurses(uint32_t flags) {
    attr_t a = A_NORMAL;
    if (flags & TUI_ATTR_BOLD)      a |= A_BOLD;
    if (flags & TUI_ATTR_DIM)       a |= A_DIM;
    if (flags & TUI_ATTR_UNDERLINE) a |= A_UNDERLINE;
    if (flags & TUI_ATTR_REVERSE)   a |= A_REVERSE;
#ifdef A_ITALIC
    if (flags & TUI_ATTR_ITALIC)   a |= A_ITALIC;
#endif
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
    wattr_set(win, attrs, 0, &pair);
}

void tui_color_init(int mode_override) {
    if (mode_override >= 0) {
        g_color_mode = (TUI_ColorMode)mode_override;
        return;
    }

    /* Auto-detect: check tigetflag("RGB") for direct color */
    if (tigetflag("RGB") == 1) {
        g_color_mode = TUI_COLOR_MODE_DIRECT;
        return;
    }

    /* Check COLORTERM env var as hint for palette mode preference */
    const char* colorterm = getenv("COLORTERM");
    if (colorterm) {
        if (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0) {
            if (can_change_color()) {
                g_color_mode = TUI_COLOR_MODE_PALETTE;
                return;
            }
        }
    }

    /* Check can_change_color() for palette redefinition */
    if (can_change_color()) {
        g_color_mode = TUI_COLOR_MODE_PALETTE;
        return;
    }

    /* Final fallback */
    g_color_mode = TUI_COLOR_MODE_256;
}

TUI_ColorMode tui_color_get_mode(void) {
    return g_color_mode;
}

const char* tui_color_mode_name(TUI_ColorMode mode) {
    switch (mode) {
    case TUI_COLOR_MODE_256:     return "256";
    case TUI_COLOR_MODE_PALETTE: return "palette";
    case TUI_COLOR_MODE_DIRECT:  return "direct";
    default:                     return "unknown";
    }
}
