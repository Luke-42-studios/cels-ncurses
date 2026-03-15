---
phase: 05-frame-pipeline
verified: 2026-03-14T23:30:00Z
status: passed
score: 5/5 must-haves verified
must_haves:
  truths:
    - "NCurses registers begin-frame and end-frame systems in CELS pipeline phases"
    - "Begin-frame system blocks SIGWINCH at PreRender before developer draws"
    - "End-frame system composites panels and calls doupdate at PostRender"
    - "All visible entity layers are auto-cleared at PreRender before developer draws"
    - "Developer-defined systems at OnRender run between PreRender and PostRender without explicit ordering"
  artifacts:
    - path: "src/ncurses_module.c"
      provides: "TUI_FrameBeginSystem, TUI_FrameEndSystem, layer clearing in TUI_LayerSystem"
    - path: "src/input/tui_input.c"
      provides: "Input system without legacy layer/frame references"
    - path: "include/cels_ncurses_draw.h"
      provides: "Drawing API without legacy layer/frame sections"
    - path: "CMakeLists.txt"
      provides: "Build config without tui_layer.c, tui_frame.c, draw_test target"
    - path: "src/tui_internal.h"
      provides: "Internal header without ncurses_register_frame_systems declaration"
  key_links:
    - from: "src/ncurses_module.c TUI_FrameBeginSystem"
      to: "sigprocmask SIG_BLOCK"
      via: "cel_run body"
    - from: "src/ncurses_module.c TUI_FrameEndSystem"
      to: "update_panels + doupdate + tui_hook_frame_end"
      via: "cel_run body"
    - from: "src/ncurses_module.c TUI_LayerSystem"
      to: "ncurses_layer_clear_window"
      via: "cel_each body for visible layers"
    - from: "src/ncurses_module.c CEL_Module(NCurses)"
      to: "TUI_FrameBeginSystem + TUI_FrameEndSystem"
      via: "cels_register"
---

# Phase 5: Frame Pipeline Verification Report

**Phase Goal:** NCurses owns the frame lifecycle through CELS pipeline phases, with developer systems running naturally between begin and end frame
**Verified:** 2026-03-14T23:30:00Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | NCurses registers begin-frame and end-frame systems in CELS pipeline phases | VERIFIED | `cels_register(TUI_LayerSystem, TUI_FrameBeginSystem, TUI_FrameEndSystem)` at line 181 of ncurses_module.c |
| 2 | Begin-frame system blocks SIGWINCH at PreRender before developer draws | VERIFIED | `CEL_System(TUI_FrameBeginSystem, .phase = PreRender)` at line 145, calls `sigprocmask(SIG_BLOCK, ...)` at line 150 |
| 3 | End-frame system composites panels and calls doupdate at PostRender | VERIFIED | `CEL_System(TUI_FrameEndSystem, .phase = PostRender)` at line 158, calls `update_panels()` + `doupdate()` + `sigprocmask(SIG_UNBLOCK, ...)` + `tui_hook_frame_end()` at lines 160-168 |
| 4 | All visible entity layers are auto-cleared at PreRender before developer draws | VERIFIED | TUI_LayerSystem at PreRender (line 96) iterates layers via `cel_each` and calls `ncurses_layer_clear_window()` for visible layers at lines 125-129 |
| 5 | Developer-defined systems at OnRender run between PreRender and PostRender without explicit ordering | VERIFIED | minimal.c defines `CEL_System(Renderer, .phase = OnRender)` at line 56 -- CELS pipeline ordering is PreRender < OnRender < PostRender. No explicit ordering code in example. |

**Score:** 5/5 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/ncurses_module.c` | TUI_FrameBeginSystem + TUI_FrameEndSystem + layer clearing | VERIFIED (210 lines) | Systems defined inline at lines 145-170, layer clearing at 124-129, both registered at 181-182 |
| `src/input/tui_input.c` | Input system without legacy layer/frame references | VERIFIED (188 lines) | No tui_layer_resize_all or tui_frame_invalidate_all calls. KEY_RESIZE handler is consume-and-continue (lines 91-95). |
| `include/cels_ncurses_draw.h` | No legacy layer/frame API sections | VERIFIED (539 lines) | No TUI_Layer typedef, no g_layers/g_layer_count/g_frame_state externs, no tui_layer_*/tui_frame_* declarations. Only documentation comments referencing legacy names remain (lines 273, 281). |
| `CMakeLists.txt` | No tui_layer.c, tui_frame.c, draw_test references | VERIFIED (105 lines) | No references to deleted files. Only minimal target exists. |
| `src/tui_internal.h` | No ncurses_register_frame_systems declaration | VERIFIED (63 lines) | Declaration removed. File declares only window lifecycle accessors, ECS systems, input config, and layer panel operations. |
| `src/frame/tui_frame.c` | DELETED | VERIFIED | File and directory do not exist |
| `src/layer/tui_layer.c` | DELETED | VERIFIED | File does not exist |
| `examples/draw_test.c` | DELETED | VERIFIED | File does not exist |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| ncurses_module.c TUI_FrameBeginSystem | sigprocmask SIG_BLOCK | cel_run body | WIRED | Line 150: `sigprocmask(SIG_BLOCK, &winch_set, NULL)` |
| ncurses_module.c TUI_FrameEndSystem | update_panels + doupdate | cel_run body | WIRED | Lines 160-161: `update_panels(); doupdate();` followed by SIG_UNBLOCK and tui_hook_frame_end() |
| ncurses_module.c TUI_LayerSystem | ncurses_layer_clear_window | cel_each for visible layers | WIRED | Lines 125-129: conditional on `TUI_LayerConfig->visible`, clears win and subcell_buf |
| ncurses_module.c CEL_Module(NCurses) | TUI_FrameBeginSystem + TUI_FrameEndSystem | cels_register | WIRED | Line 181-182: `cels_register(TUI_LayerSystem, TUI_FrameBeginSystem, TUI_FrameEndSystem)` |
| examples/minimal.c Renderer system | OnRender phase (between PreRender and PostRender) | CELS pipeline phase ordering | WIRED | Line 56: `CEL_System(Renderer, .phase = OnRender)` -- no explicit ordering needed |

### Requirements Coverage

| Requirement | Status | Blocking Issue |
|-------------|--------|----------------|
| FRAM-01: NCurses registers begin-frame and end-frame systems in CELS pipeline phases | SATISFIED | -- |
| FRAM-02: Begin-frame system clears layers; end-frame system composites panels and calls doupdate() | SATISFIED | -- |
| FRAM-03: Developer systems run between begin-frame and end-frame phases (natural CELS phase ordering) | SATISFIED | -- |

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | -- | -- | -- | No anti-patterns detected |

No TODO, FIXME, placeholder, or stub patterns found in any modified files.

### Build Verification

The `minimal` target builds and links cleanly with zero errors and zero warnings at `/tmp/cels-ncurses-build/`. All source files compile without unresolved symbols.

### Human Verification Required

### 1. Frame Pipeline Visual Behavior
**Test:** Run `./minimal` in a terminal. Observe that the background layer (z=0) fills with dots and the overlay layer (z=10) shows a bordered box.
**Expected:** Both layers render cleanly every frame. Resizing the terminal should cause layers to update correctly without visual corruption. No flickering during resize (SIGWINCH is blocked during rendering).
**Why human:** Visual rendering correctness and resize behavior cannot be verified programmatically.

### 2. Layer Auto-Clearing
**Test:** Run `./minimal` and observe that each frame the layers show only current-frame content (no ghost artifacts from previous frames).
**Expected:** The background shows only its dots pattern and text; the overlay shows only its box and text. No residual content from prior frames.
**Why human:** Frame-to-frame clearing behavior requires visual observation.

### Gaps Summary

No gaps found. All three success criteria (FRAM-01, FRAM-02, FRAM-03) are verified:

1. **TUI_FrameBeginSystem** (PreRender) and **TUI_FrameEndSystem** (PostRender) are defined as inline CELS pipeline phase systems in ncurses_module.c and registered via cels_register in the module init.

2. TUI_LayerSystem auto-clears all visible entity layers at PreRender. TUI_FrameEndSystem composites panels via `update_panels()`, flushes to terminal via `doupdate()`, unblocks SIGWINCH, and throttles FPS via `tui_hook_frame_end()`.

3. The minimal example demonstrates a developer-defined Renderer system at OnRender that draws into layers -- it runs naturally between PreRender and PostRender with zero explicit ordering code.

All legacy frame/layer code has been deleted (tui_frame.c, tui_layer.c, draw_test.c). Headers and CMake are clean of legacy references. The build compiles and links successfully.

---

_Verified: 2026-03-14T23:30:00Z_
_Verifier: Claude (gsd-verifier)_
