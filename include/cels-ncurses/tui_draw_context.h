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
 * TUI Draw Context - Draw context wrapping an ncurses WINDOW
 *
 * Provides a lightweight context struct that wraps an ncurses WINDOW* with
 * position, dimensions, and clipping state. TUI_DrawContext is the first
 * parameter to all draw functions in Phase 2+. It carries everything a draw
 * call needs: the target window, the origin offset, the drawable area size,
 * and the current effective clip rect.
 *
 * TUI_DrawContext borrows (does not own) the WINDOW* -- callers manage
 * WINDOW lifetime. The struct is a stack-allocated value type with no
 * malloc/free churn per frame.
 *
 * Usage:
 *   #include <cels-ncurses/tui_draw_context.h>
 *
 *   TUI_DrawContext ctx = tui_draw_context_create(my_window, 0, 0, 80, 24);
 *   tui_draw_rect(&ctx, rect, style);
 *   tui_draw_text(&ctx, 5, 3, "Hello", style);
 */

#ifndef CELS_NCURSES_TUI_DRAW_CONTEXT_H
#define CELS_NCURSES_TUI_DRAW_CONTEXT_H

#include <ncurses.h>
#include "cels-ncurses/tui_types.h"

/* ============================================================================
 * Draw Context
 * ============================================================================
 *
 * All draw functions in Phase 2+ take TUI_DrawContext* as their first
 * parameter. No global "current context" -- caller passes the target
 * explicitly. The clip field represents the current effective clip rect.
 * Phase 2's scissor stack will update this field.
 */
typedef struct TUI_SubCellBuffer TUI_SubCellBuffer;

typedef struct TUI_DrawContext {
    WINDOW* win;          /* Borrowed -- caller manages lifetime */
    int x, y;             /* Origin offset within the window */
    int width, height;    /* Drawable area dimensions */
    TUI_CellRect clip;    /* Current effective clip rect (set by scissor stack in Phase 2) */
    TUI_SubCellBuffer** subcell_buf; /* Pointer to layer's buffer pointer (NULL for non-layer contexts) */
} TUI_DrawContext;

/* ============================================================================
 * Creation
 * ============================================================================ */

/*
 * Create a draw context wrapping an ncurses WINDOW.
 * Returns a stack-allocated context with clip initialized to the full
 * drawable area. The WINDOW* is borrowed, not owned.
 */
static inline TUI_DrawContext tui_draw_context_create(WINDOW* win,
                                                      int x, int y,
                                                      int width, int height) {
    TUI_DrawContext ctx;
    ctx.win = win;
    ctx.x = x;
    ctx.y = y;
    ctx.width = width;
    ctx.height = height;
    ctx.clip = (TUI_CellRect){ .x = x, .y = y, .w = width, .h = height };
    ctx.subcell_buf = NULL;
    return ctx;
}

#endif /* CELS_NCURSES_TUI_DRAW_CONTEXT_H */
