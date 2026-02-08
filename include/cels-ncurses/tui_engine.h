/*
 * TUI Engine Module - Single Entry Point
 *
 * Bundles all TUI providers (Window, Input, Renderer) into one module.
 * Instead of manually including each provider header and calling init:
 *
 *   Before:
 *     #include <cels-ncurses/tui_window.h>
 *     #include <cels-ncurses/tui_input.h>
 *     #include <cels-ncurses/tui_renderer.h>
 *     CEL_Use(TUI_Window, .title = "App", .fps = 60);
 *     CEL_Use(TUI_Input);
 *     tui_renderer_init();
 *
 *   After:
 *     #include <cels-ncurses/tui_engine.h>
 *     TUI_Engine_use((TUI_EngineConfig){
 *         .title = "CELS Demo", .version = "0.9.4.1", .fps = 60,
 *         .root = AppUI
 *     });
 *
 * Usage (with root composition):
 *   CEL_Build(App) {
 *       TUI_Engine_use((TUI_EngineConfig){
 *           .title = "CELS Demo",
 *           .version = "0.9.4.1",
 *           .fps = 60,
 *           .root = AppUI
 *       });
 *       InitConfig();
 *       InitSystems();
 *   }
 *
 *   CEL_Root(AppUI, TUI_EngineContext) {
 *       TUI_WindowState_t* win = CEL_WatchId(ctx.windowState, TUI_WindowState_t);
 *       ...
 *   }
 */

#ifndef CELS_NCURSES_TUI_ENGINE_H
#define CELS_NCURSES_TUI_ENGINE_H

#include <cels-ncurses/tui_window.h>
#include <cels-ncurses/tui_input.h>
#include <cels-ncurses/tui_renderer.h>

/* ===========================================
 * TUI Engine Context -- passed to root composition
 * ===========================================
 *
 * Contains state IDs from the engine. Root composition
 * uses CEL_WatchId to get typed state pointers.
 */
typedef struct TUI_EngineContext {
    cels_entity_t windowState;   /* ID to observe window state via CEL_WatchId */
} TUI_EngineContext;

/* ===========================================
 * TUI Engine Module Config
 * ===========================================
 *
 * Configuration struct for TUI_Engine_use().
 * Fields are forwarded to TUI_Window provider.
 * .root receives TUI_EngineContext with state IDs.
 */
typedef struct TUI_EngineConfig {
    const char* title;      /* Window title (default: "CELS App") */
    const char* version;    /* App version for info display (default: "0.0.0") */
    int fps;                /* Target frames per second (default: 60) */
    void (*root)(TUI_EngineContext ctx);  /* Root composition init function */
} TUI_EngineConfig;

/* Module entity ID and init function */
extern cels_entity_t TUI_Engine;
extern void TUI_Engine_init(void);

/* Module use with config -- initializes engine and calls root composition */
extern void TUI_Engine_use(TUI_EngineConfig config);

#endif /* CELS_NCURSES_TUI_ENGINE_H */
