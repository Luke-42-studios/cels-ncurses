/*
 * TUI Engine Module - Implementation
 *
 * Registers TUI providers (Window, Input) and initializes the frame pipeline.
 * Uses CEL_Module for idempotent initialization.
 *
 * Usage:
 *   TUI_Engine_use(config) -- configure, init, and call root composition
 *
 * The root composition receives TUI_EngineContext with state IDs so it can
 * use CEL_WatchId instead of storing raw pointers.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-ncurses/tui_engine.h"
#include "cels-ncurses/tui_frame.h"

/* Module-level config (stored by TUI_Engine_use, read by init body) */
static TUI_EngineConfig g_tui_config = {0};
static bool g_config_set = false;

CEL_Module(TUI_Engine) {
    CEL_ModuleProvides(Window);
    CEL_ModuleProvides(Input);
    CEL_ModuleProvides(FramePipeline);

    /* Set active backend for provider filtering */
    cels_set_active_backend("TUI");

    /* Register window provider with config (or defaults) */
    CEL_Use(TUI_Window,
        .title = g_tui_config.title ? g_tui_config.title : "CELS App",
        .version = g_tui_config.version ? g_tui_config.version : "0.0.0",
        .fps = g_tui_config.fps > 0 ? g_tui_config.fps : 60
    );

    /* Register input provider (zero-config) */
    CEL_Use(TUI_Input);

    /* Initialize frame pipeline: background layer + ECS systems */
    tui_frame_init();
    tui_frame_register_systems();

    /* If root function provided, call it with engine context */
    if (g_tui_config.root) {
        /* Get window state ID from the registered pointer */
        cels_entity_t win_state_id = cels_state_register_ptr(
            "TUI_WindowState", &TUI_WindowState, sizeof(TUI_WindowState_t));

        /* Store real ID so tui_window_frame_loop can notify_change correctly */
        TUI_WindowStateID = win_state_id;

        TUI_EngineContext ctx;
        ctx.windowState = win_state_id;

        g_tui_config.root(ctx);
    }
}

void TUI_Engine_use(TUI_EngineConfig config) {
    /* Store config for module init body */
    g_tui_config = config;
    g_config_set = true;

    /* Trigger module initialization (idempotent) */
    TUI_Engine_init();
}
