/*
 * TUI Scissor - Clip region stack for nested clipping
 *
 * Manages a stack of clip rectangles for nested clipping regions.
 * Push intersects a new rect with the current top-of-stack, narrowing the
 * clip region. Pop restores the previous clip region. TUI_DrawContext.clip
 * is updated on every push, pop, and reset so that draw functions always
 * see the current effective clip.
 *
 * Stack depth is TUI_SCISSOR_STACK_MAX (16 levels), sufficient for
 * practical UI hierarchies (scroll containers, nested panels, etc.).
 *
 * Frame lifecycle: call tui_scissor_reset at frame start to clear stale
 * state and initialize the base clip to the full drawable area.
 *
 * Usage:
 *   #include "cels-ncurses/tui_scissor.h"
 *
 *   tui_scissor_reset(&ctx);
 *   tui_push_scissor(&ctx, outer_region);
 *   -- draw calls clipped to outer_region --
 *   tui_push_scissor(&ctx, inner_region);
 *   -- draw calls clipped to intersection of outer and inner --
 *   tui_pop_scissor(&ctx);
 *   -- back to outer_region clipping --
 *   tui_pop_scissor(&ctx);
 *   -- back to full drawable area --
 */

#ifndef CELS_NCURSES_TUI_SCISSOR_H
#define CELS_NCURSES_TUI_SCISSOR_H

#include "cels-ncurses/tui_draw_context.h"
#include "cels-ncurses/tui_types.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TUI_SCISSOR_STACK_MAX 16

/* ============================================================================
 * Scissor Stack API
 * ============================================================================
 *
 * tui_scissor_reset  -- Clear stack and set base clip to full drawable area.
 *                       Call once per frame before any push/pop.
 * tui_push_scissor   -- Push a new clip rect (intersected with current top).
 * tui_pop_scissor    -- Pop the top clip rect, restoring the previous one.
 */

extern void tui_scissor_reset(TUI_DrawContext* ctx);
extern void tui_push_scissor(TUI_DrawContext* ctx, TUI_CellRect rect);
extern void tui_pop_scissor(TUI_DrawContext* ctx);

#endif /* CELS_NCURSES_TUI_SCISSOR_H */
