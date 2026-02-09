/*
 * Minimal CELS ncurses Example
 *
 * The absolute minimum to get a CELS TUI app running:
 * - CEL_Build sets up the application
 * - TUI_Engine_use configures the ncurses backend
 * - tui_widgets_register enables the standard widget renderers
 * - CEL_Root re-renders on state changes
 * - Press 'q' to quit (built into the TUI input provider)
 *
 * Build & run:
 *   ./modules/cels-ncurses/examples/run_minimal.sh
 */

#define _POSIX_C_SOURCE 200809L
#include <cels/cels.h>
#include <cels-widgets/widgets.h>
#include <cels-ncurses/tui_engine.h>
#include <cels-ncurses/tui_widgets.h>

/* ============================================================================
 * Compositions
 * ============================================================================ */

/* Content area: centered greeting */
#define HelloContent(...) CEL_Init(HelloContent, __VA_ARGS__)
CEL_Composition(HelloContent) {
    (void)props;
    CEL_Has(W_TabContent,
        .text = "Hello CELS!",
        .hint = "This is a minimal cels-ncurses example"
    );
}

/* Status bar: quit hint at the bottom */
#define HelloStatus(...) CEL_Init(HelloStatus, __VA_ARGS__)
CEL_Composition(HelloStatus) {
    (void)props;
    CEL_Has(W_StatusBar,
        .left  = "minimal example",
        .right = "q:quit "
    );
}

/* ============================================================================
 * Root Composition
 * ============================================================================ */

CEL_Root(AppUI, TUI_EngineContext) {
    TUI_WindowState_t* win = CEL_WatchId(ctx.windowState, TUI_WindowState_t);

    if (win->state == WINDOW_STATE_READY) {
        HelloContent() {}
        HelloStatus() {}
    }
}

/* ============================================================================
 * Application Entry Point
 * ============================================================================ */

CEL_Build(Minimal) {
    (void)props;

    Widgets_init();
    TUI_Engine_use((TUI_EngineConfig){
        .title   = "CELS Minimal",
        .version = "0.1.0",
        .fps     = 30,
        .root    = AppUI
    });

    tui_widgets_register();
}
