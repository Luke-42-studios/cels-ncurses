---
phase: 03-input-system
verified: 2026-03-02T02:39:16Z
status: passed
score: 3/3 must-haves verified
---

# Phase 3: Input System Verification Report

**Phase Goal:** Developer can read keyboard and mouse input state that NCurses fills each frame in a dedicated CELS phase
**Verified:** 2026-03-02T02:39:16Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | An NCurses input system runs in a CELS input phase, reading getch/mouse events without developer intervention | VERIFIED | `CEL_System(NCurses_InputSystem, .phase = OnLoad)` at tui_input.c:295 calls `tui_read_input_ncurses()` which drains `wgetch(stdscr)` and mouse events. Registered via `ncurses_register_input_system()` called from `CEL_Module(NCurses)` at ncurses_module.c:105. Strong symbol in tui_input.c:319 overrides weak stub at ncurses_module.c:163. |
| 2 | Developer can read input state (key presses, mouse position, button events) from a component after the input phase completes | VERIFIED | `NCurses_InputState` CEL_Component defined at tui_ncurses.h:108 with keyboard fields (axis_x/y, accept, cancel, tab, etc.), mouse fields (mouse_x/y, mouse_button, mouse_pressed/released), and held state. Extern ID at tui_ncurses.h:166-169. examples/minimal.c:42 reads via `cel_watch(win, NCurses_InputState)` and renders axis, mouse position, accept/cancel, raw key at lines 44-62. |
| 3 | Input state is fresh each frame -- previous frame state does not leak into the current frame | VERIFIED | tui_input.c:104 builds `NCurses_InputState state = {0}` (all per-frame fields zeroed). Lines 107-113 copy only persistent fields (mouse_x, mouse_y, mouse_left/middle/right_held) from previous component state. Even on ERR (no input), the zeroed-but-persistent state is written to entity at lines 118-121. After switch/case, state is written at lines 282-284. |

**Score:** 3/3 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `include/cels-ncurses/tui_ncurses.h` | NCurses_InputState CEL_Component definition + extern ID | VERIFIED | 199 lines. CEL_Component(NCurses_InputState) at line 108 with 23 fields (keyboard + mouse). Extern ID `_ncurses_InputState_id` declared at line 166, macro alias at line 169. |
| `src/ncurses_module.c` | InputState_id global definition + cel_has in composition | VERIFIED | 167 lines. `_ncurses_InputState_id = 0` at line 55. `cel_has(NCurses_InputState, ...)` at line 144 in CEL_Compose(NCursesWindow) with zeroed defaults and mouse at -1,-1. |
| `src/input/tui_input.c` | Input system writing NCurses_InputState to window entity | VERIFIED | 348 lines. Entity-component pattern: gets entity via ncurses_window_get_entity() (line 96), reads previous state (line 100-101), builds fresh state (line 104), copies persistent fields (lines 107-113), drains getch (line 115+), writes via cels_entity_set_component + cels_component_notify_change (lines 118-121 for ERR, lines 282-284 for normal). Full switch/case with keyboard, mouse, resize, pause, custom keys preserved. |
| `include/cels-ncurses/tui_input.h` | Internal-only header with quit guard API | VERIFIED | 17 lines. Stripped to just `tui_input_set_quit_guard()` declaration. No TUI_InputState, no g_input_state, no CELS_Input, no getters. |
| `examples/minimal.c` | Reference example reading NCurses_InputState via cel_watch | VERIFIED | 70 lines. `cel_watch(win, NCurses_InputState)` at line 42. Displays axis, mouse position, accept/cancel, raw key events at lines 44-62. Dual cel_watch pattern (WindowState + InputState). |
| `src/window/tui_window.c` | Window module without running pointer bridge | VERIFIED | 249 lines. No `tui_window_get_running_ptr()` function. g_running stays internal (SIGINT handler at line 71, frame update at line 188). |
| `include/cels-ncurses/tui_window.h` | Window header without tui_window_get_running_ptr | VERIFIED | 53 lines. No declaration of tui_window_get_running_ptr. Only ncurses_window_frame_update() and tui_hook_frame_end() exported. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| tui_input.c | window entity | ncurses_window_get_entity() + cels_entity_set_component | WIRED | Line 96: gets entity. Lines 118-119, 282-283: writes NCurses_InputState_id to entity. Lines 120, 284: notifies change. |
| ncurses_module.c | NCurses_InputState component | cel_has in CEL_Compose(NCursesWindow) | WIRED | Line 144: `cel_has(NCurses_InputState, .axis_x = 0, .axis_y = 0, .mouse_x = -1, .mouse_y = -1)` |
| tui_ncurses.h | ncurses_module.c | extern _ncurses_InputState_id | WIRED | Header declares extern at line 166. Module defines global at line 55. Macro alias at header line 169. |
| CEL_Module(NCurses) | Input system | ncurses_register_input_system() | WIRED | Module init calls at ncurses_module.c:105. Strong definition at tui_input.c:319 overrides weak stub at ncurses_module.c:163. Registers CEL_System at OnLoad via NCurses_InputSystem_register() at tui_input.c:339. |
| examples/minimal.c | NCurses_InputState | cel_watch(win, NCurses_InputState) | WIRED | Line 42 reads input. Lines 49-61 display axis, mouse, accept, cancel, raw_key in mvprintw calls. |
| tui_input.c | quit signaling | cels_request_quit() | WIRED | Line 170: only quit path on 'q'/'Q'. No g_running pointer bridge. |

### Requirements Coverage

| Requirement | Status | Blocking Issue |
|-------------|--------|----------------|
| INP-01: NCurses input system runs in a CELS input phase, reading getch/mouse events and writing to input state | SATISFIED | None. CEL_System at OnLoad phase reads getch, writes NCurses_InputState to window entity each frame. |
| INP-02: Developer can read input state (key presses, mouse position, button events) after the input phase completes | SATISFIED | None. cel_watch(entity, NCurses_InputState) demonstrated in minimal.c. Component has all keyboard and mouse fields. |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None | - | - | - | No anti-patterns detected |

Zero TODO/FIXME/placeholder patterns found in tui_input.c or tui_ncurses.h. Zero stale references to removed APIs (g_input_state, tui_input_get_state, tui_window_get_running_ptr, CELS_Input, TUI_MOUSE_*) in src/, include/, or examples/. References in .planning/ documentation are historical and expected.

### Build Verification

| Target | Status | Details |
|--------|--------|---------|
| `minimal` | PASS | Builds without errors at /tmp/cels-ncurses-build |
| `draw_test` | PASS | Builds without errors at /tmp/cels-ncurses-build |

### Human Verification Required

### 1. Input system reads keyboard events at runtime

**Test:** Build and run the minimal example. Press arrow keys, WASD, Enter, Escape, number keys, function keys.
**Expected:** Line 2 shows axis values changing (-1/0/1). "ENTER pressed!" and "ESC pressed!" appear on key press. Raw key display updates for character keys.
**Why human:** Requires running terminal application and observing ncurses output.

### 2. Mouse input is captured at runtime

**Test:** Move mouse over the terminal window. Click left/middle/right buttons.
**Expected:** Mouse coordinates on line 2 update to track cursor position. Button events reflected in component state.
**Why human:** Requires mouse interaction with terminal and observing real-time coordinate updates.

### 3. Per-frame state reset is observable

**Test:** Press Enter, then observe next frame. Press arrow key, then release.
**Expected:** "ENTER pressed!" appears for one frame only, then disappears. Axis values return to 0 when no directional key is held.
**Why human:** Requires observing temporal behavior (single-frame events vs persistent state) which cannot be verified structurally.

### 4. Quit via 'q' works correctly

**Test:** Press 'q' in the running minimal example.
**Expected:** Application exits cleanly (terminal restored to normal mode).
**Why human:** Requires running the application and verifying clean shutdown behavior.

### Gaps Summary

No gaps found. All three phase success criteria are verified at the structural level:

1. **Input system as CELS phase system:** CEL_System(NCurses_InputSystem) runs at OnLoad, registered through the module init chain. Reads getch/mouse events each frame without developer intervention.

2. **Developer reads component:** NCurses_InputState is a proper CEL_Component with keyboard (axis, accept, cancel, tab, navigation, numbers, function keys, raw passthrough) and mouse (position, button, press/release, held state) fields. Readable via cel_watch() demonstrated in minimal.c.

3. **Fresh each frame:** Stack-allocated `{0}` struct with explicit persistent field copy from previous component state. Written to entity on every code path (ERR, normal input, mouse events) except quit/resize/pause.

---

_Verified: 2026-03-02T02:39:16Z_
_Verifier: Claude (gsd-verifier)_
