---
phase: 06-mouse-input
verified: 2026-02-21T08:00:00Z
status: passed
score: 9/9 must-haves verified
---

# Phase 6: Mouse Input Verification Report

**Phase Goal:** Developer can poll mouse position and receive button press/release events through the module-local input system
**Verified:** 2026-02-21
**Status:** PASSED
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Module-local input struct replaces removed CELS_Input -- no dependency on cels/backend.h for input types | VERIFIED | `TUI_InputState` defined in `tui_input.h` (lines 75-119). Zero occurrences of `CELS_Input` or `cels_input_set` in `src/input/tui_input.c`. Zero occurrences of `cels/backend.h` include in `tui_input.c`. |
| 2 | Mouse position is pollable via tui_input_get_mouse_position() at any time | VERIFIED | Function declared in header (line 131), implemented in source (lines 96-99). Returns `g_input_state.mouse_x/y` with NULL-safe pointer checks. |
| 3 | Mouse button press and release events are reported for left, middle, and right buttons each frame | VERIFIED | KEY_MOUSE handler (lines 193-243) decodes BUTTON1/2/3 PRESSED and RELEASED bstate flags. Sets `mouse_button`, `mouse_pressed`, and `mouse_released` fields. |
| 4 | Mouse button held state is tracked persistently (set on press, cleared on release) | VERIFIED | `mouse_left_held`, `mouse_middle_held`, `mouse_right_held` fields in struct (lines 116-118). Set to true on PRESSED, set to false on RELEASED (lines 209-234). NOT reset in per-frame reset block (line 133 comment confirms). |
| 5 | Multiple mouse events per frame are drained -- final position and last button event win | VERIFIED | Drain loop uses `do { ... } while ((ch = wgetch(stdscr)) == KEY_MOUSE)` (line 238). Each iteration overwrites position and button state. Non-mouse key is put back via `ungetch(ch)` (line 241). |
| 6 | Keyboard input continues to work identically to v1.0 behavior | VERIFIED | All keyboard mappings preserved: arrow keys (143-146), WASD (149-152), Enter/Escape (155-159), Tab/navigation (162-177), Q quit (180-190), function keys (283-294), number keys (289-291), raw key passthrough (296-297). Field names unchanged from v1.0. |
| 7 | Space Invaders example compiles and runs with the new TUI_InputState API | VERIFIED | `game.c` uses `TUI_InputState` type (line 119) and `tui_input_get_state()` (lines 181, 197, 219). Zero occurrences of `CELS_Input` or `cels_input_get` in game.c. |
| 8 | Game input behavior is identical to v1.0 (arrow keys move ship, space fires) | VERIFIED | All input field names unchanged (`axis_left`, `button_accept`, `raw_key`, `has_raw_key`). Only type name and getter function changed. `sizeof(TUI_InputState)` used in memcpy (lines 189, 209, 267). |
| 9 | Full project builds from cels repo root without errors | VERIFIED | `cmake --build build` returns 0 errors. All targets built successfully including cels, cels-debug, and test binaries. |

**Score:** 9/9 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/cels-ncurses/tui_input.h` | TUI_InputState struct, TUI_MouseButton enum, extern g_input_state, getter declarations, backward compat shims | VERIFIED | 171 lines. TUI_InputState (lines 75-119) with all keyboard + mouse fields. TUI_MouseButton enum (lines 57-62). extern g_input_state (line 121). Getters declared (lines 128, 131). Backward compat CELS_Input typedef (line 163) and cels_input_get() inline shim (lines 166-169). |
| `src/input/tui_input.c` | Mouse-enabled input system with g_input_state, mousemask init, KEY_MOUSE drain loop, getter implementations | VERIFIED | 377 lines. g_input_state definition (line 59). mousemask() call with BUTTON1-3 PRESSED/RELEASED + REPORT_MOUSE_POSITION (lines 346-350). mouseinterval(0) (line 351). KEY_MOUSE case with drain loop (lines 193-243). Getters implemented (lines 92-99). |
| `examples/space_invaders/game.c` | Updated game using TUI_InputState instead of CELS_Input | VERIFIED | 839 lines. All 3 input systems (TitleInput, GameOverInput, PlayerInput) use tui_input_get_state(). g_prev_input declared as TUI_InputState (line 119). sizeof(TUI_InputState) in all memcpy calls. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `src/input/tui_input.c` | `include/cels-ncurses/tui_input.h` | defines extern g_input_state declared in header | WIRED | `TUI_InputState g_input_state = {0};` at line 59 satisfies `extern TUI_InputState g_input_state;` at header line 121 |
| `src/input/tui_input.c` | ncurses mouse API | mousemask + getmouse in init and event loop | WIRED | `mousemask(desired, NULL)` at line 350. `getmouse(&event)` at line 198. `MEVENT event` struct used. Full button decode logic present. |
| `examples/space_invaders/game.c` | `include/cels-ncurses/tui_input.h` | calls tui_input_get_state() to read input | WIRED | 3 call sites at lines 181, 197, 219. Return value assigned to `const TUI_InputState* input` and fields accessed throughout. |

### Requirements Coverage

| Requirement | Status | Blocking Issue |
|-------------|--------|----------------|
| MOUS-01 (partial -- no modifier keys) | SATISFIED | Button press/release with cell coordinates implemented. Modifier keys explicitly deferred per ROADMAP scope reduction. |
| MOUS-02 (scroll wheel) | DEFERRED | Explicitly deferred in ROADMAP. Not in Phase 6 scope. |
| MOUS-03 (hit testing) | DEFERRED | Explicitly deferred in ROADMAP. Not in Phase 6 scope. |
| MOUS-04 (drag detection) | DEFERRED | Explicitly deferred in ROADMAP. Not in Phase 6 scope. |
| MOUS-05 (hover events) | DEFERRED | Explicitly deferred in ROADMAP. Not in Phase 6 scope. |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `tui_input.h` | 140 | `int _placeholder` in TUI_Input struct | Info | Intentional -- C requires at least one struct field. Zero-config provider pattern documented in comment. Not an anti-pattern. |

No blockers, no warnings. Zero TODO/FIXME/PLACEHOLDER patterns in implementation files. Zero stub patterns (empty returns, console-only handlers).

### Human Verification Required

### 1. Mouse Position Tracking

**Test:** Run the application in a terminal that supports mouse events (xterm, kitty, alacritty). Move the mouse around the terminal window. Read `g_input_state.mouse_x` and `g_input_state.mouse_y` values (add a debug print or use a debugger).
**Expected:** `mouse_x` and `mouse_y` update to reflect the cell coordinates of the cursor. Values should be (-1, -1) before any mouse movement.
**Why human:** Cannot verify ncurses mouse event delivery without a real terminal session.

### 2. Mouse Button Press/Release

**Test:** Click left, middle, and right mouse buttons in the terminal. Observe `mouse_button`, `mouse_pressed`, `mouse_released`, and `mouse_*_held` fields.
**Expected:** On left click press: `mouse_button = TUI_MOUSE_LEFT`, `mouse_pressed = true`, `mouse_left_held = true`. On left click release: `mouse_button = TUI_MOUSE_LEFT`, `mouse_released = true`, `mouse_left_held = false`. Same pattern for middle and right.
**Why human:** Requires real terminal mouse event generation and ncurses mouse support.

### 3. Space Invaders Gameplay

**Test:** Run the Space Invaders example. Play through title screen, gameplay (move ship, fire bullets), and game over/wave clear transitions.
**Expected:** All keyboard controls work identically to v1.0. Arrow keys move ship, Enter/Space fires, Q quits.
**Why human:** Functional gameplay testing requires visual and interactive verification.

### Gaps Summary

No gaps found. All 9 observable truths verified against the actual codebase. All 3 artifacts pass existence, substantive, and wiring checks. All key links are connected. The phase goal -- "Developer can poll mouse position and receive button press/release events through the module-local input system" -- is achieved at the code level.

The only remaining verification is human testing of mouse events in a real terminal (listed above), which requires interactive terminal access that cannot be automated structurally.

---

_Verified: 2026-02-21_
_Verifier: Claude (gsd-verifier)_
