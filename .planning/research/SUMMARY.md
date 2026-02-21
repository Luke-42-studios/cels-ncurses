# Project Research Summary: v1.1 Enhanced Rendering

**Project:** cels-ncurses v1.1
**Domain:** Terminal graphics API -- advanced rendering capabilities
**Researched:** 2026-02-20
**Supersedes:** v1.0 research summary (2026-02-07)
**Confidence:** HIGH

## Executive Summary

The v1.1 milestone extends a proven v1.0 foundation (all 39 requirements verified, panel-backed layers, frame pipeline, color/attribute system) with five deferred features: mouse input, sub-cell rendering, true color, damage tracking, and layer transparency. No new external dependencies are required -- every needed API is already present in the installed ncurses 6.6.20251230. The features span the entire difficulty spectrum: mouse input and sub-cell rendering are purely additive (new code, no existing changes), true color requires a surgical modification to the color resolution path, damage tracking requires instrumenting all draw primitives, and layer transparency fundamentally conflicts with how ncurses panels composite.

The recommended approach is to build in dependency order: mouse input first (completely independent, highest user-facing value), then true color (modifies only tui_color.c, unlocks half-block coloring), then sub-cell rendering (new draw primitives that benefit from true color), then damage tracking (instruments all draw functions including the new sub-cell ones), and finally layer transparency (most invasive, benefits from damage tracking infrastructure). This ordering is driven by two constraints: sub-cell half-block rendering is dramatically more powerful with true color (each cell uses fg/bg to encode two pixel colors), and damage tracking should be built after all draw functions exist to avoid retrofitting instrumentation.

The dominant risk is layer transparency. ncurses panels have zero transparency concept -- `update_panels()` paints each window opaquely in z-order. Achieving per-cell transparency requires custom compositing that partially bypasses the panel library, and this custom compositing invalidates the retained-mode dirty tracking from v1.0 (changes to a lower layer must trigger re-compositing of overlapping transparent layers). The recommended mitigation is a two-tier system: opaque layers continue using panel compositing (fast, proven), while transparent layers are removed from the panel stack and composited manually using `overlay()` or cell-by-cell reading via `mvwin_wch()`. This limits the blast radius to layers explicitly marked transparent.

## Key Findings

### From STACK.md

- **No new dependencies.** All five features use APIs already in ncurses 6.6 (`mousemask`, `getmouse`, `setcchar`, `init_color`, `overlay`, `touchline`). No CMakeLists.txt changes needed except adding new source files.
- **Mouse input** uses `mousemask()` + `getmouse()` + `wenclose()` + `wmouse_trafo()`. SGR 1006 extended protocol handled internally by ncurses. Set `mouseinterval(0)` for responsive press/release.
- **Sub-cell rendering** uses half-block characters (U+2580/U+2584) via existing `setcchar()` + `mvwadd_wch()` path. Braille (U+2800-U+28FF) available as opt-in high-resolution mode.
- **True color** via `init_color()` palette redefinition + `can_change_color()` detection. System ncurses was compiled WITHOUT `NCURSES_RGB_COLORS`, so direct color (packed RGB indices) is not available -- must use palette redefinition approach instead.
- **Damage tracking** via custom `TUI_DirtyRegion` with fixed-size rect list and overflow-to-full-dirty fallback. `touchline()` available as optional ncurses-level hint.
- **Layer transparency** via `overlay()` for binary compositing (spaces are transparent). No per-cell alpha -- terminals have no alpha channel.

### From FEATURES.md

**Table stakes (must ship in v1.1):**
- M-1: Click detection (left/right/middle) with layer-aware hit testing
- M-2: Scroll wheel input (BUTTON4/5 as discrete events)
- M-3: Layer-aware hit testing (`wenclose()` + z-order iteration)
- S-1: Half-block characters (2x vertical resolution, the high-value technique)
- C-1: Direct/true color mode (preserve exact RGB from Clay)
- C-2: Graceful fallback to 256-color palette
- D-1: Per-region dirty tracking (replace layer-level boolean with dirty rects)
- T-1: Binary transparency (transparent/opaque cells)

**Differentiators (should have, build if time allows):**
- M-4: Drag detection (press + motion + release state machine)
- M-5: Hover/motion tracking (with input queue drain)
- S-2: Quadrant block characters (2x2 resolution)
- S-3: Braille patterns (2x4 resolution, monochrome)
- C-3: Environment variable override (`COLORTERM=truecolor`)
- D-3: Frame skipping (zero draw calls = skip doupdate entirely)
- D-4: Synchronized output protocol (`\033[?2026h/l`)

**Defer to v1.2+:**
- T-3: Alpha blending (requires true color, compositing buffer, per-cell color extraction -- very complex)
- S-4: Sextant characters (font support uncertain)
- M-4/M-5: Drag and hover can be deferred if scope needs trimming

**Anti-features (explicitly excluded):**
- No raw terminal escape sequences (breaks ncurses state)
- No per-cell alpha in TUI_Style (transparency is a layer property)
- No GPU blend modes, no sixel/kitty graphics protocol
- No gesture recognition, no custom cursor rendering
- No application-level shadow buffer duplicating ncurses internals
- No multi-threaded rendering (ncurses is not thread-safe)

### From ARCHITECTURE.md

**Component impact classification:**
| Feature | Impact | Files Changed |
|---------|--------|---------------|
| Mouse Input | ADDITIVE | tui_input.c (+KEY_MOUSE case), new tui_mouse.h |
| Sub-Cell Rendering | ADDITIVE | New tui_subcell.c/h, CMakeLists.txt (+1 source) |
| True Color | MODIFICATION | tui_color.h (TUI_Color struct), tui_color.c (color path) |
| Damage Tracking | INSTRUMENTATION | tui_draw.c (record rects), tui_frame.c (selective clear), new tui_damage.h |
| Layer Transparency | INVASIVE | tui_layer.h (transparent flag), tui_frame.c (compositing pass) |

**Key architectural decisions:**
- Mouse state stored as `TUI_MouseState` global with extern linkage (same pattern as `g_frame_state`), NOT modifying cross-backend `CELS_Input`.
- True color uses `init_color()` palette redefinition (Approach A), not raw escape sequences or recompiled ncurses. Color slot allocator manages indices 16-255 with deduplication.
- TUI_Color struct grows from 4 bytes to 8 bytes (adds r, g, b, is_rgb). TUI_Style grows from 12 to 20 bytes. Still stack-allocated, still passed by value.
- Damage tracking uses double-buffer pattern: `prev_damage` (what to clear) and `curr_damage` (what's being drawn). Overflow at 64 rects triggers `full_dirty = true`.
- Transparent layers removed from panel stack (`hide_panel`), composited manually after `update_panels()` but before `doupdate()`.

### From PITFALLS.md

**20 pitfalls identified. Top 7 requiring active prevention:**

1. **Mouse coords are screen-relative, not window-relative** (Critical) -- Always use `wmouse_trafo()` to convert to layer-local. Never expose raw MEVENT coords. Test with non-zero-origin layers.

2. **alloc_pair incompatible with direct color on this system** (Critical) -- `NCURSES_RGB_COLORS=0` means color indices are palette indices, not packed RGB. Must use `init_color()` to redefine palette slots. Keep 256-color fallback.

3. **Panels have no transparency** (Critical) -- `update_panels()` composites opaquely. Custom compositing required for transparent layers. Only `overlay()` treats spaces as transparent.

4. **Transparency breaks retained-mode dirty tracking** (Critical) -- Changes to layer A must trigger re-compositing of overlapping transparent layer B. Damage tracking must include inter-layer overlap dependencies.

5. **Mouse events share wgetch() queue** (Critical) -- Motion events can flood the queue, starving keyboard input. Must drain entire input queue per frame, keeping only last mouse position. Already have drain pattern for KEY_RESIZE.

6. **mousemask() must be called AFTER initscr()** (High) -- Calling during `TUI_Input_use()` (which runs at CEL_Build time, before initscr) silently fails. Call in startup hook instead.

7. **Sub-cell rendering doubles color pair pressure** (Moderate) -- Each half-block cell needs a unique fg/bg pair for its two pixel colors. With gradients, the 32767 pair limit can be hit. Monitor `alloc_pair()` return values.

## Recommended Stack

| Capability | API | Fallback | Notes |
|------------|-----|----------|-------|
| Mouse events | `mousemask()` + `getmouse()` | None needed | Standard ncurses, NCURSES_MOUSE_VERSION=2 |
| Mouse hit test | `wenclose()` + `wmouse_trafo()` | Manual coord math | Built into ncurses |
| Half-block rendering | `setcchar()` + `mvwadd_wch()` with U+2580/U+2584 | Full-cell only | Same wide-char path as v1.0 borders |
| Braille rendering | Same + U+2800-U+28FF | Half-block | Opt-in, requires font support |
| True color | `init_color()` + `can_change_color()` | `rgb_to_nearest_256()` (v1.0 path) | ~240 simultaneous true colors |
| Color detection | `can_change_color()`, `COLORS` global | Assume 256 palette | Check at startup |
| Dirty rect tracking | Custom `TUI_DamageTracker` | `werase()` full layer (v1.0) | Application-level, trusts ncurses for I/O diff |
| Binary transparency | `overlay(src, dst)` or `mvwin_wch()` read | Opaque panels only | Spaces treated as transparent by `overlay()` |
| Sync output | `\033[?2026h/l` escape | None (terminal optional) | Differentiator, not table stakes |

## Architecture Approach

### Component Boundaries

```
Input Layer (tui_input.c, tui_mouse.h)
  - Reads wgetch() stream, drains queue per frame
  - Maps KEY_MOUSE -> getmouse() -> TUI_MouseState
  - Hit tests layers via wenclose() + wmouse_trafo()

Drawing Layer (tui_draw.c, tui_subcell.c)
  - All draw functions take TUI_DrawContext*
  - Each draw call records damage rect into layer's tracker
  - Sub-cell draws use same DrawContext + Style infrastructure

Color System (tui_color.c)
  - Dual-mode: palette (v1.0) or true-color (init_color redefinition)
  - Detection at startup via can_change_color()
  - Color slot allocator for indices 16-255 with dedup

Frame Pipeline (tui_frame.c, tui_damage.h)
  - frame_begin: clear prev_damage rects (not werase entire layer)
  - frame_end: update_panels() for opaque layers
  - frame_end: composite transparent layers manually
  - frame_end: doupdate()
  - Swap prev_damage = curr_damage

Layer System (tui_layer.c)
  - Existing create/destroy/show/hide/z-order unchanged
  - NEW: transparent flag, damage tracker per layer
  - Transparent layers hidden from panel stack, composited in frame_end
```

### Data Flow (v1.1)

```
wgetch loop (drain all)
  |
  +-> keyboard events -> CELS_Input (unchanged)
  +-> KEY_MOUSE -> getmouse() -> TUI_MouseState
  |
frame_begin:
  clear prev_damage rects per layer (selective, not werase)
  |
draw phase:
  tui_draw_* -> WINDOW writes + damage accumulation
  tui_draw_halfblock/braille -> sub-cell rendering
  |
frame_end:
  update_panels()            -- opaque layers composited by ncurses
  composite_transparent()    -- manual overlay for transparent layers
  doupdate()                 -- flush to terminal
  swap damage buffers
```

### Build Order

The five features form a dependency chain with two independent entry points:

```
Mouse Input (independent)
    |
    v
[can start immediately, no deps]

True Color (independent)
    |
    v
Sub-Cell Rendering (benefits from true color for half-block coloring)
    |
    v
Damage Tracking (instruments all draw functions, including sub-cell)
    |
    v
Layer Transparency (requires damage tracking for efficient compositing)
```

## Critical Pitfalls (Prevention Strategies)

| # | Pitfall | Prevention | Phase |
|---|---------|------------|-------|
| 1 | Mouse coords screen-relative | `wmouse_trafo()` for all coord conversion; test with offset layers | Mouse |
| 5 | Mouse floods keyboard queue | Drain entire wgetch queue per frame; keep only last mouse position | Mouse |
| 11 | mousemask() before initscr silently fails | Call in startup hook, not CEL_Build; check return value | Mouse |
| 2 | alloc_pair wrong for true color on this system | Use `init_color()` palette redefinition; color slot allocator with dedup | True Color |
| 10 | TERM not set for true color | Detect `can_change_color()` at startup; log COLORS value; document TERM requirements | True Color |
| 3 | Panels have no transparency | Custom compositing for transparent layers; remove from panel stack | Transparency |
| 4 | Transparency breaks dirty tracking | Inter-layer overlap dependency tracking; accept higher redraw cost for transparent layers | Damage + Transparency |

## Implications for Roadmap

### Recommended Phase Structure: 5 Phases

**Phase 1: Mouse Input**
- Scope: M-1 (click), M-2 (scroll), M-3 (hit testing)
- Rationale: Completely independent of rendering pipeline. Smallest surface area (extends tui_input.c, adds ~30 lines + new TUI_MouseState struct). Highest immediate user-facing value -- unlocks interactive UI for downstream cels-clay consumer.
- Must avoid: Pitfall #1 (screen coords), #5 (queue flood), #11 (mousemask timing), #17 (F1 pause disables mouse)
- Delivers: Click, scroll wheel, layer-aware hit testing
- Research needed: LOW -- standard ncurses mouse API, well-documented

**Phase 2: True Color**
- Scope: C-1 (true color mode), C-2 (fallback), C-3 (env detection)
- Rationale: Must come before sub-cell rendering. Half-block rendering uses fg+bg to encode two pixel colors -- true color makes this dramatically more powerful (16M colors vs 256). Modifies only tui_color.h/c. API unchanged externally (`tui_color_rgb()` signature stays the same).
- Must avoid: Pitfall #2 (alloc_pair vs init_color), #8 (short pair overflow), #10 (TERM detection)
- Delivers: 24-bit RGB color fidelity with graceful 256-color fallback
- Research needed: MEDIUM -- `init_color()` slot management needs careful implementation, `can_change_color()` behavior under tmux/screen needs testing

**Phase 3: Sub-Cell Rendering**
- Scope: S-1 (half-block), S-2 (quadrant), S-3 (braille -- if time)
- Rationale: New draw functions, zero changes to existing code. Purely additive. Benefits from Phase 2 true color for half-block pixel coloring. Uses existing `setcchar()` + `mvwadd_wch()` infrastructure.
- Must avoid: Pitfall #7 (fg/bg duality -- standardize on U+2584, fg=bottom, bg=top), #16 (pair pressure), #12 (braille font support)
- Delivers: 2x vertical resolution (half-block), 2x2 resolution (quadrant), 2x4 high-res plotting (braille)
- Research needed: LOW -- standard Unicode block elements, community-proven technique

**Phase 4: Damage Tracking**
- Scope: D-1 (dirty rects), D-2 (layer dirty rects), D-3 (frame skipping)
- Rationale: Instruments all draw primitives (including Phase 3 sub-cell functions). Requires all draw functions to exist first to avoid retrofitting. Modifies frame pipeline (tui_frame.c) to use selective clearing instead of werase(). Foundation for efficient transparency compositing in Phase 5.
- Must avoid: Pitfall #9 (granularity -- start with dirty rects, not per-cell bitmaps), #14 (layer movement invalidates old+new regions)
- Delivers: Reduced CPU from skipping unchanged region redraws; frame skipping for idle apps
- Research needed: LOW -- application-level rect tracking is straightforward; trust ncurses for terminal I/O optimization

**Phase 5: Layer Transparency**
- Scope: T-1 (binary transparency), T-2 (transparent background)
- Rationale: Most invasive feature. Modifies the frame pipeline's compositing path. Benefits from Phase 4 damage tracking (skip re-compositing unchanged transparent layers). Must be last because it changes the critical `tui_frame_end()` path that all other features depend on.
- Must avoid: Pitfall #3 (panels are opaque), #4 (transparency breaks dirty tracking), #13 (dual compositing systems conflicting)
- Delivers: Popup layers with transparent backgrounds; overlay content showing through to layers below
- Research needed: HIGH -- custom compositing vs `overlay()` tradeoffs need prototyping. Interaction with damage tracking needs careful design. This phase should run `/gsd:research-phase` before detailed planning.

### Phase Ordering Rationale

The ordering is driven by three constraints:

1. **True color before sub-cell:** Half-block rendering's power comes from encoding two distinct pixel colors via fg/bg per cell. With 256-color mode, you get 256 possible pixel colors. With true color, you get 16M. Building true color first means sub-cell rendering immediately benefits.

2. **All draw functions before damage tracking:** Damage tracking adds a `tui_damage_record()` call inside every draw function. If sub-cell draw functions do not exist yet, damage tracking must be retrofitted when they are added. Building sub-cell first means damage tracking instruments everything in one pass.

3. **Damage tracking before transparency:** Transparent layer compositing is expensive (reads every cell). Damage tracking lets the compositor skip unchanged regions of transparent layers, making transparency viable for real-time rendering.

### Research Flags

| Phase | Needs `/gsd:research-phase`? | Rationale |
|-------|------------------------------|-----------|
| Phase 1 (Mouse) | No | Standard ncurses API, well-documented, no unknowns |
| Phase 2 (True Color) | Maybe | `init_color()` slot management is novel for this codebase; tmux behavior needs testing |
| Phase 3 (Sub-Cell) | No | Community-proven technique (btop, chafa, drawille), uses existing wide-char path |
| Phase 4 (Damage) | No | Application-level rect tracking, standard pattern |
| Phase 5 (Transparency) | Yes | Custom compositing conflicts with panel library; interaction with damage tracking is the hardest design problem in v1.1 |

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All APIs verified against ncurses 6.6 on target system. Compile tests confirm linkage. No new dependencies. |
| Features | HIGH | Feature set derived from v1.0 deferred list + downstream cels-clay requirements. Anti-features clearly scoped from v1.0 learnings. |
| Architecture | HIGH (mouse, sub-cell, true color), MEDIUM (damage, transparency) | Additive features well-understood. Damage tracking approach is sound but rect merging strategy may need tuning. Transparency compositing has multiple viable approaches -- needs prototyping. |
| Pitfalls | HIGH | 20 pitfalls sourced from ncurses man pages, system header inspection, and v1.0 codebase analysis. All prevention strategies are concrete. |

**Overall confidence:** HIGH for the milestone scope. MEDIUM for Phase 5 (transparency) implementation details.

## Open Questions / Gaps to Address

1. **True color via init_color() -- slot pressure under load.** With only ~240 reusable color slots (indices 16-255), complex UIs with many distinct RGB values may exceed capacity. The deduplication cache helps, but worst-case behavior under gradient rendering with sub-cell needs profiling. Could `reset_color_pairs()` between frames help, or does it cause flicker?

2. **Transparent layer compositing approach.** `overlay()` treats ALL spaces as transparent (including intentional opaque spaces). The marker-character approach (private-use Unicode codepoint as sentinel) gives full control but requires cell-by-cell reading. Which approach works better in practice needs a prototype in Phase 5 planning.

3. **Mouse input queue drain performance.** When `REPORT_MOUSE_POSITION` is enabled, dragging across a 200-column terminal generates ~200 events per frame. The drain loop is O(events) per frame. Need to verify this does not cause frame drops at 60fps. If it does, motion reporting should be opt-in (enabled only during active drag).

4. **F1 pause mode mouse interaction.** Disabling mouse via `mousemask(0, &saved)` before entering pause mode should allow terminal-native text selection. Need to verify the save/restore pattern works correctly and that no queued mouse events leak into keyboard processing on resume.

5. **TUI_Color size change backward compatibility.** Growing TUI_Color from 4 to 8 bytes changes TUI_Style from 12 to 20 bytes. All callers pass by value, so this is source-compatible. But any downstream code that stores TUI_Style in fixed-size buffers or makes sizeof assumptions will break. Need to audit cels-clay integration code.

6. **tmux true color passthrough.** tmux has its own color handling. Even if the inner terminal supports `can_change_color()`, tmux may not pass `init_color()` palette changes through. Need to test and document tmux configuration requirements (e.g., `set -g default-terminal "tmux-256color"` with `terminal-overrides`).

## Sources

### Primary (HIGH confidence)
- ncurses 6.6 system headers (`/usr/include/curses.h`, NCURSES_EXT_COLORS=20251230, NCURSES_MOUSE_VERSION=2)
- ncurses man pages: curs_mouse(3X), curs_color(3X), new_pair(3X), panel(3X), curs_touch(3X), overlay(3X)
- Local `infocmp -x xterm-direct` verification (colors#0x1000000, RGB flag present)
- v1.0 codebase: all source files in `src/` and `include/` analyzed
- v1.0 architecture decisions in `.planning/STATE.md`

### Secondary (MEDIUM confidence)
- [ncurses official documentation](https://invisible-island.net/ncurses/man/) -- API reference
- [kakoune #1807](https://github.com/mawww/kakoune/issues/1807) -- true color + ncurses gotchas
- [ncurses transparency techniques](https://hoop.dev/blog/ncurses-processing-transparency-without-hacks/) -- overlay patterns
- [ncurses direct color discussion](https://lists.gnu.org/archive/html/bug-ncurses/2018-04/msg00011.html) -- init_extended_pair behavior
- [Drawille (braille terminal graphics)](https://github.com/asciimoo/drawille) -- braille canvas pattern
- [Block Elements Unicode](https://en.wikipedia.org/wiki/Block_Elements) -- codepoint reference
- [Braille Patterns Unicode](https://en.wikipedia.org/wiki/Braille_Patterns) -- dot-to-bit mapping
- [True color terminal support list](https://gist.github.com/sindresorhus/bed863fb8bedf023b833c88c322e44f9)

### Tertiary (LOW confidence)
- [Alacritty block elements #5485](https://github.com/alacritty/alacritty/issues/5485) -- terminal-specific rendering
- [ncurses transparency blog](http://nion.modprobe.de/blog/archives/205-Transparency-with-ncurses.html) -- older techniques

---
*Research synthesis completed: 2026-02-20*
*Ready for roadmap: yes*
