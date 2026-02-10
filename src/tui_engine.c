/*
 * Engine Module - Implementation
 *
 * Registers TUI providers (Window, Input) and initializes the frame pipeline.
 * Uses CEL_Module for idempotent initialization.
 *
 * Usage:
 *   Engine_use(config) -- configure, init, and call root composition
 *
 * The root composition receives Engine_Context with state IDs so it can
 * use CEL_WatchId instead of storing raw pointers.
 *
 * NOTE: This file compiles in the CONSUMER's context (INTERFACE library).
 */

#include "cels-ncurses/tui_engine.h"
#include "cels-ncurses/tui_frame.h"
#include <cels-clay/clay_engine.h>
#include <cels-clay/clay_ncurses_renderer.h>

/* Module-level config (stored by Engine_use, read by init body) */
static Engine_Config g_engine_config = {0};
static bool g_config_set = false;

CEL_Module(Engine) {
    CEL_ModuleProvides(Window);
    CEL_ModuleProvides(Input);
    CEL_ModuleProvides(FramePipeline);

    /* Set active backend for provider filtering */
    cels_set_active_backend("TUI");

    /* Register window provider with config (or defaults) */
    CEL_Use(TUI_Window,
        .title = g_engine_config.title ? g_engine_config.title : "CELS App",
        .version = g_engine_config.version ? g_engine_config.version : "0.0.0",
        .fps = g_engine_config.fps > 0 ? g_engine_config.fps : 60
    );

    /* Register input provider (zero-config) */
    CEL_Use(TUI_Input);

    /* Initialize frame pipeline: background layer + ECS systems */
    tui_frame_init();
    tui_frame_register_systems();

    /* If root function provided, call it with engine context */
    if (g_engine_config.root) {
        /* Get window state ID from the registered pointer */
        cels_entity_t win_state_id = cels_state_register_ptr(
            "Engine_WindowState", &Engine_WindowState, sizeof(Engine_WindowState_t));

        /* Store real ID so tui_window_frame_loop can notify_change correctly */
        Engine_WindowStateID = win_state_id;

        Engine_Context ctx;
        ctx.windowState = win_state_id;

        g_engine_config.root(ctx);
    }
}

void Engine_use(Engine_Config config) {
    /* Store config for module init body */
    g_engine_config = config;
    g_config_set = true;

    /* Trigger module initialization (idempotent) */
    Engine_init();
}

/* ============================================================================
 * CelsNcurses Module -- absorbs Engine + Clay + Renderer init
 * ============================================================================
 *
 * CEL_BuildHas(CelsNcurses) calls CelsNcurses_init() which internally:
 *   1. Reads CEL_RunConfig for title/version/fps/root
 *   2. Calls Engine_use() (which initializes Window + Input + frame pipeline)
 *   3. Calls Clay_Engine_use() (Clay layout engine)
 *   4. Calls clay_ncurses_renderer_init() (ncurses Clay renderer)
 *   5. Registers root composition via cfg->root() if specified
 */
CEL_Module(CelsNcurses) {
    CEL_ModuleProvides(Window);
    CEL_ModuleProvides(Input);
    CEL_ModuleProvides(Renderer);
    CEL_ModuleProvides(FrameLoop);

    /* Read run config (stored by CEL_Run macro before CELS_BuildInit) */
    const CEL_RunConfig* cfg = _cels_get_run_config();

    /* Initialize engine -- pass .root = NULL because CEL_RunConfig.root is
       void(*)(void) (1-arg CEL_Root), not void(*)(Engine_Context) (2-arg).
       Engine_use expects the 2-arg form. We handle the 1-arg root separately. */
    Engine_use((Engine_Config){
        .title = cfg->title ? cfg->title : "CELS App",
        .version = cfg->version ? cfg->version : "0.0.0",
        .fps = cfg->fps > 0 ? cfg->fps : 60,
        .root = NULL
    });

    /* Initialize Clay layout engine */
    Clay_Engine_use(NULL);

    /* Initialize ncurses renderer */
    clay_ncurses_renderer_init(NULL);

    /* Register root composition if specified in CEL_RunConfig.
       cfg->root points to AppUI (function pointer to AppUI_init),
       which calls cels_root_composition_register(). */
    if (cfg->root) {
        cfg->root();
    }
}
