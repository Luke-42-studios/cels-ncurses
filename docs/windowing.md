# Window

The window entity represents the terminal. Create one with the `NCursesWindow` composition and NCurses handles initialization, frame timing, resize detection, and shutdown.

## Creating a Window

```c
CEL_Compose(World) {
    NCursesWindow(.title = "My App", .fps = 60) {}
}
```

`NCursesWindow` creates an entity with `NCurses_WindowConfig`:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `title` | `const char*` | `NULL` | Window title (terminal emulator title bar) |
| `fps` | `int` | `0` | Target FPS (0 = uncapped) |
| `color_mode` | `int` | `0` | 0=auto, 1=256-color, 2=palette-redef, 3=direct-RGB |

Only one window entity should exist at a time.

## Window State

After initialization, NCurses attaches `NCurses_WindowState` to the window entity and updates it every frame. Read it in systems with `cel_read`:

```c
CEL_System(MySystem, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_WindowState* ws = cel_read(NCurses_WindowState);
        if (!ws) return;

        int w = ws->width;       // terminal columns
        int h = ws->height;      // terminal rows
        float fps = ws->actual_fps;
        float dt = ws->delta_time;
    }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `width` | `int` | Terminal width in columns |
| `height` | `int` | Terminal height in rows |
| `running` | `bool` | `false` after quit requested |
| `actual_fps` | `float` | Measured FPS |
| `delta_time` | `float` | Seconds since last frame |

## Reactive Resize

Use `cel_watch` in a composition to trigger recomposition when the terminal is resized:

```c
CEL_Compose(World) {
    NCursesWindow(.title = "Resize Demo", .fps = 30) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        // This block re-runs when terminal dimensions change.
        // Surfaces declared here will be recreated at new sizes.
        TUISurface(.z_order = 0, .visible = true) {}
    }
}
```

`cel_watch` returns `NULL` until the state is first written. Always null-check before using.

## Session Loop

The standard application loop:

```c
cels_main() {
    cels_register(NCurses);
    cels_register(Renderer, GameInput);

    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
```

- `cels_register(NCurses)` — registers all NCurses components, systems, and lifecycles
- `cels_session(World)` — enters the session, composes the root, runs until exit
- `cels_running()` — returns `false` after `cels_request_quit()`
- `cels_step(0)` — advances one frame (0 = use module FPS timing)

## Complete Example

```c
#include <cels/cels.h>
#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>

CEL_Compose(World) {
    NCursesWindow(.title = "Window Demo", .fps = 30) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        TUISurface(.z_order = 0, .visible = true) {}
    }
}

CEL_System(ShowInfo, .phase = OnRender) {
    cel_query(TUI_SurfaceConfig, TUI_DrawContext_Component);
    cel_each(TUI_SurfaceConfig, TUI_DrawContext_Component) {
        TUI_DrawContext ctx = TUI_DrawContext_Component->ctx;
        const struct NCurses_WindowState* ws = cel_read(NCurses_WindowState);
        if (!ws) return;

        TUI_Style s = { .fg = tui_color_rgb(255, 255, 255) };

        char buf[64];
        snprintf(buf, sizeof(buf), "Terminal: %dx%d  FPS: %.0f",
                 ws->width, ws->height, ws->actual_fps);
        tui_draw_text(&ctx, 1, 1, buf, s);
        tui_draw_text(&ctx, 1, 2, "Resize the terminal. Press 'q' to quit.", s);
    }
}

CEL_System(Quit, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_InputState* input = cel_read(NCurses_InputState);
        if (!input) return;
        for (int i = 0; i < input->key_count; i++) {
            if (input->keys[i] == 'q') cels_request_quit();
        }
    }
}

cels_main() {
    cels_register(NCurses);
    cels_register(ShowInfo, Quit);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
```
