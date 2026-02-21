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
 * TUI Sub-Cell - Sub-cell rendering types and shadow buffer infrastructure
 *
 * Provides the type system and buffer lifecycle for sub-cell rendering modes
 * that achieve pixel resolutions finer than one terminal character cell:
 *
 *   - Half-block: 1x2 virtual pixels per cell (U+2580/U+2584)
 *   - Quadrant:   2x2 virtual pixels per cell (U+2596-U+259F + block chars)
 *   - Braille:    2x4 virtual pixels per cell (U+2800-U+28FF)
 *
 * Each cell in the shadow buffer tracks its current sub-cell mode and
 * mode-specific state (colors, dot masks, quadrant masks). The buffer is
 * per-layer, lazily allocated on first sub-cell draw, cleared alongside
 * werase in frame_begin, resized with the layer, and freed on destroy.
 *
 * Usage:
 *   #include <cels-ncurses/tui_subcell.h>
 *
 *   TUI_SubCellBuffer* buf = tui_subcell_buffer_create(80, 24);
 *   // ... draw sub-cell content via tui_draw_halfblock_plot() etc ...
 *   tui_subcell_buffer_clear(buf);   // reset at frame begin
 *   tui_subcell_buffer_destroy(buf); // free at layer destroy
 */

#ifndef CELS_NCURSES_TUI_SUBCELL_H
#define CELS_NCURSES_TUI_SUBCELL_H

#include <stdbool.h>
#include <stdint.h>
#include "cels-ncurses/tui_color.h"

/* ============================================================================
 * Sub-Cell Mode
 * ============================================================================
 *
 * Each cell is in exactly one sub-cell mode at a time. NONE means the cell
 * has no sub-cell content (normal character). Mode mixing uses last-mode-wins:
 * drawing braille over a half-block cell replaces it entirely.
 */
typedef enum TUI_SubCellMode {
    TUI_SUBCELL_NONE = 0,   /* Cell has no sub-cell content (normal character) */
    TUI_SUBCELL_HALFBLOCK,  /* Half-block mode: top/bottom colors */
    TUI_SUBCELL_QUADRANT,   /* Quadrant mode: 4 quadrants, 2-color constraint */
    TUI_SUBCELL_BRAILLE     /* Braille mode: 8 dots on/off */
} TUI_SubCellMode;

/* ============================================================================
 * TUI_SubCell -- Per-cell sub-cell state
 * ============================================================================
 *
 * Tagged union discriminated by mode. Each cell stores mode-specific state:
 *   - halfblock: independent top/bottom colors with has_top/has_bottom flags
 *   - quadrant: 4-bit mask + fg/bg colors (2-color constraint per cell)
 *   - braille: 8-bit dot bitmask + fg color
 */
typedef struct TUI_SubCell {
    TUI_SubCellMode mode;
    union {
        struct {
            TUI_Color top;      /* Color for upper half */
            TUI_Color bottom;   /* Color for lower half */
            bool has_top;       /* Whether top half has been drawn */
            bool has_bottom;    /* Whether bottom half has been drawn */
        } halfblock;
        struct {
            uint8_t mask;       /* Bitmask: bit0=UL, bit1=UR, bit2=LL, bit3=LR */
            TUI_Color fg;       /* Foreground color (filled quadrants) */
            TUI_Color bg;       /* Background color (empty quadrants) */
        } quadrant;
        struct {
            uint8_t dots;       /* Bitmask: 8 bits for 8 dot positions */
            TUI_Color fg;       /* Dot color (foreground) */
        } braille;
    };
} TUI_SubCell;

/* ============================================================================
 * TUI_SubCellBuffer -- Per-layer shadow buffer
 * ============================================================================
 *
 * Grid of TUI_SubCell entries, one per terminal cell. Dimensions match the
 * layer's window dimensions. Allocated lazily on first sub-cell draw,
 * cleared at frame_begin alongside werase, resized with the layer.
 */
typedef struct TUI_SubCellBuffer {
    TUI_SubCell* cells;     /* width * height array */
    int width, height;      /* Matches layer dimensions (in terminal cells) */
} TUI_SubCellBuffer;

/* ============================================================================
 * Buffer Lifecycle API
 * ============================================================================ */

/* Allocate a new sub-cell buffer with given dimensions.
 * All cells are zero-initialized (mode = TUI_SUBCELL_NONE).
 * Returns NULL on allocation failure. */
extern TUI_SubCellBuffer* tui_subcell_buffer_create(int width, int height);

/* Reset all cells to TUI_SUBCELL_NONE (zero-fill).
 * Called from tui_frame_begin alongside werase. NULL-safe. */
extern void tui_subcell_buffer_clear(TUI_SubCellBuffer* buf);

/* Resize buffer to new dimensions, discarding old content.
 * Called from tui_layer_resize. NULL-safe. */
extern void tui_subcell_buffer_resize(TUI_SubCellBuffer* buf, int width, int height);

/* Free buffer memory (cells array + buffer struct).
 * Called from tui_layer_destroy. NULL-safe. */
extern void tui_subcell_buffer_destroy(TUI_SubCellBuffer* buf);

#endif /* CELS_NCURSES_TUI_SUBCELL_H */
