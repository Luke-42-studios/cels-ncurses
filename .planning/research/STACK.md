# Stack Research: cels-ncurses Graphics API

**Research Date:** 2026-02-07
**Confidence:** HIGH
**Domain:** Terminal graphics API / ncurses rendering engine

## Key Finding

No new external dependencies needed. ncurses 6.5 (already installed) provides every primitive required: `WINDOW` objects for drawing surfaces, the `panel` library for z-ordered layering, `derwin()` for clipping/scissoring, and `cchar_t`/`WACS_*` constants for Unicode box-drawing.

One build system change: Add `-lpanelw` to CMakeLists.txt link flags and `#include <panel.h>` to new source files. The panel library is part of the ncurses distribution — no new package install required.

## Drawing Primitives

| Primitive | Primary API | Why |
|-----------|-------------|-----|
| Filled rectangle | `mvwhline(win, y+row, x, ch, w)` in a loop | Does not advance cursor, targets specific WINDOW, one call per row |
| Outlined rectangle (window edge) | `wborder(win, ls, rs, ts, bs, tl, tr, bl, br)` or `box(win, 0, 0)` | Single call, 8 configurable character positions |
| Outlined rectangle (arbitrary pos) | `mvwaddch` for corners + `mvwhline`/`mvwvline` for sides | Full position control within any window |
| Single-line borders | `WACS_ULCORNER`, `WACS_HLINE`, `WACS_VLINE`, etc. | Standard, always available |
| Double-line borders | `WACS_D_ULCORNER`, `WACS_D_HLINE`, `WACS_D_VLINE`, etc. | ncursesw extension, verified present |
| Thick borders | `WACS_T_ULCORNER`, `WACS_T_HLINE`, `WACS_T_VLINE`, etc. | ncursesw extension, verified present |
| Rounded corners | `setcchar()` with U+256D/U+256E/U+256F/U+2570 | No WACS_* constant exists; must construct cchar_t manually |
| Bounded text | `mvwaddnstr(win, y, x, text, max_len)` | Length limit prevents overflow — critical for Clay text bounds |
| Formatted text | `mvwprintw(win, y, x, fmt, ...)` | No length limit — use only when format output length is known |
| Attribute control | `wattr_set(win, attrs, pair, NULL)` | Modern X/Open API, separate color pair param, supports >256 pairs |

### Critical Migration

All existing `stdscr`-targeted calls must migrate to `w*` variants (`mvwaddnstr`, `mvwhline`, `wattr_set`, etc.) that target specific `WINDOW` objects, enabling per-layer drawing.

## Layer Management (panel library)

| Function | Purpose |
|----------|---------|
| `new_panel(WINDOW*)` | Create layer, place on top of stack |
| `del_panel(PANEL*)` | Remove layer from stack (does NOT free the WINDOW) |
| `show_panel` / `hide_panel` | Toggle layer visibility |
| `top_panel` / `bottom_panel` | Reorder in z-stack |
| `move_panel(pan, y, x)` | Reposition (NEVER use `mvwin` on panel windows) |
| `update_panels()` | Recalculate visibility, call wnoutrefresh per panel — call once per frame before `doupdate()` |
| `set_panel_userptr` / `panel_userptr` | Attach metadata (name, z-order hint) to panels |
| `replace_panel(pan, new_win)` | Swap window on resize (use with `wresize()`) |

**Critical rules:**
- Never call `wnoutrefresh()` manually on panel-managed windows
- Never use `mvwin()` on panel windows — use `move_panel()` instead
- `update_panels()` replaces `wnoutrefresh(stdscr)` in the frame loop

## Scissor/Clipping (derwin)

- `derwin(parent, nlines, ncols, begin_y, begin_x)` creates a derived window with parent-relative coordinates
- Drawing to a derwin is automatically clipped to its boundaries
- Shares memory with parent (zero copy)
- Use `derwin` not `subwin` (same behavior, but derwin uses parent-relative coords)
- After destroying a derwin, call `touchwin()` on parent to mark dirty
- For scroll containers, use `newpad()` + `pnoutrefresh()` (pads can be larger than screen)
- Caveat: pads cannot be managed by panels; use `copywin()` to blit pad content into panel windows

## Color System

- Use `init_pair(n, fg, bg)` with a dynamic allocation pool (avoid hardcoded pair numbers)
- Use `wattr_set(win, attrs, pair, NULL)` instead of `attron(COLOR_PAIR(n))` — supports >256 pairs
- `init_extended_pair(pair, fg, bg)` available for >32767 pairs (ncurses 6.5 extension)
- `use_default_colors()` + `-1` already in codebase for terminal-native colors
- Do NOT use `init_color()` for core colors (not all terminals support `can_change_color()`)

## What NOT to Use

| Avoid | Use Instead | Why |
|-------|-------------|-----|
| `mvprintw()` / `printw()` | `mvwprintw()` / `mvwaddnstr()` | stdscr-targeted; all drawing must target specific WINDOWs |
| `wrefresh()` | `wnoutrefresh()` + `doupdate()` | Batched updates, single terminal I/O burst |
| `attron(COLOR_PAIR(n))` | `wattr_set(win, attrs, pair, NULL)` | Window-specific, >256 pair support |
| `subwin()` | `derwin()` | Parent-relative coords vs screen-relative |
| `mvwin()` on panels | `move_panel()` | Panel stack position tracking |
| `scrollok()` on layers | Manual positioning | Automatic scroll defeats explicit layout |
| `A_BLINK` | `A_REVERSE` | Unreliable across terminals |
| `immedok()` | Explicit refresh cycle | Defeats batched rendering |

## Build System Change

```cmake
find_library(PANEL_LIBRARY NAMES panelw panel)
target_link_libraries(cels-ncurses INTERFACE
    ${CURSES_LIBRARIES}
    ${PANEL_LIBRARY}
    cels
)
```

## SDL Mental Model Mapping

| SDL Concept | cels-ncurses Equivalent |
|-------------|------------------------|
| `SDL_CreateRenderer` | `tui_graphics_init()` |
| `SDL_SetRenderTarget` | Set active layer WINDOW |
| `SDL_RenderFillRect` | `tui_draw_filled_rect(layer, rect, color)` |
| `SDL_RenderDrawRect` | `tui_draw_border_rect(layer, rect, style)` |
| `SDL_RenderPresent` | `update_panels()` + `doupdate()` |
| Render target | Layer (PANEL + WINDOW) |
| Viewport / clip rect | `derwin()` |

## Roadmap Implications

1. **Phase 1: Drawing primitives** — Implement `mvwhline`/`mvwvline`/`mvwaddnstr` wrappers that target WINDOWs. Foundation everything else builds on.
2. **Phase 2: Layer system** — Add panel library integration. Requires the CMake change. All subsequent drawing targets layer windows.
3. **Phase 3: Clipping/scissoring** — Implement derwin-based scissor stack. Depends on layers being in place.
4. **Phase 4: Color system** — Dynamic color pair allocation pool, replacing hardcoded CP_* constants.
5. **Phase 5: Pipeline integration** — Wire the new APIs into the CELS frame loop, replacing stdscr-based rendering.

## Open Questions

- **Pad + panel compositing:** When a scroll container (pad) needs z-ordering with panels, the `copywin()` approach needs profiling.
- **Terminal resize handling:** `SIGWINCH` + `wresize()` + `replace_panel()` interaction needs validation.
- **Wide-character text measurement:** For Clay integration, text width measurement in terminal cells needs `wcswidth()` or similar.

---
*Stack research: 2026-02-07*
