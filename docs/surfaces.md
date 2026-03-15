# Surfaces

A surface is a drawable rectangular area backed by an ncurses panel. Surfaces composite by z-order — higher values draw on top. Create surfaces in compositions, draw into them in systems.

## Creating Surfaces

```c
CEL_Compose(World) {
    NCursesWindow(.title = "Demo", .fps = 30) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        TUISurface(.z_order = 0, .visible = true) {}
        TUISurface(.z_order = 10, .visible = true,
                   .x = 5, .y = 3, .width = 40, .height = 12) {}
    }
}
```

`TUISurface` creates an entity with `TUI_SurfaceConfig`:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `z_order` | `int` | `0` | Stacking order (higher = on top) |
| `visible` | `bool` | `false` | Must be `true` to draw and display |
| `x`, `y` | `int` | `0, 0` | Position in screen coordinates |
| `width`, `height` | `int` | `0, 0` | Dimensions (0 = fullscreen) |

**Important:** `.visible` defaults to `false` (C99 zero-init). Always pass `.visible = true` explicitly.

## Drawing

NCurses attaches a `TUI_DrawContext_Component` to each surface entity after creation. Query it in a system at `OnRender`:

```c
CEL_System(Renderer, .phase = OnRender) {
    cel_query(TUI_SurfaceConfig, TUI_DrawContext_Component);
    cel_each(TUI_SurfaceConfig, TUI_DrawContext_Component) {
        TUI_DrawContext ctx = TUI_DrawContext_Component->ctx;

        TUI_Style style = {
            .fg = tui_color_rgb(255, 255, 255),
            .bg = tui_color_rgb(30, 30, 60),
            .attrs = TUI_ATTR_BOLD
        };

        tui_draw_text(&ctx, 2, 1, "Hello!", style);
        tui_draw_border_rect(&ctx,
            (TUI_CellRect){0, 0, ctx.width, ctx.height},
            TUI_BORDER_SINGLE, style);
    }
}
```

### Styles

Every draw call takes a `TUI_Style`:

```c
TUI_Style style = {
    .fg = tui_color_rgb(r, g, b),    // foreground color
    .bg = tui_color_rgb(r, g, b),    // background color
    .attrs = TUI_ATTR_BOLD | TUI_ATTR_UNDERLINE
};
```

Attributes: `TUI_ATTR_NORMAL`, `TUI_ATTR_BOLD`, `TUI_ATTR_DIM`, `TUI_ATTR_UNDERLINE`, `TUI_ATTR_REVERSE`, `TUI_ATTR_BLINK`.

### Draw Primitives

| Function | Description |
|----------|-------------|
| `tui_draw_text(&ctx, x, y, str, style)` | Draw a string |
| `tui_draw_fill_rect(&ctx, rect, ch, style)` | Fill a rectangle with a character |
| `tui_draw_border_rect(&ctx, rect, border, style)` | Draw a bordered rectangle |
| `tui_draw_hline(&ctx, x, y, len, ch, style)` | Horizontal line |
| `tui_draw_vline(&ctx, x, y, len, ch, style)` | Vertical line |
| `tui_draw_char(&ctx, x, y, ch, style)` | Single character |

Border types: `TUI_BORDER_SINGLE`, `TUI_BORDER_DOUBLE`, `TUI_BORDER_ROUNDED`, `TUI_BORDER_HEAVY`, `TUI_BORDER_NONE`.

Rectangles use `TUI_CellRect`:

```c
(TUI_CellRect){.x = 0, .y = 0, .w = ctx.width, .h = ctx.height}
```

## Z-Order and Compositing

Surfaces stack by `z_order`. The frame pipeline sorts panels and calls `update_panels()` + `doupdate()` at `PostRender`. You do not need to manage compositing yourself.

```c
TUISurface(.z_order = 0, .visible = true) {}   // background
TUISurface(.z_order = 10, .visible = true) {}   // middle
TUISurface(.z_order = 100, .visible = true) {}  // foreground
```

Gaps in z-order values are fine and encouraged — leave room for future insertions.

## Frame Pipeline

Surfaces are cleared automatically each frame at `PreRender`. You draw at `OnRender`. Compositing happens at `PostRender`. This means:

1. You get a blank canvas every frame
2. Draw everything you need — no need to clear manually
3. Panels are composited and flushed to the terminal automatically

See [Frame Pipeline](frame-pipeline.md) for the full phase breakdown.

## Identifying Surfaces

Use `TUI_SurfaceConfig->z_order` or position to distinguish surfaces in your render system:

```c
cel_each(TUI_SurfaceConfig, TUI_DrawContext_Component) {
    TUI_DrawContext ctx = TUI_DrawContext_Component->ctx;

    if (TUI_SurfaceConfig->z_order == 0) {
        // Background surface
        draw_background(&ctx);
    } else if (TUI_SurfaceConfig->z_order == 10) {
        // Overlay surface
        draw_overlay(&ctx);
    }
}
```
