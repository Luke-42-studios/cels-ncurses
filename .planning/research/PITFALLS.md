# Domain Pitfalls

**Domain:** Terminal graphics API / ncurses rendering engine
**Researched:** 2026-02-07
**Confidence:** HIGH (ncurses 6.5 man pages, codebase inspection)

## Critical Pitfalls

### Pitfall 1: Mixing stdscr with Panels Causes Visual Corruption

**What goes wrong:** The current codebase renders everything to stdscr. The panels library sits above stdscr and stdscr is not part of the panel stack. Writing to stdscr after panels are introduced causes flickering, ghost characters, and z-order violations. The panel(3X) man page states: "The stdscr window is beneath all panels, and is not considered part of the stack."

**Consequences:** Visual artifacts where stdscr content bleeds through panels. erase() on stdscr clears panel content.

**Prevention:**
1. Create a dedicated full-screen WINDOW via newwin(LINES, COLS, 0, 0) attached to a panel for the base layer. Never write to stdscr after panel init.
2. All drawing primitives must target a WINDOW* parameter, never assume stdscr.
3. Replace erase() with werase(win) on the appropriate panel window.
4. Use update_panels() then doupdate() -- never wnoutrefresh() or wrefresh() on panel windows.

**Detection:** Ghost characters where lower panel content shows through. Characters that stick after panel hide/move.
**Phase:** Layer System (first phase).

### Pitfall 2: Panel Resize Requires replace_panel, Not wresize Alone

**What goes wrong:** wresize() on panel windows leaves panel internal data stale. update_panels() uses stale geometry causing clipping errors or crashes. panel(3X) documents replace_panel() for this case.

**Prevention:**
1. On SIGWINCH: resizeterm() first, then for each panel: wresize, replace_panel, move_panel.
2. After all: touchwin() each, update_panels(), doupdate().
3. Never call resizeterm() from signal handler -- set flag, handle in frame loop.

**Detection:** Crashes or garbled display after terminal resize.
**Phase:** Layer System phase.

### Pitfall 3: COLOR_PAIR Macro Silently Truncates Beyond 256 Pairs

**What goes wrong:** COLOR_PAIR(n) packs pair number into A_COLOR (8 bits). COLOR_PAIR(259) silently becomes COLOR_PAIR(3). curs_attr(3X) documents this explicitly.

**Prevention:**
1. Use fixed small set of pairs initially (current 6 is fine).
2. For dynamic allocation: use alloc_pair/find_pair/free_pair ncurses extensions.
3. Or extended API: init_extended_pair() with wattr_set().
4. Check COLOR_PAIRS after start_color().

**Detection:** assert(pair_num < COLOR_PAIRS && pair_num < 256) for legacy API.
**Phase:** Color/Attribute System phase.

### Pitfall 4: Wide Characters Break Column Arithmetic

**What goes wrong:** CJK characters occupy 2 terminal columns. strlen() reports bytes not columns. Must use wcswidth()/wcwidth() for all column calculations. Clipping half a wide char corrupts the cell.

**Prevention:**
1. Use wcswidth()/wcwidth() everywhere, never strlen() for column math.
2. Check double-width boundary when clipping text.
3. Use mvwhline()/mvwvline() for fills.

**Detection:** Test with CJK/emoji strings. Check border alignment.
**Phase:** Text Rendering primitive phase.

### Pitfall 5: Missing setlocale Before initscr Breaks All Unicode

**What goes wrong:** ncurses(3X): "If the locale is not initialized, the library assumes ISO 8859-1." WACS_* box-drawing chars render as garbage. Current code calls initscr() without setlocale().

**Prevention:** Add setlocale(LC_ALL, "") before initscr().
**Detection:** Any non-ASCII character renders incorrectly.
**Phase:** First phase prerequisite.

### Pitfall 6: mvwin vs move_panel -- Wrong Move Function

**What goes wrong:** mvwin() on panel window does not update panel metadata. panel(3X) warns: "Be sure to use move_panel, not mvwin."

**Prevention:** Always use move_panel(). Layer API should wrap this.
**Detection:** Panel occlusion incorrect after move.
**Phase:** Layer System phase.

## Moderate Pitfalls

### Pitfall 7: attron/attroff State Leaks Across Draw Calls

**What goes wrong:** attron()/attroff() toggle global window state. Early returns skip attroff(), leaking attributes.

**Prevention:** Use wattr_set() (atomic replace, not toggle). Save/restore with wattr_get() at entry/exit.
**Phase:** Color/Attribute System phase.

### Pitfall 8: erase() vs werase() vs wclear()

**What goes wrong:** wclear() forces complete terminal repaint via clearok(TRUE). erase() targets stdscr only.

**Prevention:** Use werase(win) in frame loop. Reserve wclear() for corruption recovery. Never erase() with panels.
**Phase:** Frame Pipeline phase.

### Pitfall 9: Subwindows Are Not Scissors

**What goes wrong:** subwin()/derwin() share parent memory, cannot stack, cannot attach to panels. Wrong abstraction for clipping.

**Prevention:** Software clip rect stack with bounds-checking on every write. Clay SCISSOR_START/END maps to push/pop clip rect.
**Phase:** Scissor/Clipping phase.

### Pitfall 10: SIGWINCH Handler Conflict

**What goes wrong:** Custom SIGWINCH handler replaces ncurses built-in. ncurses stops auto-calling resizeterm().

**Prevention:** Let ncurses handle SIGWINCH. Handle KEY_RESIZE from wgetch() in input loop. Use sigaction() not signal().
**Phase:** Layer System phase.

### Pitfall 11: Frame Timing Drift

**What goes wrong:** usleep(full_budget) ignores work duration. 5ms work + 16ms sleep = 47 FPS not 60.

**Prevention:** clock_gettime(CLOCK_MONOTONIC) before/after, nanosleep() remaining budget only.
**Phase:** Frame Pipeline phase.

### Pitfall 12: Static Globals in INTERFACE Library Headers

**What goes wrong:** static int g_render_row in header gives each TU its own copy. State desyncs.

**Prevention:** extern declarations in headers, definitions in one .c file.
**Phase:** All phases.

### Pitfall 13: show_panel vs top_panel Portability

**What goes wrong:** Identical in ncurses but semantically different in System V. show_panel for hidden->visible, top_panel for visible->top.

**Prevention:** Use correct function for each case. Wrap in layer API.
**Phase:** Layer System phase.

## Minor Pitfalls

### Pitfall 14: Use WACS Constants for Borders

**Prevention:** wborder()/wborder_set() with WACS_* for terminal fallback. Rounded corners via setcchar() + wadd_wch().
**Phase:** Border Drawing phase.

### Pitfall 15: doupdate() Exactly Once Per Frame

**Prevention:** update_panels() then doupdate() once. Primitives never call refresh functions.
**Phase:** Frame Pipeline phase.

### Pitfall 16: Color Pair 0 Is Immutable

**Prevention:** Allocate from index 1. Exclude 0 from dynamic allocation.
**Phase:** Color/Attribute System phase.

### Pitfall 17: wgetch() Returns One Key Per Call

**Prevention:** Loop wgetch() until ERR to drain buffer each frame.
**Phase:** Frame Pipeline phase.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Layer System | #1 stdscr+panels; #2 resize; #6 mvwin; #10 SIGWINCH; #13 show/top | Panel-only architecture. KEY_RESIZE in input. |
| Drawing Primitives | #4 wide chars; #5 setlocale; #14 ACS/WACS | wcswidth/wcwidth. setlocale before initscr. |
| Color/Attribute | #3 256 limit; #7 attr leaks; #16 pair 0 | wattr_set atomic. Extended pairs. Index from 1. |
| Scissor/Clipping | #9 subwin not scissors | Software clip rect stack. |
| Frame Pipeline | #8 wclear; #11 timing; #15 doupdate; #17 input | werase. Measure+sleep. Single doupdate. |
| INTERFACE Library | #12 static headers | extern only. |

## ECS Integration Warnings

### ECS Pitfall A: Primitives Must Be ECS-Agnostic

Draw primitives must accept explicit parameters and know nothing about entities. Clay renderer calls from render command array, not ECS queries.

**Prevention:** Pure functions: tui_draw_rect(ctx, x, y, w, h, style).

### ECS Pitfall B: Layer Lifecycle Independent of Entity Lifecycle

Panels as ECS components risk use-after-free if destroyed mid-frame by ECS GC.

**Prevention:** Graphics API owns layer lifetime. ECS systems select but do not own layers.

## Sources

- ncurses 6.5 man pages: panel(3X), curs_color(3X), curs_attr(3X), curs_refresh(3X), curs_addch(3X), curs_add_wch(3X), curs_border(3X), curs_touch(3X), resizeterm(3X), wresize(3X), new_pair(3X), ncurses(3X), wcwidth(3) -- HIGH confidence
- Direct codebase inspection -- HIGH confidence
- .planning/codebase/CONCERNS.md -- HIGH confidence

*Pitfalls research: 2026-02-07*
