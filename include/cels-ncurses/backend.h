/*
 * CELS ncurses Backend - Single Include for ncurses Backend Apps
 *
 * This is the umbrella header for applications using the ncurses TUI backend.
 * It re-exports all backend-related headers so apps only need one include:
 *
 *   #include <cels-ncurses/backend.h>
 *
 * Instead of individually including:
 *   #include <cels-ncurses/tui_engine.h>
 *   #include <cels-clay/clay_engine.h>
 *   #include <cels-clay/clay_ncurses_renderer.h>
 *
 * Provides two build composition modules:
 *
 *   CelsNcurses  -- Absorbs Engine + Clay + Renderer init into one call.
 *                   CEL_BuildHas(CelsNcurses) replaces the manual sequence of
 *                   Engine_use(...) + Clay_Engine_use(NULL) + renderer_init(NULL).
 *
 *   CelsEngine   -- Explicit no-op marker documenting that the app uses the
 *                   CELS core engine. Engine_Startup() runs before CELS_BuildInit(),
 *                   so CelsEngine_init() just registers the module entity.
 *
 * Usage:
 *   CEL_BuildComposition(App) {
 *       CEL_BuildHas(CelsEngine);    // core engine (marker)
 *       CEL_BuildHas(CelsNcurses);   // ncurses backend (Engine + Clay + Renderer)
 *       CEL_BuildHas(Widgets);       // widget library
 *   }
 *   CEL_Run(.title = "My App", .fps = 60, .root = AppUI)
 */

#ifndef CELS_NCURSES_BACKEND_H
#define CELS_NCURSES_BACKEND_H

/* Re-export all backend headers an app needs */
#include <cels-ncurses/tui_engine.h>         /* Engine module (re-exports tui_window.h, tui_input.h, tui_frame.h) */
#include <cels-clay/clay_engine.h>           /* Clay layout engine */
#include <cels-clay/clay_ncurses_renderer.h> /* ncurses Clay renderer */

/* ============================================================================
 * CelsNcurses Module
 * ============================================================================
 *
 * Absorbs Engine + Clay layout engine + ncurses renderer initialization
 * into a single CEL_BuildHas(CelsNcurses) call. Reads CEL_RunConfig for
 * title/version/fps/root and passes them to Engine_use internally.
 *
 * Also creates the CEL_Window singleton entity and updates it each frame.
 */
extern cels_entity_t CelsNcurses;
extern void CelsNcurses_init(void);

/* ============================================================================
 * CelsEngine Module
 * ============================================================================
 *
 * Explicit marker module for build compositions. CelsEngine is always
 * explicit -- never auto-loaded. Engine_Startup() has already run before
 * CELS_BuildInit() is called, so CelsEngine_init() only registers the
 * module entity for self-documentation and introspection.
 */
extern cels_entity_t CelsEngine;
extern void CelsEngine_init(void);

#endif /* CELS_NCURSES_BACKEND_H */
