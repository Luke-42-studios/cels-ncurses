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
 * TUI Window - Internal Header
 *
 * Internal-facing header for the window subsystem. Public types
 * (NCurses_WindowConfig, NCurses_WindowState) are in tui_ncurses.h.
 * This header provides:
 *   - WindowState enum (internal lifecycle states)
 *   - tui_window_get_running_ptr() bridge for input system
 *   - ncurses_window_frame_update() called by frame pipeline
 *   - tui_hook_frame_end() FPS throttle called by frame pipeline
 */

#ifndef CELS_NCURSES_TUI_WINDOW_H
#define CELS_NCURSES_TUI_WINDOW_H

/* ===========================================
 * Window State Machine (internal lifecycle)
 * =========================================== */
typedef enum WindowState {
    WINDOW_STATE_NONE = 0,
    WINDOW_STATE_CREATED,
    WINDOW_STATE_SURFACE_READY,
    WINDOW_STATE_READY,
    WINDOW_STATE_RESIZING,
    WINDOW_STATE_MINIMIZED,
    WINDOW_STATE_CLOSING,
    WINDOW_STATE_CLOSED
} WindowState;

/* Get pointer to g_running flag for quit signaling from input provider */
extern volatile int* tui_window_get_running_ptr(void);

/* Per-frame window state update (resize detection, timing, quit check).
 * Called by the frame pipeline's frame_begin system. */
extern void ncurses_window_frame_update(void);

/* FPS throttle -- sleeps to maintain target frame rate.
 * Called by the frame pipeline's frame_end system. */
extern void tui_hook_frame_end(void);

#endif /* CELS_NCURSES_TUI_WINDOW_H */
