/*
 * TUI Scissor - Clip region stack implementation
 *
 * Implements a 16-deep stack of clip rectangles for nested clipping.
 * Push intersects the new rect with the current top-of-stack. Pop restores
 * the previous clip. Reset clears the stack and initializes the base clip
 * to the full drawable area from the draw context.
 *
 * ctx->clip is updated on every push, pop, and reset so draw functions
 * always see the current effective clip region.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 * Static variables are per-consumer translation unit.
 */

#include "cels-ncurses/tui_scissor.h"

/* ============================================================================
 * Static State
 * ============================================================================
 *
 * The scissor stack is a fixed-size array with a stack pointer.
 * g_scissor_stack[0] is always the base clip (full drawable area).
 * g_scissor_sp indexes the current top of stack.
 */

static TUI_CellRect g_scissor_stack[TUI_SCISSOR_STACK_MAX];
static int g_scissor_sp = 0;

/* ============================================================================
 * Scissor Stack API
 * ============================================================================ */

/*
 * Reset the scissor stack and set base clip to the full drawable area.
 * Call once per frame before any push/pop operations.
 */
void tui_scissor_reset(TUI_DrawContext* ctx) {
    g_scissor_stack[0] = (TUI_CellRect){
        ctx->x, ctx->y, ctx->width, ctx->height
    };
    g_scissor_sp = 0;
    ctx->clip = g_scissor_stack[0];
}

/*
 * Push a new clip rect onto the stack.
 * The effective clip is the intersection of rect and the current top.
 * If the stack is full (TUI_SCISSOR_STACK_MAX - 1 reached), the push
 * is silently ignored.
 */
void tui_push_scissor(TUI_DrawContext* ctx, TUI_CellRect rect) {
    if (g_scissor_sp >= TUI_SCISSOR_STACK_MAX - 1) return;

    TUI_CellRect clipped = tui_cell_rect_intersect(rect,
                                                     g_scissor_stack[g_scissor_sp]);
    g_scissor_sp++;
    g_scissor_stack[g_scissor_sp] = clipped;
    ctx->clip = clipped;
}

/*
 * Pop the top clip rect from the stack, restoring the previous clip.
 * If already at the base level (sp == 0), the pop is silently ignored.
 */
void tui_pop_scissor(TUI_DrawContext* ctx) {
    if (g_scissor_sp > 0) {
        g_scissor_sp--;
    }
    ctx->clip = g_scissor_stack[g_scissor_sp];
}
