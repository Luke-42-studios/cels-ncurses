/*
 * Engine Module - Single Entry Point
 *
 * Bundles TUI providers (Window, Input) and frame pipeline into one module.
 * Instead of manually including each provider header and calling init:
 *
 *   Before:
 *     #include <cels-ncurses/tui_window.h>
 *     #include <cels-ncurses/tui_input.h>
 *     CEL_Use(TUI_Window, .title = "App", .fps = 60);
 *     CEL_Use(TUI_Input);
 *
 *   After:
 *     #include <cels-ncurses/tui_engine.h>
 *     Engine_use((Engine_Config){
 *         .title = "CELS Demo", .version = "0.9.4.1", .fps = 60,
 *         .root = AppUI
 *     });
 *
 * Usage (with root composition):
 *   CEL_Build(App) {
 *       Engine_use((Engine_Config){
 *           .title = "CELS Demo",
 *           .version = "0.9.4.1",
 *           .fps = 60,
 *           .root = AppUI
 *       });
 *       InitConfig();
 *       InitSystems();
 *   }
 *
 *   CEL_Root(AppUI, Engine_Context) {
 *       Engine_WindowState_t* win = CEL_WatchId(ctx.windowState, Engine_WindowState_t);
 *       ...
 *   }
 */

#ifndef CELS_NCURSES_TUI_ENGINE_H
#define CELS_NCURSES_TUI_ENGINE_H

#include <cels-ncurses/tui_window.h>
#include <cels-ncurses/tui_input.h>
#include <cels-ncurses/tui_frame.h>

/* ===========================================
 * Engine_Context -- passed to root composition
 * ===========================================
 *
 * Contains state IDs from the engine. Root composition
 * uses CEL_WatchId to get typed state pointers.
 */
typedef struct Engine_Context {
    cels_entity_t windowState;   /* ID to observe window state via CEL_WatchId */
} Engine_Context;

/* ===========================================
 * Engine_Config -- module configuration
 * ===========================================
 *
 * Configuration struct for Engine_use().
 * Fields are forwarded to TUI_Window provider.
 * .root receives Engine_Context with state IDs.
 */
typedef struct Engine_Config {
    const char* title;      /* Window title (default: "CELS App") */
    const char* version;    /* App version for info display (default: "0.0.0") */
    int fps;                /* Target frames per second (default: 60) */
    void (*root)(Engine_Context ctx);  /* Root composition init function */
} Engine_Config;

/* Module entity ID and init function */
extern cels_entity_t Engine;
extern void Engine_init(void);

/* Module use with config -- initializes engine and calls root composition */
extern void Engine_use(Engine_Config config);

/* ============================================================================
 * Backward Compatibility (v0.2 -> v0.3)
 * ============================================================================ */
typedef Engine_Context TUI_EngineContext;
typedef Engine_Config TUI_EngineConfig;
#define TUI_Engine_use Engine_use
#define TUI_Engine_init Engine_init
#define TUI_Engine Engine

#endif /* CELS_NCURSES_TUI_ENGINE_H */
