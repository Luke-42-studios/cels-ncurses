<p align="center">
  <h1 align="center">cels-ncurses</h1>
  <p align="center"><strong>Terminal rendering module for CELS.</strong></p>
  <p align="center">
    <img src="https://img.shields.io/badge/version-v0.2.0-blue" alt="version">
    <img src="https://img.shields.io/badge/license-Apache%202.0-green" alt="license">
    <img src="https://img.shields.io/badge/C99-orange?logo=c" alt="C99">
  </p>
</p>

Create window and surface entities, draw with primitives, read input state. NCurses manages the terminal, panels, and frame pipeline through CELS systems. You describe structure in compositions and behavior in systems — the module handles everything else.

```c
#include <cels/cels.h>
#include <cels_ncurses.h>
#include <cels_ncurses_draw.h>

CEL_Compose(World) {
    NCursesWindow(.title = "Hello", .fps = 30) {
        const struct NCurses_WindowState* ws = cel_watch(NCurses_WindowState);
        if (!ws) return;

        TUISurface(.z_order = 0, .visible = true) {}
    }
}

CEL_System(Renderer, .phase = OnRender) {
    cel_query(TUI_SurfaceConfig, TUI_DrawContext_Component);
    cel_each(TUI_SurfaceConfig, TUI_DrawContext_Component) {
        TUI_DrawContext ctx = TUI_DrawContext_Component->ctx;
        TUI_Style style = {
            .fg = tui_color_rgb(255, 255, 255),
            .bg = tui_color_rgb(0, 0, 0),
            .attrs = TUI_ATTR_BOLD
        };
        tui_draw_text(&ctx, 2, 1, "Hello from cels-ncurses!", style);
    }
}

CEL_System(Input, .phase = OnUpdate) {
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
    cels_register(Renderer, Input);
    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
```

## Concepts

| Concept | What it does |
|---------|-------------|
| `NCursesWindow` | Composition that creates the terminal window entity |
| `TUISurface` | Composition that creates a drawable surface (panel-backed, z-ordered) |
| `TUI_DrawContext` | Drawing handle — pass to `tui_draw_*` functions |
| `NCurses_InputState` | Per-frame keyboard and mouse state singleton |
| `NCurses_WindowState` | Terminal dimensions, FPS, delta time |

The API has two patterns:

```
CEL_Compose  = WHAT exists    (window, surfaces)
CEL_System   = WHAT HAPPENS   (drawing, input handling)
```

## Quick Start

```cmake
include(FetchContent)
FetchContent_Declare(cels
    GIT_REPOSITORY https://github.com/42-Galaxies/cels.git
    GIT_TAG        v0.5.3
)
FetchContent_Declare(cels-ncurses
    GIT_REPOSITORY https://github.com/42-Galaxies/cels-ncurses.git
    GIT_TAG        v0.2.0
)
FetchContent_MakeAvailable(cels cels-ncurses)

target_link_libraries(your_app PRIVATE cels cels-ncurses)
```

```bash
cmake -B build && cmake --build build && ./build/your_app
```

## Documentation

- [**Windowing**](docs/windowing.md) — Terminal window lifecycle and reactive state
- [**Surfaces**](docs/surfaces.md) — Drawable areas, z-ordering, and draw primitives
- [**Input**](docs/input.md) — Keyboard and mouse input each frame
- [**Frame Pipeline**](docs/frame-pipeline.md) — Pipeline phases and where your code runs

## License

[Apache 2.0](LICENSE)
