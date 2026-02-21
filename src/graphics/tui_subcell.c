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
 * TUI Sub-Cell Buffer - Shadow buffer lifecycle implementation
 *
 * Implements create/clear/resize/destroy for the per-layer sub-cell shadow
 * buffer. The buffer tracks sub-cell state (half-block colors, quadrant masks,
 * braille dot patterns) so that drawing to one part of a cell preserves the
 * other parts. ncurses cells are atomic -- writing a character replaces the
 * entire cell -- so the shadow buffer is the source of truth.
 */

#include "cels-ncurses/tui_subcell.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * tui_subcell_buffer_create -- Allocate a new shadow buffer
 * ============================================================================
 *
 * malloc for the buffer struct, calloc for the cells array (zero-init sets
 * all modes to TUI_SUBCELL_NONE). Returns NULL on allocation failure.
 */
TUI_SubCellBuffer* tui_subcell_buffer_create(int width, int height) {
    TUI_SubCellBuffer* buf = malloc(sizeof(TUI_SubCellBuffer));
    if (!buf) return NULL;

    buf->width = width;
    buf->height = height;
    buf->cells = calloc((size_t)(width * height), sizeof(TUI_SubCell));
    if (!buf->cells) {
        free(buf);
        return NULL;
    }

    return buf;
}

/* ============================================================================
 * tui_subcell_buffer_clear -- Reset all cells to NONE
 * ============================================================================
 *
 * memset to zero resets all modes to TUI_SUBCELL_NONE (enum value 0) and
 * clears all mode-specific state. Called from tui_frame_begin alongside
 * werase for dirty layers.
 */
void tui_subcell_buffer_clear(TUI_SubCellBuffer* buf) {
    if (!buf || !buf->cells) return;
    memset(buf->cells, 0, (size_t)(buf->width * buf->height) * sizeof(TUI_SubCell));
}

/* ============================================================================
 * tui_subcell_buffer_resize -- Reallocate for new dimensions
 * ============================================================================
 *
 * Frees the old cells array and allocates a new one at the new dimensions.
 * Content is discarded (new array is zero-initialized). Called from
 * tui_layer_resize when the layer dimensions change.
 */
void tui_subcell_buffer_resize(TUI_SubCellBuffer* buf, int width, int height) {
    if (!buf) return;

    TUI_SubCell* new_cells = calloc((size_t)(width * height), sizeof(TUI_SubCell));
    if (!new_cells) return;

    free(buf->cells);
    buf->cells = new_cells;
    buf->width = width;
    buf->height = height;
}

/* ============================================================================
 * tui_subcell_buffer_destroy -- Free buffer and cells
 * ============================================================================
 *
 * Frees the cells array, then the buffer struct itself. NULL-safe.
 */
void tui_subcell_buffer_destroy(TUI_SubCellBuffer* buf) {
    if (!buf) return;
    free(buf->cells);
    free(buf);
}
