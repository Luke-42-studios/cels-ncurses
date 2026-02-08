# Phase 1: Foundation - Research

**Researched:** 2026-02-07
**Domain:** ncurses type system, color pair management, drawing context, coordinate mapping
**Confidence:** HIGH

## Summary

This phase establishes the foundational type vocabulary for the cels-ncurses graphics API: `TUI_Style` (color + attributes), `TUI_Color` (RGB with nearest-color mapping), `TUI_Rect` (float coordinates with cell conversion), `TUI_DrawContext` (WINDOW wrapper), and the dynamic color pair pool. All subsequent phases (drawing primitives, layers, frame pipeline) build on these types.

The research confirms that ncurses 6.5 on the target system provides everything needed: `alloc_pair()` / `find_pair()` / `free_pair()` for dynamic color pair management with built-in LRU eviction, `wattr_set()` for atomic attribute application supporting pairs up to 32767, and 256 terminal colors with `can_change_color()` support. The critical insight is that ncurses's own `alloc_pair()` function already implements the exact LRU color pair pool behavior the user specified -- we should use it directly rather than building a custom pool.

The existing codebase uses hardcoded `CP_*` color pair constants (6 pairs), `attron()`/`attroff()` state management, and renders everything to `stdscr`. Phase 1 replaces this foundation with the new type system. No existing code is modified yet -- new files are added alongside existing ones, and migration happens in Phase 5.

**Primary recommendation:** Use ncurses `alloc_pair()` as the color pair pool (it has built-in LRU eviction), map RGB to nearest xterm-256 color index at creation time via a pure lookup function, and use `wattr_set()` for all attribute application.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncursesw | 6.5 | Terminal graphics, color pairs, attribute control | Already in project, provides alloc_pair/find_pair/free_pair, wattr_set |
| C99 standard library | -- | math.h (floorf, ceilf), locale.h (setlocale), stdint.h (uint32_t) | No external deps needed |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| math.h | C99 | floorf/ceilf for float-to-cell coordinate conversion | TUI_Rect conversion functions |
| locale.h | C99 | setlocale(LC_ALL, "") for Unicode support | Called once at startup before initscr() |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| alloc_pair (ncurses built-in) | Custom hash map pool | alloc_pair already does LRU eviction, dedup, and init_pair internally -- custom pool adds code with no benefit |
| RGB nearest-color via lookup table | init_color() to redefine palette entries | init_color modifies shared palette (affects all users of that color slot), not all terminals support can_change_color |
| wattr_set (atomic) | attron/attroff (toggle) | wattr_set prevents state leaks, supports extended pairs, is the modern X/Open API |

**Installation:**
No additional packages needed. ncursesw 6.5 is already installed and linked.

## Architecture Patterns

### Recommended Project Structure (new files for Phase 1)
```
include/cels-ncurses/
    tui_types.h          # TUI_Rect, TUI_CellRect, coordinate conversion
    tui_color.h          # TUI_Color, TUI_Style, TUI_ATTR_* flags, color pool API
    tui_draw_context.h   # TUI_DrawContext struct definition
src/
    graphics/
        tui_color.c      # Color pool implementation (alloc_pair wrapper), RGB mapping
```

### Pattern 1: Value-Type Structs with Stack Allocation
**What:** All new types (TUI_Style, TUI_Rect, TUI_DrawContext, TUI_Color) are plain C structs meant to be stack-allocated and passed by value or pointer. No heap allocation for these foundational types.
**When to use:** Always for per-frame data that does not outlive a function scope.
**Example:**
```c
/* Source: CONTEXT.md user decision -- stack-allocated, no malloc churn */
TUI_Style style = {
    .fg = tui_color_rgb(255, 100, 50),
    .bg = TUI_COLOR_DEFAULT,
    .attrs = TUI_ATTR_BOLD | TUI_ATTR_UNDERLINE
};

TUI_DrawContext ctx = tui_draw_context_create(my_window, 0, 0, 80, 24);
```

### Pattern 2: Eager Resolution at Creation
**What:** RGB-to-terminal-color mapping happens when `tui_color_rgb()` is called, not when the color is used for drawing. The returned `TUI_Color` stores the resolved ncurses color index, not the original RGB.
**When to use:** Every time a color is created from RGB values.
**Example:**
```c
/* Source: CONTEXT.md user decision -- eager mapping at creation */
TUI_Color red = tui_color_rgb(255, 0, 0);
/* red.index is now the nearest xterm-256 color index (e.g., 196) */
/* This index is reused directly with alloc_pair() at draw time */
```

### Pattern 3: Internal Pair Resolution via alloc_pair
**What:** When a style is applied to a window, the color pair is resolved internally using `alloc_pair(fg_index, bg_index)`. The developer never sees pair numbers. alloc_pair deduplicates (returns existing pair if fg/bg combo exists) and handles LRU eviction when the pool fills.
**When to use:** Every time `tui_style_apply()` is called.
**Example:**
```c
/* Source: ncurses man page new_pair(3x), verified on system */
void tui_style_apply(WINDOW* win, TUI_Style style) {
    int fg = style.fg.index;  /* -1 for default */
    int bg = style.bg.index;  /* -1 for default */
    int pair = alloc_pair(fg, bg);  /* ncurses handles dedup + LRU */
    attr_t attrs = tui_attrs_to_ncurses(style.attrs);
    wattr_set(win, attrs, (short)pair, NULL);
}
```

### Pattern 4: Explicit Context Parameter
**What:** All draw functions take `TUI_DrawContext*` as their first parameter. No global "current context" -- the caller explicitly passes the target.
**When to use:** All draw function signatures in Phase 2+.
**Example:**
```c
/* Source: CONTEXT.md user decision -- no global current context */
void tui_draw_rect(TUI_DrawContext* ctx, TUI_CellRect rect, TUI_Style style);
void tui_draw_text(TUI_DrawContext* ctx, int x, int y, const char* text, TUI_Style style);
```

### Anti-Patterns to Avoid
- **attron/attroff state toggles:** Use wattr_set() exclusively. attron/attroff leak state on early returns and are per-window global.
- **Hardcoded color pair constants:** The CP_* enum pattern is exactly what Phase 1 replaces. No new CP_* constants.
- **Storing ncurses pair index in style:** TUI_Style stores fg/bg color indices, not pair indices. Pairs are resolved at apply-time via alloc_pair.
- **Using COLOR_PAIR() macro:** This macro truncates to 8 bits (max 256 pairs). Use wattr_set with pair parameter instead.
- **init_color() for RGB mapping:** Modifies the shared terminal palette, affecting all uses of that color slot. Use nearest-index mapping instead.

## Don't Hand-Roll

Problems that look simple but have existing solutions:

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Color pair allocation + dedup | Custom hash map with pair indices | `alloc_pair(fg, bg)` | ncurses 6.1+ provides this with built-in LRU eviction, dedup, and init_pair integration |
| Color pair lookup (does combo exist?) | Manual pair table scan | `find_pair(fg, bg)` | Returns pair index or -1, O(1) via ncurses internal fast index |
| Color pair deallocation | Manual free + tracking | `free_pair(pair)` | Marks pair as unused in ncurses internal table |
| Atomic attribute application | attron/attroff wrappers | `wattr_set(win, attrs, pair, NULL)` | Single call replaces all attributes + color pair atomically |
| Float-to-int coordinate rounding | Custom rounding logic | `floorf()` / `ceilf()` from math.h | Standard C99, well-defined behavior for negative values |

**Key insight:** ncurses 6.1+ added `alloc_pair()` / `find_pair()` / `free_pair()` specifically to solve the dynamic color pair management problem. These functions implement the exact LRU pool with dedup that the user specified. Using them eliminates ~150 lines of custom pool code and is guaranteed correct with ncurses internals.

## Common Pitfalls

### Pitfall 1: COLOR_PAIR() Macro Truncates Above 256
**What goes wrong:** `COLOR_PAIR(n)` packs the pair number into `A_COLOR` using only 8 bits. `COLOR_PAIR(259)` silently becomes `COLOR_PAIR(3)`. With `alloc_pair()` allocating pair numbers up to 32766, this would silently corrupt styles.
**Why it happens:** Legacy macro from when terminals supported only 64 pairs.
**How to avoid:** Never use `COLOR_PAIR()` macro. Always use `wattr_set(win, attrs, (short)pair, NULL)` which passes the pair number through a separate parameter supporting up to 32767.
**Warning signs:** Incorrect colors appearing when many unique color combinations are in use.

### Pitfall 2: Color Pair 0 Is Reserved
**What goes wrong:** `alloc_pair()` never returns 0. `init_pair(0, ...)` fails. Pair 0 is always the terminal default.
**Why it happens:** ncurses reserves pair 0 as "no color" / default foreground+background.
**How to avoid:** When both fg and bg are `TUI_COLOR_DEFAULT`, use pair 0 directly (no alloc_pair call needed). The pool correctly starts allocating from index 1.
**Warning signs:** `alloc_pair(-1, -1)` returns 1 rather than 0 -- wasting a pair slot for the default case.

### Pitfall 3: Missing setlocale Before initscr
**What goes wrong:** Box-drawing characters (WACS_HLINE, etc.) and any Unicode output renders as garbage. ncurses documentation states: "If the locale is not initialized, the library assumes ISO 8859-1."
**Why it happens:** Current codebase calls `initscr()` without `setlocale()` first.
**How to avoid:** Add `setlocale(LC_ALL, "")` before `initscr()` in `tui_window_startup()`.
**Warning signs:** Any non-ASCII character (box-drawing, Unicode text) renders incorrectly.

### Pitfall 4: Mixing attron/attroff With wattr_set
**What goes wrong:** `attron()` adds to the current attribute state. If previous code set `A_BOLD` via `attron()` and new code uses `wattr_set()`, the bold leaks because `wattr_set()` replaces the entire attribute word -- but only if the old code did not clean up.
**Why it happens:** Transitional period where old code uses attron/attroff and new code uses wattr_set.
**How to avoid:** The new `tui_style_apply()` function uses `wattr_set()` exclusively. Existing component renderers still use `attron()`/`attroff()`. They coexist safely as long as they operate on different windows (which they will once layers exist in Phase 3).
**Warning signs:** Attribute bleed-through between draw calls.

### Pitfall 5: Static Globals in INTERFACE Library Headers
**What goes wrong:** `static` variables in headers create separate copies per translation unit. State desyncs between files.
**Why it happens:** This module is a CMake INTERFACE library -- all sources compile in the consumer's context. The existing `g_render_row` in `tui_components.h` already has this bug.
**How to avoid:** New headers contain only struct definitions, macro constants, and `extern` function declarations. All mutable state lives in `.c` files. The color pool state is in `tui_color.c`.
**Warning signs:** Color pool appears empty in one file but full in another.

### Pitfall 6: Negative Color Index for Default Colors
**What goes wrong:** `use_default_colors()` enables `-1` as a valid color index meaning "terminal default." Code that validates `color_index >= 0` incorrectly rejects default colors.
**Why it happens:** Natural assumption that color indices are non-negative.
**How to avoid:** `TUI_COLOR_DEFAULT` should store `-1` as its internal index. Validation checks should allow `-1` explicitly. `alloc_pair(-1, -1)` works correctly with ncurses.
**Warning signs:** Default terminal colors cannot be used after the new color system is in place.

## Code Examples

Verified patterns from official sources and system testing:

### TUI_Color: RGB-to-Nearest-256 Mapping
```c
/* Source: xterm 256-color specification, verified with system ncurses 6.5 */

/* The 6 component levels in the xterm 256-color cube (colors 16-231) */
static const uint8_t CUBE_LEVELS[6] = { 0, 95, 135, 175, 215, 255 };

/* Greyscale ramp: colors 232-255, values 8 to 238 in steps of 10 */
/* grey_value = 8 + (index - 232) * 10 */

/* Map a single 0-255 component to nearest cube index (0-5) */
static int nearest_cube_index(uint8_t val) {
    if (val < 48)  return 0;  /* midpoint(0, 95)  = 47.5  */
    if (val < 115) return 1;  /* midpoint(95, 135) = 115   */
    if (val < 155) return 2;  /* midpoint(135,175) = 155   */
    if (val < 195) return 3;  /* midpoint(175,215) = 195   */
    if (val < 235) return 4;  /* midpoint(215,255) = 235   */
    return 5;
}

/* Map RGB (0-255 per channel) to nearest xterm-256 color index */
static int rgb_to_nearest_256(uint8_t r, uint8_t g, uint8_t b) {
    /* Try color cube (16-231) */
    int ci = nearest_cube_index(r);
    int gi = nearest_cube_index(g);
    int bi = nearest_cube_index(b);
    int cube_color = 16 + (ci * 36) + (gi * 6) + bi;
    uint8_t cr = CUBE_LEVELS[ci], cg = CUBE_LEVELS[gi], cb = CUBE_LEVELS[bi];
    int cube_dist = (r-cr)*(r-cr) + (g-cg)*(g-cg) + (b-cb)*(b-cb);

    /* Try greyscale ramp (232-255) */
    int grey_avg = (r + g + b) / 3;
    int grey_idx = (grey_avg - 8) / 10;
    if (grey_idx < 0) grey_idx = 0;
    if (grey_idx > 23) grey_idx = 23;
    int grey_val = 8 + grey_idx * 10;
    int grey_dist = (r-grey_val)*(r-grey_val) + (g-grey_val)*(g-grey_val) + (b-grey_val)*(b-grey_val);

    return (grey_dist < cube_dist) ? (232 + grey_idx) : cube_color;
}
```

### TUI_Style: Applying Style via wattr_set
```c
/* Source: ncurses curs_attr(3x) man page, verified on system */

/* Attribute flag definitions (TUI_ATTR_*) */
#define TUI_ATTR_NORMAL    0x00
#define TUI_ATTR_BOLD      0x01
#define TUI_ATTR_DIM       0x02
#define TUI_ATTR_UNDERLINE 0x04
#define TUI_ATTR_REVERSE   0x08

/* Convert TUI_ATTR flags to ncurses attr_t */
static attr_t tui_attrs_to_ncurses(uint32_t flags) {
    attr_t a = A_NORMAL;
    if (flags & TUI_ATTR_BOLD)      a |= A_BOLD;
    if (flags & TUI_ATTR_DIM)       a |= A_DIM;
    if (flags & TUI_ATTR_UNDERLINE) a |= A_UNDERLINE;
    if (flags & TUI_ATTR_REVERSE)   a |= A_REVERSE;
    return a;
}

/* Apply a TUI_Style to an ncurses WINDOW atomically */
static void tui_style_apply(WINDOW* win, TUI_Style style) {
    int pair;
    if (style.fg.index == -1 && style.bg.index == -1) {
        pair = 0;  /* Default pair -- no alloc needed */
    } else {
        pair = alloc_pair(style.fg.index, style.bg.index);
    }
    attr_t attrs = tui_attrs_to_ncurses(style.attrs);
    wattr_set(win, attrs, (short)pair, NULL);
}
```

### alloc_pair: Verified Behavior
```c
/* Source: verified on system -- ncurses 6.5, COLOR_PAIRS=32767 */

/* alloc_pair returns existing pair for duplicate fg/bg combos */
int p1 = alloc_pair(COLOR_RED, -1);   /* Returns 1 (first allocation) */
int p2 = alloc_pair(COLOR_GREEN, -1); /* Returns 2 (new combo) */
int p3 = alloc_pair(COLOR_RED, -1);   /* Returns 1 (dedup -- same as p1) */

/* find_pair checks existence without allocating */
int found = find_pair(COLOR_RED, -1);    /* Returns 1 */
int nope  = find_pair(COLOR_WHITE, -1);  /* Returns -1 (not allocated) */

/* When pool fills (32766 unique combos), LRU eviction kicks in automatically */
```

### wattr_set: Pair Range Verification
```c
/* Source: verified on system -- ncurses 6.5, sizeof(short)=2 */

/* On this system, short holds up to 32767, which covers the full
 * COLOR_PAIRS range (32767). The opts parameter for extended pairs
 * is NOT needed. wattr_set with short pair parameter works for all
 * pair numbers returned by alloc_pair(). */

init_pair(300, COLOR_CYAN, COLOR_MAGENTA);
wattr_set(stdscr, A_BOLD, (short)300, NULL);  /* Works correctly */

/* Verification: read back the pair */
attr_t cur_attrs;
short cur_pair;
wattr_get(stdscr, &cur_attrs, &cur_pair, NULL);
/* cur_pair == 300 -- correct */
```

### TUI_Rect: Float-to-Cell Conversion
```c
/* Source: CONTEXT.md user decision + REQUIREMENTS.md FNDN-05/FNDN-06 */

#include <math.h>

typedef struct TUI_Rect {
    float x, y, w, h;
} TUI_Rect;

typedef struct TUI_CellRect {
    int x, y, w, h;
} TUI_CellRect;

/* Convert float rect to integer cell rect */
/* floorf for position: snap to top-left cell */
/* ceilf for dimensions: ensure content not clipped */
static inline TUI_CellRect tui_rect_to_cells(TUI_Rect r) {
    return (TUI_CellRect){
        .x = (int)floorf(r.x),
        .y = (int)floorf(r.y),
        .w = (int)ceilf(r.w),
        .h = (int)ceilf(r.h)
    };
}

/* Intersection of two cell rects (for scissor/clip) */
static inline TUI_CellRect tui_cell_rect_intersect(TUI_CellRect a, TUI_CellRect b) {
    int x1 = a.x > b.x ? a.x : b.x;
    int y1 = a.y > b.y ? a.y : b.y;
    int x2 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y2 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    int w = x2 - x1;
    int h = y2 - y1;
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    return (TUI_CellRect){ .x = x1, .y = y1, .w = w, .h = h };
}
```

### setlocale: Correct Placement
```c
/* Source: ncurses(3X) man page -- "If the locale is not initialized,
 * the library assumes ISO 8859-1" */

#include <locale.h>

static void tui_window_startup(void) {
    /* MUST be before initscr() -- enables Unicode box-drawing chars */
    setlocale(LC_ALL, "");

    initscr();
    /* ... rest of initialization ... */
}
```

## State of the Art

| Old Approach (current code) | Current Approach (Phase 1) | When Changed | Impact |
|------|------|------|------|
| `attron(COLOR_PAIR(CP_X) \| A_BOLD)` / `attroff(...)` | `wattr_set(win, attrs, pair, NULL)` | ncurses 5.4+ (X/Open Curses) | No state leaks, supports >256 pairs |
| Hardcoded `CP_SELECTED = 1` enum | `alloc_pair(fg, bg)` dynamic pool | ncurses 6.1 (2018) | Unlimited color combinations, auto LRU |
| `init_pair(1, COLOR_YELLOW, -1)` manual setup | `alloc_pair()` handles init_pair internally | ncurses 6.1 | No manual pair numbering |
| Direct `stdscr` usage | `WINDOW*` passed via `TUI_DrawContext` | Design decision | Enables layers in Phase 3 |
| Integer-only coordinates | Float `TUI_Rect` with `tui_rect_to_cells()` | Design decision | Clay float layout compatibility |

**Deprecated/outdated:**
- `COLOR_PAIR()` macro: Truncates to 8 bits. Use `wattr_set()` pair parameter instead.
- `attron()`/`attroff()`: Toggle-based state. Use `wattr_set()` for atomic replace.
- Manual `init_pair()` numbering: Use `alloc_pair()` for automatic management.

## Discretion Recommendations

### Default/Inherit Sentinel for Colors
**Recommendation:** Support `TUI_COLOR_DEFAULT` with internal index `-1`.

**Rationale:** ncurses already uses `-1` for default colors (via `use_default_colors()`). `alloc_pair(-1, bg)` correctly creates a pair with the terminal's default foreground. This enables partial style overrides: a child element can set `fg = tui_color_rgb(255,0,0)` while keeping `bg = TUI_COLOR_DEFAULT` to inherit the terminal background.

**Implementation:**
```c
typedef struct TUI_Color {
    int index;  /* -1 = default terminal color, 0-255 = xterm color index */
} TUI_Color;

#define TUI_COLOR_DEFAULT ((TUI_Color){ .index = -1 })
```

### Clipping State on TUI_DrawContext
**Recommendation:** Embed a single `TUI_CellRect clip` field in `TUI_DrawContext`. Do NOT add a clip stack here.

**Rationale:** Phase 2 adds `tui_push_scissor()` / `tui_pop_scissor()` which manage a separate clip stack. `TUI_DrawContext.clip` represents the current effective clip rect (the intersection of all active scissors). The stack lives in a separate module-level structure. Embedding just the current clip keeps `TUI_DrawContext` small (stack-friendly) while giving draw functions the bounds they need.

```c
typedef struct TUI_DrawContext {
    WINDOW* win;
    int x, y;             /* Origin offset within the window */
    int width, height;    /* Drawable area dimensions */
    TUI_CellRect clip;    /* Current effective clip rect (set by scissor stack) */
} TUI_DrawContext;
```

### TUI_Rect Representation
**Recommendation:** Use `x/y/w/h` (origin + dimensions) for `TUI_Rect`, not `x1/y1/x2/y2`.

**Rationale:**
1. Clay uses `{ x, y, width, height }` in `Clay_BoundingBox` -- direct mapping, no conversion
2. ncurses drawing functions take position + size: `mvwhline(win, y, x, ch, w)`, `newwin(h, w, y, x)`
3. `x/y/w/h` is more natural for "shrink by N" and "expand by N" utilities
4. Intersection math works fine with either representation (compute intersection, check for non-negative w/h)

### Color Pool Sizing
**Recommendation:** No custom sizing needed. Use ncurses `alloc_pair()` directly.

**Rationale:** ncurses manages the pool internally. `COLOR_PAIRS` on the target system is 32767. `alloc_pair()` allocates from 1 to `COLOR_PAIRS-1` with automatic LRU eviction when full. No configuration needed from our side.

### Pool Persistence
**Recommendation:** Persist across frames (already the behavior of alloc_pair).

**Rationale:** `alloc_pair()` returns existing pairs for duplicate fg/bg combinations. Pairs are never freed unless the pool fills up (32766 unique combos), at which point LRU eviction kicks in. This matches the user's decision: "Pair pool persists across frames with LRU eviction only when full."

## Open Questions

Things that couldn't be fully resolved:

1. **NCURSES_EXT_COLORS is NOT defined on target system**
   - What we know: The system ncursesw 6.5 was compiled without `NCURSES_EXT_COLORS`. However, `wattr_set` with `short pair` works correctly for pair numbers up to 32767 (verified), and `COLOR_PAIRS` is 32767 -- so the standard short parameter covers the full range.
   - What's unclear: Whether any edge case exists where extended color support would be needed despite short being sufficient.
   - Recommendation: Use the standard `wattr_set(win, attrs, (short)pair, NULL)` API. The opts parameter for extended pairs is not needed on this system.

2. **First 16 color indices may differ between terminals**
   - What we know: Colors 0-15 are terminal-theme-dependent. Colors 16-255 are standardized (xterm 6x6x6 cube + greyscale ramp).
   - What's unclear: Whether the RGB nearest-color mapping should skip colors 0-15 to avoid theme-dependent appearance.
   - Recommendation: Map to the full 0-255 range. The first 16 colors are the most commonly used and users expect `rgb(255,0,0)` to produce red regardless of theme.

3. **alloc_pair with both fg=-1 and bg=-1**
   - What we know: `alloc_pair(-1, -1)` allocates pair 1 (not pair 0). Pair 0 is the immutable default.
   - What's unclear: Whether this wastes a pair slot that could be avoided.
   - Recommendation: Special-case default+default to use pair 0 directly without calling alloc_pair. Only call alloc_pair when at least one of fg/bg is not -1.

## Sources

### Primary (HIGH confidence)
- ncurses 6.5 on target system -- direct compilation and runtime testing of alloc_pair, find_pair, wattr_set, init_pair, COLOR_PAIRS, can_change_color
- [ncurses curs_color(3x) man page](https://invisible-island.net/ncurses/man/curs_color.3x.html) -- init_pair, init_extended_pair, COLOR_PAIRS limits
- [ncurses new_pair(3x) man page](https://invisible-island.net/ncurses/man/new_pair.3x.html) -- alloc_pair, find_pair, free_pair, LRU eviction behavior
- [ncurses curs_attr(3x) man page](https://man7.org/linux/man-pages/man3/wattr_set.3x.html) -- wattr_set signature, opts parameter for extended pairs
- Existing codebase inspection -- all source files read and analyzed

### Secondary (MEDIUM confidence)
- [xterm 256-color palette reference](https://jonasjacek.github.io/colors/) -- color cube levels [0, 95, 135, 175, 215, 255], greyscale ramp
- [ncurses 6.1 announcement](https://lists.gnu.org/archive/html/info-gnu/2018-01/msg00012.html) -- alloc_pair/find_pair/free_pair introduction

### Tertiary (LOW confidence)
- None -- all claims verified with primary or secondary sources

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- verified on system with test programs
- Architecture: HIGH -- patterns derived from user decisions in CONTEXT.md and verified ncurses APIs
- Pitfalls: HIGH -- sourced from ncurses man pages and verified with system tests
- RGB mapping: HIGH -- xterm-256 color cube is standardized and well-documented
- alloc_pair behavior: HIGH -- directly tested on target system

**Research date:** 2026-02-07
**Valid until:** 2026-05-07 (stable domain -- ncurses and xterm-256 color specifications rarely change)
