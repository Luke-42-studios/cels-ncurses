---
phase: 04-frame-pipeline
verified: 2026-02-08T19:00:00Z
status: passed
score: 4/4 must-haves verified
---

# Phase 4: Frame Pipeline Verification Report

**Phase Goal:** Developer uses a begin/end frame lifecycle that orchestrates layer compositing with a single doupdate() per frame
**Verified:** 2026-02-08T19:00:00Z
**Status:** PASSED
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | tui_frame_begin() clears all dirty layers and resets drawing state | VERIFIED | `src/frame/tui_frame.c:99-106` iterates `g_layers[0..g_layer_count)`, for dirty+visible layers calls `werase()` + `wattr_set(A_NORMAL, 0, NULL)` + sets `dirty=false`. Frame timing updated via `clock_gettime(CLOCK_MONOTONIC)`. Assert guards against nested calls. |
| 2 | tui_frame_end() composites via update_panels() + doupdate() | VERIFIED | `src/frame/tui_frame.c:121-128` calls `update_panels()` then `doupdate()` in sequence. Assert guards against call without matching begin(). Sets `in_frame=false` before compositing. |
| 3 | No drawing primitive calls wrefresh, wnoutrefresh, or doupdate | VERIFIED | Grep of `src/graphics/` for actual function calls to `wrefresh(`, `wnoutrefresh(`, `doupdate(` returns zero matches. Only `tui_frame.c:126-127` has `update_panels()` and `doupdate()` as actual code calls. No `wrefresh` or `wnoutrefresh` function calls exist anywhere in `src/`. |
| 4 | Frame pipeline integrates with TUI_Window frame loop, replacing wnoutrefresh(stdscr) | VERIFIED | `src/window/tui_window.c` frame loop is now `while(g_running) { Engine_Progress(delta); usleep(...); }` with zero calls to `update_panels()` or `doupdate()`. Renderer (`src/renderer/tui_renderer.c:225`) uses `overwrite(stdscr, bg->win)` bridge instead of `wnoutrefresh(stdscr)`. Frame pipeline ECS systems bracket the renderer: FrameBegin at EcsPreStore, FrameEnd at EcsPostFrame. |

**Score:** 4/4 truths verified

### Required Artifacts

| Artifact | Expected | Exists | Substantive | Wired | Status |
|----------|----------|--------|-------------|-------|--------|
| `include/cels-ncurses/tui_frame.h` | TUI_FrameState struct, 6 API declarations | YES (105 lines) | YES: TUI_FrameState with frame_count/delta_time/fps/in_frame, extern g_frame_state, 6 function declarations with full doc comments | YES: included by tui_engine.h, tui_engine.c, tui_renderer.c, tui_input.c, tui_frame.c | VERIFIED |
| `src/frame/tui_frame.c` | Frame pipeline implementation | YES (211 lines) | YES: 6 functions implemented (init, begin, end, invalidate_all, get_background, register_systems), ECS callbacks, timespec helper, global state definitions | YES: compiled via CMakeLists.txt INTERFACE sources, functions called from tui_engine.c and tui_input.c | VERIFIED |
| `include/cels-ncurses/tui_layer.h` | TUI_Layer with dirty flag | YES (164 lines) | YES: `bool dirty` field on line 58 with comment documenting auto-set/clear behavior | YES: used by tui_frame.c to read/write dirty, by tui_layer.c to initialize | VERIFIED |
| `src/layer/tui_layer.c` | dirty=true in get_draw_context, dirty=false in create | YES (194 lines) | YES: line 61 `layer->dirty = false` in create, line 192 `layer->dirty = true` as first line of get_draw_context | YES: functions called throughout system | VERIFIED |
| `CMakeLists.txt` | tui_frame.c in INTERFACE sources | YES (63 lines) | YES: line 27 `${CMAKE_CURRENT_SOURCE_DIR}/src/frame/tui_frame.c` | YES: build succeeds, app target compiles | VERIFIED |
| `src/window/tui_window.c` | Simplified frame loop, no update_panels/doupdate, COLS/LINES dimensions | YES (166 lines) | YES: frame loop is Engine_Progress+usleep only (lines 130-133), dimensions use COLS/LINES (lines 122-123), no hardcoded 600/800 | YES: registered as frame_loop provider | VERIFIED |
| `src/renderer/tui_renderer.c` | Background layer via overwrite bridge, no wnoutrefresh(stdscr) | YES (240 lines) | YES: gets background layer (line 146), gets draw context to mark dirty (line 148), uses `overwrite(stdscr, bg->win)` bridge (line 225), no wnoutrefresh(stdscr) | YES: registered as render provider at OnStore phase | VERIFIED |
| `src/tui_engine.c` | tui_frame_init + tui_frame_register_systems calls | YES (65 lines) | YES: `tui_frame_init()` on line 38, `tui_frame_register_systems()` on line 39, after `tui_renderer_init()` | YES: called during TUI_Engine module initialization | VERIFIED |
| `src/input/tui_input.c` | tui_frame_invalidate_all in KEY_RESIZE handler | YES (165 lines) | YES: line 97 `tui_frame_invalidate_all()` after `tui_layer_resize_all(COLS, LINES)` on line 96 | YES: runs as ECS system at OnLoad phase | VERIFIED |
| `include/cels-ncurses/tui_engine.h` | Includes tui_frame.h | YES (82 lines) | YES: line 46 `#include <cels-ncurses/tui_frame.h>` | YES: consumer entry point for engine module | VERIFIED |

### Key Link Verification

| From | To | Via | Status | Evidence |
|------|----|-----|--------|----------|
| `src/frame/tui_frame.c` | `g_layers` global | `extern` from tui_layer.h | WIRED | Lines 100-106: `g_layers[i].dirty`, `g_layers[i].visible`, `g_layers[i].win` accessed directly |
| `src/layer/tui_layer.c` | `tui_layer_get_draw_context` | sets dirty flag before returning | WIRED | Line 192: `layer->dirty = true;` is first statement in function body |
| `src/frame/tui_frame.c` | `update_panels` + `doupdate` | tui_frame_end composites | WIRED | Lines 126-127: sequential `update_panels()` then `doupdate()` |
| `src/tui_engine.c` | `tui_frame_init` | called during module init | WIRED | Line 38: `tui_frame_init();` after `tui_renderer_init()` |
| `src/tui_engine.c` | `tui_frame_register_systems` | registers ECS systems | WIRED | Line 39: `tui_frame_register_systems();` |
| `src/renderer/tui_renderer.c` | `tui_frame_get_background` | gets background layer for drawing | WIRED | Line 146: `TUI_Layer* bg = tui_frame_get_background();` |
| `src/renderer/tui_renderer.c` | `overwrite(stdscr, bg->win)` | copies stdscr to panel window | WIRED | Line 225: replaces former `wnoutrefresh(stdscr)` |
| `src/input/tui_input.c` | `tui_frame_invalidate_all` | forces full redraw on resize | WIRED | Line 97: after `tui_layer_resize_all(COLS, LINES)` |

### Requirements Coverage

| Requirement | Status | Evidence |
|-------------|--------|----------|
| FRAM-01: tui_frame_begin() clears layers and resets state | SATISFIED | tui_frame.c:85-108 -- werase + wattr_set on dirty+visible layers, timing update |
| FRAM-02: tui_frame_end() calls update_panels() to composite | SATISFIED | tui_frame.c:126 -- update_panels() |
| FRAM-03: doupdate() exactly once per frame after frame_end | SATISFIED | Only doupdate() call in entire src/ is tui_frame.c:127, inside tui_frame_end() |
| FRAM-04: Drawing primitives never call wrefresh/wnoutrefresh/doupdate | SATISFIED | Zero functional calls to these in src/graphics/*.c |
| FRAM-05: Integrates with TUI_Window frame loop | SATISFIED | Frame loop simplified to Engine_Progress+usleep; ECS systems bracket renderer; overwrite bridge replaces wnoutrefresh(stdscr) |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| None | - | - | - | No anti-patterns detected |

No TODO, FIXME, HACK, placeholder, or stub patterns found in any Phase 4 files.

### Human Verification Required

### 1. Visual Rendering Correctness
**Test:** Run `cd /home/cachy/workspaces/libs/cels && ./build/app` and verify the UI renders without visual corruption, ghost characters, or flickering
**Expected:** Menu renders cleanly, W/S navigate items with highlight, A/D changes settings, Q quits without corruption
**Why human:** Visual rendering quality cannot be verified programmatically -- requires human observation of terminal output

### 2. Terminal Resize Behavior
**Test:** While the app is running, resize the terminal window
**Expected:** All content redraws correctly at new dimensions without artifacts, ghost content from old dimensions, or crashes
**Why human:** Resize behavior involves terminal emulator interaction and visual correctness

### 3. No Flicker on Redraw
**Test:** Navigate menus rapidly and observe for any flicker or flash-to-black
**Expected:** Smooth transitions with no flicker (werase used instead of wclear to prevent this)
**Why human:** Flicker is a visual timing artifact only observable by human

### Gaps Summary

No gaps found. All four observable truths are verified with full three-level artifact checks (existence, substantive, wired). All five FRAM requirements are satisfied. The frame pipeline is fully implemented and integrated:

- `tui_frame_begin()` clears dirty layers and resets style using werase (not wclear)
- `tui_frame_end()` composites via `update_panels()` + `doupdate()` -- the only doupdate() in the entire source tree
- Drawing primitives (`src/graphics/*.c`) contain zero calls to wrefresh, wnoutrefresh, or doupdate
- Frame loop in `tui_window.c` is simplified to `Engine_Progress` + `usleep` with no direct panel/refresh calls
- Renderer uses an `overwrite(stdscr, bg->win)` bridge pattern for Phase 4 (full DrawContext migration deferred to Phase 5)
- KEY_RESIZE handler calls `tui_frame_invalidate_all()` to force full redraw after resize
- Window dimensions use `COLS`/`LINES` instead of hardcoded values
- Build compiles cleanly with zero errors and zero warnings

---
*Verified: 2026-02-08T19:00:00Z*
*Verifier: Claude (gsd-verifier)*
