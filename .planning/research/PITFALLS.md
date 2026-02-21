# Domain Pitfalls: v1.1 Features

**Domain:** Terminal graphics API / ncurses rendering engine -- v1.1 extensions
**Researched:** 2026-02-20
**Confidence:** HIGH (ncurses 6.6 man pages on system, codebase inspection, web research)
**Applies to:** Mouse input, sub-cell rendering, true color, damage tracking, layer transparency

---

## Critical Pitfalls

Mistakes that cause rewrites, data corruption, or architectural dead-ends.

---

### Pitfall 1: Mouse Coordinates Are Screen-Relative, Not Window-Relative

**Feature area:** Mouse input
**Severity:** Critical

**What goes wrong:** MEVENT.x and MEVENT.y are screen-relative (absolute terminal coordinates). v1.0 layers use local coordinates -- TUI_DrawContext origin is (0,0) within each layer's WINDOW. Passing raw MEVENT coordinates to layer hit-testing produces wrong results for any layer not at position (0,0). This silently returns wrong layers for click targets.

**Why it happens:** The ncurses documentation states: "Its y and x coordinates are screen-, not window-, relative." Developers naturally assume mouse coordinates match the coordinate system used everywhere else in their code.

**Consequences:** Click events dispatched to wrong layer. Off-by-N errors where N is the layer's screen offset. Works by coincidence during development if all layers are full-screen (origin 0,0), then breaks when non-fullscreen layers are added.

**Prevention:**
1. Use `wmouse_trafo(layer->win, &y, &x, FALSE)` to convert screen coords to window-local coords. The function returns FALSE if the point is outside the window, doubling as a hit-test.
2. Alternatively, manually subtract `layer->x` and `layer->y` from MEVENT coords, then bounds-check against `layer->width` and `layer->height`.
3. Provide a `tui_layer_contains_point(layer, screen_x, screen_y)` helper that uses `wenclose()`.
4. Never expose raw MEVENT coords in the public API -- always convert to layer-local before populating CELS_Input mouse fields.

**Detection:** Test with a popup layer at non-zero position. If clicks on the popup are offset from visual position, this bug is present.

**Sources:** [curs_mouse(3X)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- HIGH confidence

---

### Pitfall 2: alloc_pair Is Incompatible With Direct Color (True Color) Mode

**Feature area:** True color
**Severity:** Critical

**What goes wrong:** v1.0 uses `alloc_pair(fg_index, bg_index)` where `fg_index` and `bg_index` are xterm-256 color indices (0-255). True color requires passing 24-bit RGB values encoded as color indices. Under `xterm-direct` terminfo, `COLORS` is 0x1000000 (16 million) and `COLOR_PAIRS` is 0x10000 (65536). The alloc_pair pool fills up after 65535 unique pairs, then starts LRU-evicting.

More critically: in direct color mode, color indices ARE the RGB values (e.g., color index `(r << 16) | (g << 8) | b`). The v1.0 `rgb_to_nearest_256()` mapping in `tui_color.c` would need to be bypassed entirely -- it maps RGB to the nearest of 256 indices, destroying color fidelity.

**Why it happens:** v1.0 was designed for xterm-256. The `TUI_Color.index` field stores a value 0-255. Direct color requires storing values 0-16777215. The field width is `int` so it can hold the value, but the mapping function and the mental model are wrong.

**Consequences:**
- Colors rendered as nearest-256 approximation instead of exact RGB.
- alloc_pair pool exhausted quickly with many distinct colors, causing LRU churn and visual flickering as old pairs are evicted.
- `init_color()` and `init_extended_color()` break in direct color mode (the kakoune issue #1807 documents this).
- wattr_set pair parameter is `short`, limiting to 32767 pairs even with extended functions.

**Prevention:**
1. Detect direct color at startup: `tigetflag("RGB")` or check if `COLORS > 256`.
2. If direct color: skip `rgb_to_nearest_256()`, store `(r << 16) | (g << 8) | b` directly in `TUI_Color.index`.
3. Use `init_extended_pair()` instead of `alloc_pair()` for direct color mode -- alloc_pair's pool management doesn't scale to 16M colors.
4. Consider the `TERM` environment: user must have `xterm-direct`, `konsole-direct`, or similar terminfo. Provide runtime detection and fallback to 256-color mode.
5. Keep the 256-color path as fallback. Not all terminals support direct color. The system must work in both modes.

**Detection:** Compare rendered color visually against input RGB. If colors are quantized (banding in gradients), direct color is not active. Check `COLORS` at startup and log it.

**Sources:** [curs_color(3X)](https://invisible-island.net/ncurses/man/curs_color.3x.html), [ncurses 6.6 alloc_pair man page](https://invisible-island.net/ncurses/man/curs_color.3x.html), [kakoune #1807](https://github.com/mawww/kakoune/issues/1807), local `infocmp -x xterm-direct` verification -- HIGH confidence

---

### Pitfall 3: Panel Library Has No Transparency -- It Composites Opaquely

**Feature area:** Layer transparency
**Severity:** Critical

**What goes wrong:** ncurses panels composite windows opaquely. A panel at the top of the stack completely occludes all panels below it in its footprint. There is no alpha channel, no "transparent cell" concept in the panel library. `update_panels()` does not blend -- it overwrites.

The `overlay()` function copies non-blank characters from source to destination (treating blanks as transparent), but it operates between two specific windows, not across the entire panel stack. It is NOT automatically invoked by `update_panels()`.

**Why it happens:** ncurses is a character-cell library from the 1980s. Transparency was never a design goal. The panel library provides z-ordering and occlusion, not compositing.

**Consequences:**
- A semi-transparent popup layer is impossible with stock panel compositing.
- Marking cells as "transparent" in a layer has no effect -- the panel library writes whatever is in the WINDOW, including blank spaces (which overwrite what's below).
- Attempting to use `overwrite()` or `overlay()` between panel windows at the wrong time corrupts the frame.

**Prevention:**
1. Define transparency as a cels-ncurses concept, not an ncurses concept. Use a sentinel value (e.g., a specific character+attribute combination, or a parallel bitmask array) to mark cells as "transparent" within each TUI_Layer.
2. Bypass `update_panels()` for transparent layers. Instead, implement custom compositing:
   a. Iterate layers bottom-to-top in z-order.
   b. For each cell in each layer, if the cell is marked transparent, skip it.
   c. For opaque cells, write to a compositing buffer (could be stdscr or a dedicated output window).
   d. Call `wnoutrefresh()` on the compositing buffer, then `doupdate()`.
3. Alternative simpler approach: only support "cutout" transparency (entire cells are either visible or invisible, no blending). Use `overlay()` from lower layers into higher layers where transparency cells exist.
4. Transparent layers must be composited EVERY frame they overlap with, not just when dirty. This conflicts with v1.0's retained-mode dirty tracking.

**Detection:** Draw a colored background layer. Create a popup layer with blank spaces. If the background is invisible behind the popup, transparency is not working.

**Sources:** [Panel Library HOWTO](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/panels.html), [overlay(3X)](https://www.gnu.org/software/guile-ncurses/manual/html_node/Overlay-and-manipulate-overlapped-windows.html), [ncurses transparency blog](http://nion.modprobe.de/blog/archives/205-Transparency-with-ncurses.html) -- HIGH confidence

---

### Pitfall 4: Transparency Breaks Retained-Mode Dirty Tracking

**Feature area:** Layer transparency + damage tracking
**Severity:** Critical

**What goes wrong:** v1.0's frame pipeline is retained-mode: only dirty layers are erased and redrawn. Non-dirty layers keep content from the previous frame. This optimization breaks when transparency is introduced:

- If layer B is transparent and overlaps layer A, changes to layer A must trigger re-compositing of layer B's region -- even if layer B itself is not dirty.
- If layer B moves, both its old position (now revealing layer A) and new position (now occluding layer A) must be redrawn.
- `werase()` on a transparent layer fills it with blanks, which are opaque in panel compositing.

**Why it happens:** Dirty tracking in v1.0 is per-layer. Transparency creates inter-layer dependencies that per-layer tracking cannot capture.

**Consequences:**
- Stale content from lower layers visible through transparent regions after the lower layer redraws but the upper layer doesn't.
- Ghost artifacts when transparent layers move.
- Forced full-redraw every frame to maintain correctness, negating the performance benefit of dirty tracking.

**Prevention:**
1. Introduce "overlap dependency tracking": when layer A is dirty and layer B (transparent) overlaps A, mark B as dirty too.
2. For damage tracking: track dirty REGIONS (rectangles), not just dirty LAYERS. A change to region R on layer A dirties the same region R on all layers above A that intersect R and have transparency in that region.
3. Accept that transparent layers force more redraws. Optimize by limiting transparent layers to small popups (not full-screen overlays).
4. Consider a two-tier system: opaque layers use panel compositing (fast), transparent layers use software compositing (correct).

**Detection:** Create a background layer with animation. Create a static transparent overlay. If the animation shows stale frames through the transparent region, this bug is present.

**Sources:** Codebase analysis of `tui_frame.c` dirty tracking loop, [ncurses refresh documentation](https://www.gnu.org/software/guile-ncurses/manual/html_node/Refresh-windows-and-lines.html) -- HIGH confidence

---

### Pitfall 5: Mouse Events Share wgetch() With Keyboard -- Event Type Confusion

**Feature area:** Mouse input
**Severity:** Critical

**What goes wrong:** v1.0's input system reads one key per frame via `wgetch(stdscr)`. Mouse events arrive as `KEY_MOUSE` through the same channel. The current code has a flat `switch(ch)` that does not handle `KEY_MOUSE`. Adding a `case KEY_MOUSE:` that calls `getmouse()` seems simple, but:

1. Mouse events use a SEPARATE QUEUE from keyboard events. `wgetch()` returns `KEY_MOUSE` to signal availability, but the actual event data must be retrieved via `getmouse(&event)`.
2. The v1.0 code reads ONE key per frame. If the user clicks while pressing a key in the same frame, the mouse event might be deferred to the next frame, creating input lag.
3. Mouse movement events (`REPORT_MOUSE_POSITION`) can flood the input queue at high frequency (every terminal cell the mouse crosses generates an event), starving keyboard input.

**Why it happens:** ncurses's mouse interface was bolted onto the keyboard input system. The dual-queue architecture (keyboard queue + mouse queue) is non-obvious.

**Consequences:**
- Missed mouse clicks during keyboard input.
- Input lag on mouse events (one-frame delay minimum with single-key-per-frame design).
- If `REPORT_MOUSE_POSITION` is enabled, mouse movement flood causes keyboard inputs to be delayed or lost entirely.

**Prevention:**
1. Drain the entire input queue each frame, not just one key. Loop `wgetch()` until `ERR`. Process all keyboard and mouse events collected.
2. Store mouse state separately from keyboard state in CELS_Input. Mouse position/buttons should be persistent state (like a game controller), not single-frame events.
3. Be selective with `mousemask()`: start with `ALL_MOUSE_EVENTS` (clicks only), NOT `ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION`. Add position reporting only if hover detection is needed, and throttle it.
4. When `wgetch()` returns `KEY_MOUSE`, immediately call `getmouse()` and process the MEVENT. Do not defer.

**Detection:** Click rapidly while holding a key. If clicks are dropped or delayed, the input draining is insufficient.

**Sources:** [curs_mouse(3X)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html), v1.0 `tui_input.c` codebase analysis -- HIGH confidence

---

## High Pitfalls

Mistakes that cause significant bugs or performance problems, but are recoverable without rewrite.

---

### Pitfall 6: Scroll Wheel Events Are Not Paired Press/Release

**Feature area:** Mouse input
**Severity:** High

**What goes wrong:** In xterm-compatible terminals, scroll wheel events arrive as `BUTTON4_PRESSED` (scroll up) and `BUTTON5_PRESSED` (scroll down) WITHOUT matching release events. Code that waits for a press-release pair to recognize a "click" will never fire for scroll events.

**Why it happens:** The terminal protocol sends scroll wheel as button press only. ncurses 6 added BUTTON4/BUTTON5 specifically for this. Earlier ncurses (version 5) mapped scroll to BUTTON2 in some configurations, causing conflicts.

**Prevention:**
1. Handle `BUTTON4_PRESSED` and `BUTTON5_PRESSED` as complete discrete events (scroll up/down), not as press-that-needs-release.
2. Do NOT use `mouseinterval()` timing for scroll events -- they fire per detent, not per click.
3. Map to CELS_Input as `scroll_delta` (integer, +1/-1), not as button state.

**Detection:** Scroll the mouse wheel. If nothing happens but clicks work, the scroll event handling is missing or waiting for a release that never comes.

**Sources:** [curs_mouse(3X)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html), [ncurses mailing list scroll wheel discussion](https://lists.gnu.org/archive/html/bug-ncurses/2012-01/msg00011.html) -- HIGH confidence

---

### Pitfall 7: Sub-Cell Characters Are Single-Width But Need Paired Coloring

**Feature area:** Sub-cell rendering
**Severity:** High

**What goes wrong:** Half-block characters (U+2580 Upper Half Block, U+2584 Lower Half Block) allow 2 vertical "pixels" per cell. But ncurses applies ONE foreground and ONE background color per cell. The upper half uses the foreground color, the lower half uses the background color (for U+2584) or vice versa (for U+2580). This means:

1. You cannot independently color both halves with arbitrary colors -- one half is always the background.
2. If you need pixel A on top and pixel B on bottom, you must choose: U+2584 (lower half) with fg=B, bg=A, OR U+2580 (upper half) with fg=A, bg=B.
3. The "transparent" half inherits the cell's background color, which might not match the layer below.

**Why it happens:** Terminal cells have exactly one fg+bg pair. Sub-cell rendering overloads this by treating fg as one half and bg as the other. This is a well-known technique (used by btop, sixel-free image renderers, etc.) but has inherent limitations.

**Consequences:**
- Wrong colors when the developer doesn't understand the fg/bg duality.
- Cannot render 3 or more colors in adjacent half-cells without careful pair management.
- Sub-cell rendering through transparent layers is extremely complex -- the "background" half must be composited from the layer below.
- alloc_pair LRU churn if many unique fg/bg combinations are used (every sub-cell pixel pair may need a unique color pair).

**Prevention:**
1. Document the fg/bg convention clearly: for U+2584 (lower half block), fg = lower pixel, bg = upper pixel.
2. Always use U+2584 (lower half block) as the canonical character. This is the community standard (btop, chafa, etc.) because it allows the "blank" state (space character) to naturally show the background color for the upper half.
3. Expect 2x alloc_pair pressure from sub-cell rendering (every cell needs a unique pair for its two pixel colors).
4. Sub-cell drawing should have its own draw function: `tui_draw_subcell(ctx, x, y, top_color, bottom_color)`.

**Detection:** Render a 2-color vertical stripe pattern in sub-cell mode. If colors are swapped between top and bottom, the fg/bg convention is inverted.

**Sources:** [Unicode Block Elements](https://graphemica.com/%E2%96%84), [Alacritty block elements discussion](https://github.com/alacritty/alacritty/issues/5485), [Unicode graphics analysis](https://dernocua.github.io/notes/unicode-graphics.html) -- MEDIUM confidence (community practice, not official docs)

---

### Pitfall 8: wattr_set pair Parameter Is `short` -- Limits Extended Color Pairs

**Feature area:** True color
**Severity:** High

**What goes wrong:** v1.0 uses `wattr_set(win, attrs, (short)pair, NULL)`. The pair parameter is `short`, which limits values to 0-32767. In direct color mode, `COLOR_PAIRS` is 65536. Pairs above 32767 are silently truncated by the cast to `short`, displaying the wrong colors.

**Why it happens:** The X/Open Curses standard defined `wattr_set` with a `short` pair parameter. ncurses provides `wattr_set` that actually accepts the wider type internally (since ncurses 6, the pair is stored in a separate int in cchar_t), but the C prototype still uses `short` for compatibility.

**Consequences:** Colors appear wrong for pair numbers >= 32768. The bug is silent -- no error, no warning, just wrong colors.

**Prevention:**
1. For true color mode, use `wattr_set` with the extended pair workaround: pass pair=0 in the short parameter and use `attr_set`/`wattr_set` with the `opts` parameter for extended pair numbers (ncurses extension).
2. Alternatively, apply colors per-character via `setcchar()` + `mvwadd_wch()`, where the cchar_t stores the full int pair.
3. Monitor the pair number returned by `alloc_pair()` / `init_extended_pair()`. If it exceeds SHRT_MAX, switch to extended rendering path.
4. Consider `wcolor_set()` which also uses short, same limitation applies.

**Detection:** Allocate > 32767 color pairs and render them. If pair 32768 shows the same color as pair 0, truncation is occurring.

**Sources:** [curs_color(3X)](https://invisible-island.net/ncurses/man/curs_color.3x.html), system ncurses 6.6 man page, [init_extended_pair man page](https://manpages.debian.org/bullseye/ncurses-doc/init_extended_pair.3ncurses.en.html) -- HIGH confidence

---

### Pitfall 9: Damage Tracking Granularity -- Cell-Level vs Layer-Level

**Feature area:** Damage tracking
**Severity:** High

**What goes wrong:** v1.0 tracks damage at the layer level: each layer has a `bool dirty`. When dirty, `werase()` clears the ENTIRE layer. This is wasteful -- a 1-cell change forces clearing and redrawing an entire 200x50 window.

However, implementing cell-level damage tracking is complex:
1. Need a dirty-rect accumulator per layer (or a dirty bitmask array).
2. `werase()` cannot partially clear -- it always clears the whole window.
3. Partial clearing requires manually writing spaces to only the dirty cells, which needs attribute management.
4. ncurses internally does cell-level diff between virtual and physical screens (that is what doupdate() optimizes). Adding another layer of damage tracking on top may be redundant.

**Why it happens:** ncurses already performs terminal-level diff optimization in `doupdate()`. The question is whether USER-LEVEL damage tracking (skip drawing unchanged content) provides enough benefit to justify the complexity.

**Consequences:**
- Without damage tracking: every dirty layer redraws everything, causing unnecessary CPU work in the draw calls (even though doupdate() only sends changed cells to the terminal).
- With poorly implemented damage tracking: complex bookkeeping, potential for stale content when dirty regions are missed.

**Prevention:**
1. Start with rectangular dirty regions, not cell-level bitmasks. Track `TUI_CellRect dirty_rect` per layer that accumulates the bounding box of all draws since last frame.
2. Only `werase()` the dirty_rect, not the whole window. Use a fill loop for partial clearing.
3. Alternatively, rely on ncurses's internal optimization (doupdate diff) and focus damage tracking on SKIPPING DRAW CALLS rather than skipping terminal output. If a widget hasn't changed, don't call its draw function at all.
4. The two levels are complementary: application-level damage tracking reduces draw call CPU, ncurses-level diff reduces I/O.

**Detection:** Profile with a large terminal (200+ columns). If FPS drops when one small region changes, layer-level redraw is the bottleneck. If FPS is fine, ncurses's internal diff is handling it.

**Sources:** [ncurses internals hackguide](https://invisible-island.net/ncurses/hackguide.html), [ncurses FAQ](https://invisible-island.net/ncurses/ncurses.faq.html), codebase analysis of `tui_frame.c` -- HIGH confidence

---

### Pitfall 10: TERM Must Be Set Correctly for True Color -- Silent Degradation

**Feature area:** True color
**Severity:** High

**What goes wrong:** True color via ncurses requires the `TERM` environment variable to reference a terminfo entry with direct color support (e.g., `xterm-direct`, `konsole-direct`). On this system, the default `TERM` shows `colors#0x100` (256) and `pairs#0x7fff` (32767). Only `xterm-direct` shows `colors#0x1000000` and `RGB` capability.

If the user's TERM is not set to a direct-color variant, ncurses silently falls back to 256 colors. No error is raised. The application believes it is using true color, but all colors are quantized to 256.

**Why it happens:** ncurses color support is entirely driven by terminfo capabilities. Applications cannot override this -- they must work within whatever TERM provides.

**Consequences:**
- Users with `TERM=xterm-256color` (the common default) get 256-color mode, not true color, even on terminals that support it.
- If the application hard-codes `TERM=xterm-direct` at startup, it may break terminal features that depend on the correct TERM value (e.g., different key sequences, missing capabilities).
- tmux users get further complications: tmux has its own TERM and may or may not pass through 24-bit color.

**Prevention:**
1. Check `COLORS` at startup. If `COLORS <= 256`, log a message and use 256-color fallback. If `COLORS > 256`, enable true color path.
2. Do NOT override `TERM`. Document what TERM value the user should set.
3. Provide a runtime API: `tui_color_supports_true_color()` that checks `tigetflag("RGB")` or `COLORS > 256`.
4. Design the color API to be mode-agnostic: `tui_color_rgb(r, g, b)` should work in both modes (256 with nearest mapping, true color with direct encoding). This is already partially done in v1.0.
5. Test in tmux, screen, and bare terminal (kitty, alacritty, xterm).

**Detection:** Print `COLORS` at startup. If it says 256 when user expects true color, TERM is wrong.

**Sources:** Local `infocmp` verification (system ncurses 6.6), [kakoune #1807](https://github.com/mawww/kakoune/issues/1807), [ncurses color man page](https://invisible-island.net/ncurses/man/curs_color.3x.html) -- HIGH confidence

---

### Pitfall 11: mousemask() Must Be Called AFTER initscr(), Not During Input Init

**Feature area:** Mouse input
**Severity:** High

**What goes wrong:** `mousemask()` requires ncurses to be initialized. In v1.0, `TUI_Input_use()` runs during `CEL_Build()`, which is BEFORE `tui_hook_startup()` calls `initscr()`. If `mousemask()` is called in `TUI_Input_use()`, it silently fails (returns 0) because the screen is not yet initialized.

Note: v1.0 already handles this correctly for `define_key()` calls -- they happen to work because they register strings in an internal table that persists. But `mousemask()` actually sends escape sequences to the terminal, which requires an active screen.

**Why it happens:** The ECS boot sequence separates registration (`CEL_Build`, where providers are configured) from initialization (startup hook, where ncurses activates). Mouse mask setup is terminal I/O, not configuration.

**Consequences:** Mouse events never arrive. `has_mouse()` returns FALSE. No error reported.

**Prevention:**
1. Call `mousemask()` in the startup hook (`tui_hook_startup()`) after `initscr()`, not in `TUI_Input_use()`.
2. Store desired mouse mask in the TUI_Input config struct, apply it during startup.
3. Alternatively, call `mousemask()` on the first frame (lazy init), similar to how `tui_frame_ensure_background()` defers layer creation.

**Detection:** Check return value of `mousemask()`. If it returns 0, ncurses was not ready.

**Sources:** [curs_mouse(3X) man page](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) ("If the screen is not initialized... mousemask returns 0"), v1.0 boot sequence analysis -- HIGH confidence

---

## Moderate Pitfalls

Mistakes that cause bugs or performance issues, fixable without redesign.

---

### Pitfall 12: Braille Characters Require Font Support -- Silent Fallback to Garbage

**Feature area:** Sub-cell rendering
**Severity:** Moderate

**What goes wrong:** Braille characters (U+2800-U+28FF) provide 2x4 sub-cell resolution (8 dots per cell). However, they only render correctly if the terminal's font includes the Braille Patterns Unicode block. Many monospace fonts do not include Braille glyphs, or render them at incorrect widths.

Half-block characters (U+2580-U+259F) have much wider font support but provide only 1x2 sub-cell resolution.

**Prevention:**
1. Use half-block characters (U+2580/U+2584) as the default sub-cell rendering mode.
2. Offer Braille as an opt-in mode with a clear warning about font requirements.
3. Test with `wcwidth()` at startup -- Braille characters should return width 1. If `wcwidth(0x2800)` returns -1 or 0, the locale or library does not support them.
4. Document required fonts (e.g., Nerd Fonts, JetBrains Mono, Fira Code all include Braille).

**Detection:** Render a known Braille pattern. If it shows as boxes/replacement characters, font support is missing.

**Sources:** [Braille Patterns Wikipedia](https://en.wikipedia.org/wiki/Braille_Patterns), [Drawille HN discussion](https://news.ycombinator.com/item?id=15602334) -- MEDIUM confidence

---

### Pitfall 13: Custom Compositing Bypasses Panel Z-Order Tracking

**Feature area:** Layer transparency
**Severity:** Moderate

**What goes wrong:** If transparency requires custom compositing (bypassing `update_panels()`), the application must manually maintain z-order. The panel library's `top_panel()`, `bottom_panel()`, and panel iteration become unreliable or useless for transparent layers. Two compositing systems (panel-managed and custom) running simultaneously will fight over screen content.

**Prevention:**
1. Choose one compositing path. If any layer needs transparency, consider managing ALL layer compositing manually (abandon `update_panels()` entirely).
2. Alternatively, use a hybrid: opaque layers use panels, transparent layers use manual compositing into a final output window. The transparent layers must be composited AFTER `update_panels()` but BEFORE `doupdate()`.
3. If going hybrid: use `wnoutrefresh()` after `update_panels()`, then manually composite transparent layers into the virtual screen, then `doupdate()`.
4. Track z-order in `g_layers[]` array ordering, not in panel library state.

**Detection:** Create two overlapping layers, one opaque via panels and one transparent via custom compositing. If the transparent layer appears behind the opaque one despite being higher in z-order, the compositing systems are conflicting.

**Sources:** [panel(3X)](https://invisible-island.net/ncurses/man/panel.3x.html), codebase analysis -- HIGH confidence

---

### Pitfall 14: Damage Tracking Must Account for Layer Movement

**Feature area:** Damage tracking
**Severity:** Moderate

**What goes wrong:** When a layer moves (via `tui_layer_move()`), both the old region (now exposing content below) and the new region (now covering content below) must be marked dirty. If only the moved layer is marked dirty, the region it vacated shows stale content from the old frame.

**Prevention:**
1. On layer move: mark the OLD bounding rect as damaged on all layers below.
2. Mark the NEW bounding rect as damaged on the moved layer.
3. `tui_layer_move()` should call `tui_frame_invalidate_region(old_rect)` before updating position.
4. For simplicity, layer movement could trigger `tui_frame_invalidate_all()` (full redraw). Optimize later if profiling shows it matters.

**Detection:** Move a layer. If the old position shows ghost content, the vacated region is not being redrawn.

**Sources:** Codebase analysis of `tui_layer.c` move implementation -- HIGH confidence

---

### Pitfall 15: mouseinterval() Affects Click Detection Timing

**Feature area:** Mouse input
**Severity:** Moderate

**What goes wrong:** ncurses uses `mouseinterval()` to determine the maximum time between press and release to register as a "click" (vs separate press/release events). The default is 166ms (1/6 second). If this is too long, it adds perceived input lag to click events. If too short, fast clicks are reported as separate press/release pairs.

The v1.0 frame loop runs at 60 FPS (16.6ms per frame). A 166ms mouseinterval means ncurses buffers the press event for up to 10 frames before deciding if it is a click or not.

**Prevention:**
1. Set `mouseinterval(0)` to disable click detection entirely. Process raw press/release events in the application layer.
2. Alternatively, set a shorter interval: `mouseinterval(50)` (3 frames at 60 FPS).
3. For game-like applications, raw press/release is preferable to click events.
4. Document the tradeoff: lower interval = faster response but harder to detect intentional double-clicks.

**Detection:** Click a button and measure the perceived latency. If clicks feel laggy (100ms+), mouseinterval is too high.

**Sources:** [curs_mouse(3X)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- HIGH confidence

---

### Pitfall 16: Sub-Cell Rendering Doubles Color Pair Pressure

**Feature area:** Sub-cell rendering + true color
**Severity:** Moderate

**What goes wrong:** In normal rendering, each cell uses one fg/bg color pair. In sub-cell rendering with half-blocks, each cell needs a UNIQUE pair where fg = one pixel color and bg = the other pixel color. For a 200x50 terminal at 1x2 sub-cell resolution:
- Normal: up to 200x50 = 10,000 unique pairs needed (theoretical max).
- Sub-cell: same 10,000 cells, but each pair is SPECIFIC to the two pixel colors in that cell.
- In practice, gradients or images in sub-cell mode can easily exhaust the 32767 pair limit.

**Prevention:**
1. With 256-color mode: the maximum unique pairs is 257*257 = 66,049 (including default), which exceeds `COLOR_PAIRS` (32767). alloc_pair's LRU will churn.
2. With direct color mode: pairs limit is 65536, and color values are 24-bit, so unique pair combinations are effectively unlimited. LRU churn is certain for image-like content.
3. Quantize sub-cell colors to a smaller palette (e.g., 64 colors) to reduce pair count.
4. Use `reset_color_pairs()` between frames if pair churn causes visual artifacts.
5. Profile alloc_pair usage. If `find_pair()` starts returning -1 frequently, the pool is saturated.

**Detection:** Render a gradient in sub-cell mode. If colors flicker frame-to-frame, alloc_pair is LRU-evicting active pairs.

**Sources:** [new_pair(3X) man page](https://invisible-island.net/ncurses/man/curs_color.3x.html), codebase `tui_color.c` analysis -- HIGH confidence

---

### Pitfall 17: F1 Pause Mode Blocks Mouse Events Too

**Feature area:** Mouse input
**Severity:** Moderate

**What goes wrong:** v1.0 has a pause mode (F1) that switches to blocking input via `nodelay(stdscr, FALSE)` to let the user select/copy text. When mouse events are enabled, mouse clicks during pause mode will be captured by ncurses instead of being passed to the terminal for text selection. The entire purpose of pause mode (text selection) breaks.

**Prevention:**
1. Before entering pause mode, disable mouse events: `mousemask(0, &saved_mask)`.
2. On resume, restore mouse events: `mousemask(saved_mask, NULL)`.
3. This allows the terminal to handle mouse events natively during pause (text selection, copy).

**Detection:** Enable mouse input, press F1, try to select text with mouse. If text selection does not work, mouse events are intercepting the terminal's native selection.

**Sources:** v1.0 `tui_input.c` pause mode implementation, [curs_mouse(3X)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- HIGH confidence

---

## Minor Pitfalls

Mistakes that cause annoyance or minor visual issues, easily fixable.

---

### Pitfall 18: REPORT_MOUSE_POSITION Floods Input Queue

**Feature area:** Mouse input
**Severity:** Minor

**What goes wrong:** Enabling `REPORT_MOUSE_POSITION` in the mousemask causes ncurses to report every mouse movement (every cell the cursor crosses). On a 200-column terminal, dragging the mouse horizontally generates ~200 events. This fills the input queue and can cause visible processing delays.

**Prevention:**
1. Only enable `REPORT_MOUSE_POSITION` when hover detection is actually needed (e.g., tooltip system).
2. When enabled, throttle processing: only use the LAST mouse position event per frame, discard intermediates.
3. Drain the queue aggressively: after getting a mouse move event, continue draining and only keep the most recent position.

**Sources:** [curs_mouse(3X)](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- HIGH confidence

---

### Pitfall 19: Quadrant Block Characters Have Limited Terminal Support

**Feature area:** Sub-cell rendering
**Severity:** Minor

**What goes wrong:** Unicode quadrant characters (U+2596-U+259F) provide 2x2 sub-cell resolution. However, terminal support varies -- some terminals render them with visible seams or at slightly wrong sizes, causing visible grid lines in what should be seamless sub-cell graphics.

**Prevention:**
1. Test quadrant rendering on target terminals (kitty, alacritty, WezTerm, xterm).
2. Fall back to half-blocks if quadrants render poorly.
3. Half-blocks (1x2) are the safest and most widely supported sub-cell technique.

**Sources:** [Alacritty block elements issue #5485](https://github.com/alacritty/alacritty/issues/5485) -- LOW confidence (terminal-specific)

---

### Pitfall 20: alloc_pair Returns -1 on Pool Exhaustion (Edge Case)

**Feature area:** True color, sub-cell rendering
**Severity:** Minor

**What goes wrong:** `alloc_pair()` returns -1 if it cannot allocate (error updating internal index). v1.0's `tui_style_apply()` passes the return value directly to `wattr_set()` as `(short)pair`. If pair is -1, this becomes an invalid pair number, causing undefined rendering behavior.

**Prevention:**
1. Check `alloc_pair()` return value. If -1, fall back to pair 0 (default colors).
2. Log a warning on first occurrence -- it indicates color pair pool exhaustion.
3. This is unlikely in 256-color mode (65K pairs available) but possible with heavy sub-cell rendering or true color.

**Sources:** [new_pair(3X)](https://invisible-island.net/ncurses/man/curs_color.3x.html) alloc_pair return value documentation -- HIGH confidence

---

## Phase-Specific Warnings

Summary table mapping pitfalls to v1.1 feature areas for roadmap planning.

| Feature Area | Critical Pitfalls | High Pitfalls | Moderate Pitfalls | Action |
|---|---|---|---|---|
| Mouse Input | #1 (screen coords), #5 (event queue) | #6 (scroll wheel), #11 (mousemask timing) | #15 (mouseinterval), #17 (F1 pause) | Drain full queue per frame. wmouse_trafo for coords. mousemask after initscr. |
| Sub-Cell Rendering | -- | #7 (fg/bg duality) | #12 (font support), #16 (pair pressure) | Standardize on U+2584. Document fg=bottom, bg=top. |
| True Color | #2 (alloc_pair vs direct) | #8 (short pair), #10 (TERM detection) | #16 (pair pressure) | Detect COLORS at startup. Dual-path (256 + direct). Extended pair API. |
| Damage Tracking | #4 (transparency interaction) | #9 (cell vs layer granularity) | #14 (layer movement) | Start with dirty-rect per layer. Transparency forces overlap tracking. |
| Layer Transparency | #3 (panels are opaque), #4 (dirty tracking breaks) | -- | #13 (dual compositing) | Custom compositing for transparent layers. Accept higher redraw cost. |

## Architectural Recommendation

The five v1.1 features have significant cross-cutting interactions:

1. **True color + sub-cell rendering** both stress the color pair system. Solve true color first (direct color detection, extended pairs) before adding sub-cell rendering.

2. **Layer transparency + damage tracking** are tightly coupled. Transparency invalidates per-layer dirty tracking. Design damage tracking WITH transparency in mind from the start, or plan to rewrite damage tracking after adding transparency.

3. **Mouse input** is the most independent feature -- it has no interaction with the rendering pipeline. Implement it first as a low-risk warm-up.

4. **Suggested ordering:** Mouse input --> True color --> Sub-cell rendering --> Damage tracking --> Layer transparency.

## Sources

- ncurses 6.6 man pages (system-installed): curs_mouse(3X), curs_color(3X), new_pair(3X), panel(3X) -- HIGH confidence
- Local `infocmp -x` and `infocmp -x xterm-direct` verification -- HIGH confidence
- v1.0 codebase: tui_input.c, tui_color.c, tui_layer.c, tui_frame.c, tui_draw.c -- HIGH confidence
- [ncurses official man pages](https://invisible-island.net/ncurses/man/curs_mouse.3x.html) -- HIGH confidence
- [ncurses color documentation](https://invisible-island.net/ncurses/man/curs_color.3x.html) -- HIGH confidence
- [kakoune true color discussion](https://github.com/mawww/kakoune/issues/1807) -- MEDIUM confidence
- [Panel Library HOWTO](https://tldp.org/HOWTO/NCURSES-Programming-HOWTO/panels.html) -- HIGH confidence
- [ncurses transparency blog](http://nion.modprobe.de/blog/archives/205-Transparency-with-ncurses.html) -- MEDIUM confidence
- [Alacritty block elements](https://github.com/alacritty/alacritty/issues/5485) -- LOW confidence
- [Unicode graphics analysis](https://dernocua.github.io/notes/unicode-graphics.html) -- MEDIUM confidence
- [Braille Patterns](https://en.wikipedia.org/wiki/Braille_Patterns) -- MEDIUM confidence

*Pitfalls research: 2026-02-20*
