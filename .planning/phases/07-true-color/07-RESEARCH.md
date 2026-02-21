# Phase 7: True Color - Research

**Researched:** 2026-02-20
**Domain:** ncurses 24-bit RGB color via direct color mode and palette redefinition
**Confidence:** HIGH

## Summary

The ncurses library (6.1+, we have 6.6 ABI 6) supports two mechanisms for rendering exact RGB colors: **direct color mode** (via `xterm-direct` terminfo entries with the `RGB` flag) and **palette redefinition** (via `can_change_color()` + `init_extended_color()`). Both work through the existing `alloc_pair()` function the project already uses.

In direct color mode, `COLORS=16777216` and color indices are packed RGB values (0xRRGGBB) passed directly to `alloc_pair(fg, bg)`. In palette redefinition mode, `COLORS=256` but the palette slots can be reprogrammed to hold exact RGB values using `init_extended_color(slot, r, g, b)` with a 0-1000 scale. The existing 256-color nearest-match fallback (`rgb_to_nearest_256`) serves as the final fallback for terminals that support neither.

The key architectural change is that `TUI_Color` must store the original RGB values (not just a resolved index) so the color system can make mode-dependent decisions at `tui_style_apply()` time. Detection runs once at startup. The `wattr_set()` call must switch from `(short)pair, NULL` to `0, &pair` (int pointer via opts) to support pair numbers beyond 255 in direct color mode.

**Primary recommendation:** Implement a three-tier color resolution strategy -- direct color (best), palette redefinition (good), nearest-256 (fallback) -- with mode detection at startup and transparent resolution in `tui_style_apply()`.

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| ncurses | 6.6 (ABI 6) | Terminal graphics, color management | Already in use; ABI 6 provides extended pair support via `opts` parameter |
| ncurses `alloc_pair` | 6.1+ | Dynamic color pair allocation with LRU | Already used; takes `int` params -- works with both 256-color indices and packed RGB |
| ncurses `init_extended_color` | 6.1+ | Palette slot redefinition with int params | Overcomes `init_color`'s short limitation; allows redefining palette to exact RGB |
| ncurses `tigetflag("RGB")` | 6.0-20180121+ | Detect direct color support | Standard mechanism; returns 1 when terminal has RGB capability |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `COLORTERM` env var | N/A | Secondary truecolor detection hint | When `tigetflag("RGB")` returns -1 but user has set `COLORTERM=truecolor` |
| `can_change_color()` | POSIX curses | Detect palette redefinition support | Tier 2 fallback: reprogramming palette slots on 256-color terminals |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| ncurses color system | Raw ANSI escape sequences (bypass ncurses) | More control but loses ncurses compositing, panel support, all existing draw code |
| Direct color mode | Palette redefinition only (kakoune approach) | Simpler but limited to 256 simultaneous colors; we support both |
| `TERM` override to `xterm-direct` | Application-level TERM switching | Dangerous -- breaks terminal capabilities detection for other features |

**Installation:** No new dependencies. All functions are in ncursesw 6.6 already linked.

## Architecture Patterns

### Recommended Changes to TUI_Color

```
Current:
  TUI_Color { int index; }          /* -1=default, 0-255=xterm-256 */
  tui_color_rgb(r,g,b) -> resolves eagerly to nearest-256 index

New:
  TUI_Color { int index; }          /* -1=default, interpretation depends on mode */
  tui_color_rgb(r,g,b) -> behavior depends on detected color mode:
    - Direct color:   index = packed 0xRRGGBB
    - Palette redef:  index = allocated palette slot (16-255)
    - 256-color:      index = nearest_256 (existing behavior)
```

### Color Mode Enum

```c
typedef enum TUI_ColorMode {
    TUI_COLOR_MODE_256,        /* Default: nearest xterm-256 mapping */
    TUI_COLOR_MODE_PALETTE,    /* can_change_color: redefine palette slots */
    TUI_COLOR_MODE_DIRECT      /* RGB flag: packed 0xRRGGBB values */
} TUI_ColorMode;
```

### Detection Priority (at startup, after `start_color()`)

```c
// Source: verified on ncurses 6.6 (see test results below)
TUI_ColorMode detect_color_mode(void) {
    // 1. Check tigetflag("RGB") for direct color support
    if (tigetflag("RGB") == 1) {
        return TUI_COLOR_MODE_DIRECT;  // COLORS will be 16777216
    }

    // 2. Check COLORTERM env var as hint
    //    COLORTERM=truecolor means terminal supports 24-bit but TERM
    //    entry might not have RGB flag. We could override TERM, but
    //    that's risky. Instead, try palette redefinition.
    const char* ct = getenv("COLORTERM");
    // (COLORTERM alone doesn't change ncurses behavior -- verified)

    // 3. Check can_change_color() for palette redefinition
    if (can_change_color()) {
        return TUI_COLOR_MODE_PALETTE;
    }

    // 4. Final fallback: nearest 256-color mapping
    return TUI_COLOR_MODE_256;
}
```

### Pattern 1: tui_color_rgb -- Mode-Aware Color Creation

**What:** `tui_color_rgb()` checks the global color mode and resolves accordingly.
**When to use:** Every call site that creates a TUI_Color from RGB.
**Example:**
```c
// Source: synthesized from verified ncurses 6.6 behavior
static TUI_ColorMode g_color_mode = TUI_COLOR_MODE_256;

TUI_Color tui_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    switch (g_color_mode) {
    case TUI_COLOR_MODE_DIRECT:
        // Pack RGB into a single int -- ncurses treats this as color index
        return (TUI_Color){ .index = (r << 16) | (g << 8) | b };

    case TUI_COLOR_MODE_PALETTE:
        // Allocate a palette slot and redefine it to exact RGB
        return tui_color_palette_alloc(r, g, b);

    case TUI_COLOR_MODE_256:
    default:
        // Existing behavior: nearest xterm-256 index
        return (TUI_Color){ .index = rgb_to_nearest_256(r, g, b) };
    }
}
```

### Pattern 2: tui_style_apply -- Extended Pair via opts

**What:** `tui_style_apply()` uses `wattr_set` with the `opts` parameter for pair numbers > 255.
**When to use:** Always -- this is a unified approach that works for all modes.
**Example:**
```c
// Source: verified on ncurses 6.6 ABI 6 (test_opts.c)
void tui_style_apply(WINDOW* win, TUI_Style style) {
    int pair;
    if (style.fg.index == -1 && style.bg.index == -1) {
        pair = 0;
    } else {
        pair = alloc_pair(style.fg.index, style.bg.index);
    }
    attr_t attrs = tui_attrs_to_ncurses(style.attrs);
    // Use opts parameter (pointer to int) for extended pair support
    wattr_set(win, attrs, 0, &pair);
}
```

### Pattern 3: Palette Slot Allocator (for TUI_COLOR_MODE_PALETTE)

**What:** Manages the 240 usable palette slots (16-255) with LRU eviction.
**When to use:** Only in palette redefinition mode.
**Example:**
```c
// Source: synthesized from kakoune approach + ncurses 6.6 API
#define PALETTE_FIRST 16
#define PALETTE_LAST  255
#define PALETTE_SLOTS (PALETTE_LAST - PALETTE_FIRST + 1)  /* 240 */

typedef struct {
    uint8_t r, g, b;
    int slot;           /* palette index 16-255 */
    int lru_counter;    /* for eviction */
} PaletteEntry;

static PaletteEntry g_palette[PALETTE_SLOTS];
static int g_palette_count = 0;
static int g_lru_clock = 0;

static TUI_Color tui_color_palette_alloc(uint8_t r, uint8_t g, uint8_t b) {
    /* 1. Check if this exact RGB is already allocated */
    for (int i = 0; i < g_palette_count; i++) {
        if (g_palette[i].r == r && g_palette[i].g == g && g_palette[i].b == b) {
            g_palette[i].lru_counter = ++g_lru_clock;
            return (TUI_Color){ .index = g_palette[i].slot };
        }
    }

    /* 2. Allocate new slot or evict LRU */
    int target;
    if (g_palette_count < PALETTE_SLOTS) {
        target = g_palette_count++;
    } else {
        /* Find least recently used */
        target = 0;
        for (int i = 1; i < PALETTE_SLOTS; i++) {
            if (g_palette[i].lru_counter < g_palette[target].lru_counter)
                target = i;
        }
    }

    int slot = PALETTE_FIRST + target;
    /* init_extended_color uses 0-1000 scale */
    init_extended_color(slot, r * 1000 / 255, g * 1000 / 255, b * 1000 / 255);

    g_palette[target] = (PaletteEntry){
        .r = r, .g = g, .b = b,
        .slot = slot,
        .lru_counter = ++g_lru_clock
    };
    return (TUI_Color){ .index = slot };
}
```

### Anti-Patterns to Avoid

- **Overriding TERM at runtime:** Never `setenv("TERM", "xterm-direct", 1)` from within the app. This breaks all other terminfo-dependent behavior. The user or the TUI_Window config should control this.
- **Mixing direct color indices with palette indices:** In direct color mode, color index 196 means RGB(0,0,196), not xterm red. The two modes are mutually exclusive.
- **Using init_color instead of init_extended_color:** `init_color` uses `short` params, limited to 32767. `init_extended_color` uses `int`, future-proof.
- **Assuming COLORTERM changes ncurses behavior:** `COLORTERM=truecolor` does NOT make ncurses use direct color. Only the terminfo entry's `RGB` flag does that. COLORTERM is a hint for application-level decisions only.
- **Stomping palette slots 0-15:** These are the user's configured terminal colors (the ones from their colorscheme). Only redefine slots 16-255.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Color pair allocation | Manual pair number tracking | `alloc_pair(fg, bg)` | Built-in LRU eviction, works with both 256 and direct color, already used |
| RGB-to-256 nearest match | New algorithm | Existing `rgb_to_nearest_256()` | Already implemented and tested, used as final fallback |
| Terminal capability detection | Parsing escape sequences | `tigetflag("RGB")` + `can_change_color()` | Standard ncurses API, reliable |
| Extended pair application | Manual pair encoding in attrs | `wattr_set(win, attrs, 0, &pair)` | ABI 6 opts parameter handles everything |

**Key insight:** The ncurses 6.6 API already provides everything needed. No custom escape sequences, no ncurses bypass, no manual pair tables. The architecture change is purely in how `TUI_Color` resolves RGB values.

## Common Pitfalls

### Pitfall 1: The (short) Cast in wattr_set

**What goes wrong:** Current code uses `wattr_set(win, attrs, (short)pair, NULL)`. In direct color mode, `alloc_pair` can return pair numbers > 255 (up to 65535). Casting to `short` and passing NULL opts truncates the pair number.
**Why it happens:** The original code was designed for 256-color mode where pair numbers stay small.
**How to avoid:** Always use `wattr_set(win, attrs, 0, &pair)` where pair is an `int`. This works in all modes (verified on ncurses 6.6 ABI 6).
**Warning signs:** Colors look wrong on terminals with many pairs allocated.

### Pitfall 2: init_extended_color Uses 0-1000 Scale, Not 0-255

**What goes wrong:** Passing raw 0-255 RGB values to `init_extended_color()` produces washed-out colors (values only reach ~25% of full intensity).
**Why it happens:** ncurses color components use a 0-1000 scale per POSIX convention.
**How to avoid:** Convert with `r * 1000 / 255` (integer math, exact enough).
**Warning signs:** All colors appear much darker than expected in palette mode.

### Pitfall 3: Direct Color Index 0 Means Black, Not Default

**What goes wrong:** In direct color mode, `alloc_pair(0, 0)` means black-on-black, not default-on-default. The default terminal color is -1.
**Why it happens:** In 256-color mode, index 0 is also "black" but many terminals render pair 0 as the default colors. In direct color mode, 0 = 0x000000 = RGB black.
**How to avoid:** Always check for -1 (TUI_COLOR_DEFAULT) before calling `alloc_pair`. The current code already does this correctly.
**Warning signs:** Text disappears on default background.

### Pitfall 4: COLORTERM Does Not Activate Direct Color in ncurses

**What goes wrong:** Setting `COLORTERM=truecolor` alone does nothing to ncurses. `COLORS` stays at 256, `tigetflag("RGB")` stays -1.
**Why it happens:** ncurses reads terminfo entries based on `TERM`, not `COLORTERM`. `COLORTERM` is an informal convention.
**How to avoid:** Use `COLORTERM` only as a hint for the application to select a direct-color TERM entry (like `xterm-direct`) or to enable palette redefinition mode. Document this behavior.
**Warning signs:** User sets `COLORTERM=truecolor` but gets 256-color output.

### Pitfall 5: Palette Redefinition Is "Impolite"

**What goes wrong:** Redefining palette slots 0-15 overwrites the user's terminal colorscheme. After the app exits, the terminal may have wrong colors until reset.
**Why it happens:** `init_extended_color()` permanently changes the terminal's palette until `endwin()` restores it (which it does by default).
**How to avoid:** Only redefine slots 16-255. ncurses `endwin()` restores the original palette automatically on modern terminals.
**Warning signs:** Terminal colors are wrong after app crash (before endwin runs).

### Pitfall 6: INTERFACE Library and Static State

**What goes wrong:** The color mode detection state and palette allocator are `static` variables. In an INTERFACE library, each translation unit gets its own copy.
**Why it happens:** This is the same issue from Phase 1 -- INTERFACE library sources compile in the consumer's context.
**How to avoid:** All color mode state lives in `tui_color.c` (single file). Detection and the palette allocator are called from within that file only. Since `tui_color_rgb()` and `tui_style_apply()` are both in `tui_color.c`, the static variables are shared.
**Warning signs:** Different translation units see different color modes.

## Code Examples

### Example 1: Complete Detection and Initialization

```c
// Source: verified on ncurses 6.6 (tested 2026-02-20)

// Called once from tui_color_init() after start_color()
static TUI_ColorMode g_color_mode = TUI_COLOR_MODE_256;

void tui_color_init(void) {
    if (tigetflag("RGB") == 1) {
        g_color_mode = TUI_COLOR_MODE_DIRECT;
        return;
    }

    const char* colorterm = getenv("COLORTERM");
    if (colorterm) {
        if (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0) {
            /* Terminal claims truecolor but TERM lacks RGB flag.
             * Try palette redefinition as best effort. */
            if (can_change_color()) {
                g_color_mode = TUI_COLOR_MODE_PALETTE;
                return;
            }
        }
    }

    if (can_change_color()) {
        g_color_mode = TUI_COLOR_MODE_PALETTE;
        return;
    }

    g_color_mode = TUI_COLOR_MODE_256;
}
```

### Example 2: wattr_set with opts (Unified for All Modes)

```c
// Source: verified on ncurses 6.6 ABI 6 (test_opts.c, test_opts256.c)
void tui_style_apply(WINDOW* win, TUI_Style style) {
    int pair;
    if (style.fg.index == -1 && style.bg.index == -1) {
        pair = 0;
    } else {
        pair = alloc_pair(style.fg.index, style.bg.index);
    }
    attr_t attrs = tui_attrs_to_ncurses(style.attrs);
    wattr_set(win, attrs, 0, &pair);
}
```

### Example 3: Query Current Color Mode (New API)

```c
// New public API for developers to query/override color mode
TUI_ColorMode tui_color_get_mode(void);
const char* tui_color_mode_name(TUI_ColorMode mode);
```

## Verified Test Results

All tests run on ncurses 6.6 (ABI 6, NCURSES_EXT_FUNCS=20251230) on Linux 6.18.8-2-cachyos:

| Test | TERM | Result | Confirms |
|------|------|--------|----------|
| `tigetflag("RGB")` | xterm-direct | Returns 1 | Direct color detection works |
| `tigetflag("RGB")` | xterm-256color | Returns -1 | Not a boolean in 256-color |
| `COLORS` | xterm-direct | 16777216 | Full 24-bit color space |
| `COLORS` | xterm-256color | 256 | Standard palette |
| `COLOR_PAIRS` | xterm-direct | 65536 | Large pair table |
| `can_change_color()` | xterm-direct | 0 (false) | Direct color has no palette |
| `can_change_color()` | xterm-256color | 1 (true) | Palette redefinition works |
| `alloc_pair(0xFF0000, 0x000000)` | xterm-direct | Returns valid pair | Packed RGB works in alloc_pair |
| `alloc_pair(196, 0)` | xterm-256color | Returns valid pair | Standard indices still work |
| `wattr_set(win, attrs, 0, &pair)` | both | Returns 0 (OK) | opts pointer works in all modes |
| `wattr_set(win, attrs, (short)pair, NULL)` | xterm-256color | Returns 0 (OK) | Old approach still works for compat |
| `init_extended_color(16, 580, 0, 827)` | xterm-256color | Returns 0 (OK) | Palette redefinition works |
| 300 unique alloc_pair calls | xterm-direct | pair=299, opts works | Pairs > 255 correctly applied |
| `COLORTERM=truecolor` | xterm-256color | No effect on ncurses | COLORTERM is app-level hint only |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `init_pair(short pair, short fg, short bg)` | `alloc_pair(int fg, int bg)` | ncurses 6.1 (2018) | Already migrated in this project |
| `init_color(short, short, short, short)` | `init_extended_color(int, int, int, int)` | ncurses 6.1 (2018) | Use for palette redefinition |
| `wattr_set(win, attrs, (short)pair, NULL)` | `wattr_set(win, attrs, 0, &pair)` | ncurses ABI 6 | Required for pairs > 255 |
| `COLOR_PAIR(n)` macro for attrs | `wattr_set` with opts param | ncurses ABI 6 | COLOR_PAIR limited to 0-255 |
| Bypass ncurses for true color | Use ncurses direct color mode | ncurses 6.1 (2018) | Keeps ncurses panel compositing intact |

**Deprecated/outdated:**
- `init_pair` / `init_color`: Use `alloc_pair` and `init_extended_color` instead (short params limit colors to 32767)
- `attron` / `attroff`: Already replaced with `wattr_set` in this project
- `COLOR_PAIR(n)` macro: Limited to pairs 0-255, cannot encode extended pairs

## TUI_Window Configuration for Color Mode Override

The requirements specify that color mode can be overridden via TUI_Window configuration. This means adding a field to the `TUI_Window` struct:

```c
typedef struct TUI_Window {
    const char* title;
    const char* version;
    int fps;
    int width;
    int height;
    int color_mode;    /* 0=auto (default), 1=256, 2=palette, 3=direct */
} TUI_Window;
```

The override propagates through `Engine_Config` -> `tui_hook_startup()` -> `tui_color_init()`.

## Open Questions

1. **COLORTERM-based TERM override:**
   - What we know: When `COLORTERM=truecolor` is set but `TERM=xterm-256color`, the terminal supports true color but ncurses doesn't know it. Some apps (like vim) override `TERM` to a direct-color variant.
   - What's unclear: Should we auto-override TERM when COLORTERM hints at truecolor? This is invasive.
   - Recommendation: Do NOT auto-override TERM. Use `COLORTERM` to prefer palette redefinition mode. Document that users can set `TERM=xterm-direct` for full direct color. Allow override via TUI_Window config.

2. **Palette slot exhaustion in palette mode:**
   - What we know: Only 240 palette slots (16-255) are available. A complex UI could exhaust them.
   - What's unclear: How the LRU eviction interacts with ncurses' own pair LRU in `alloc_pair`.
   - Recommendation: The palette allocator evicts by re-defining the palette slot. Existing pairs using that slot will show the new color. This is acceptable since the evicted color wasn't recently used. If this becomes a problem, consider nearest-match fallback when palette is full.

3. **Interaction with sub-cell rendering (Phase 8):**
   - What we know: Half-block characters use fg for top half, bg for bottom half. True color makes this much more powerful.
   - What's unclear: No additional color API changes needed -- sub-cell rendering will use `TUI_Style` with fg/bg colors, which this phase makes true-color-capable.
   - Recommendation: No special handling needed. Phase 8 benefits automatically.

## Sources

### Primary (HIGH confidence)
- ncurses 6.6 installed on system, all APIs verified by compilation and runtime tests
- `infocmp -x xterm-direct` -- confirmed RGB flag, colors#0x1000000, pairs#0x10000
- Runtime tests on CachyOS Linux 6.18.8 with ncurses 6.6 ABI 6

### Secondary (MEDIUM confidence)
- [ncurses curs_color(3x) man page](https://invisible-island.net/ncurses/man/curs_color.3x.html) -- init_extended_color, init_extended_pair documentation
- [ncurses 6.1 announcement](https://dickey.his.com/ncurses/announce-6.1.html) -- direct color, alloc_pair, ABI 6 opts parameter
- [How should applications use direct-color mode?](https://lists.gnu.org/archive/html/bug-ncurses/2018-04/msg00011.html) -- Thomas Dickey's guidance on direct color workflow
- [More than 256 color pairs](https://reversed.top/2019-02-05/more-than-256-curses-color-pairs/) -- wattr_set opts usage
- [attr_set(3ncurses) man page](https://manpages.debian.org/testing/ncurses-doc/attr_set.3ncurses.en.html) -- ABI 6 opts as pointer to int
- [termstandard/colors](https://github.com/termstandard/colors) -- COLORTERM convention documentation
- [alloc_pair(3ncurses) man page](https://man7.org/linux/man-pages/man3/new_pair.3x.html) -- alloc_pair takes int params, LRU behavior

### Tertiary (LOW confidence)
- [Kakoune true color discussion](https://github.com/mawww/kakoune/issues/1807) -- palette redefinition approach (real-world validation)
- [Chad Austin: truecolor terminal emacs](https://chadaustin.me/2024/01/truecolor-terminal-emacs/) -- general truecolor landscape
- [init_extended_color usage discussion](https://lists.gnu.org/archive/html/bug-ncurses/2018-03/msg00015.html) -- Thomas Dickey on palette vs direct color

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all functions verified by compile-and-run on target system
- Architecture: HIGH -- three-tier approach (direct/palette/256) tested with real ncurses 6.6
- Detection: HIGH -- tigetflag("RGB"), can_change_color(), COLORTERM all tested
- wattr_set opts: HIGH -- verified that `wattr_set(win, attrs, 0, &pair)` works for pairs > 255
- Palette redefinition: HIGH -- init_extended_color verified on xterm-256color
- Pitfalls: HIGH -- each pitfall was discovered through actual testing or verified documentation

**Research date:** 2026-02-20
**Valid until:** 2026-06-20 (ncurses API is very stable; 6-month window)
