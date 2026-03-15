# Frame Pipeline

NCurses manages the frame lifecycle through CELS pipeline phases. Developer systems slot in naturally — no explicit ordering code needed.

## Pipeline Phases

Each frame executes in this order:

```
OnLoad       NCurses_InputSystem        reads getch() queue into NCurses_InputState
     │
OnUpdate     NCurses_WindowUpdateSystem updates timing, detects resize, checks quit
             ▸ Developer logic systems   game state, AI, physics
     │
PreRender    TUI_SurfaceSystem          clears surfaces, syncs visibility, rebuilds z-order
             TUI_FrameBeginSystem       blocks SIGWINCH
     │
OnRender     ▸ Developer draw systems   query surfaces, draw with tui_draw_*
     │
PostRender   TUI_FrameEndSystem         update_panels + doupdate, unblock SIGWINCH, FPS throttle
```

Phases marked with `▸` are where your systems run.

## Where Developer Code Goes

**Logic** at `OnUpdate` — input handling, game state, quit checks:

```c
CEL_System(GameLogic, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_InputState* input = cel_read(NCurses_InputState);
        if (!input) return;
        for (int i = 0; i < input->key_count; i++) {
            if (input->keys[i] == 'q') cels_request_quit();
        }
    }
}
```

**Drawing** at `OnRender` — query surfaces, draw into them:

```c
CEL_System(Renderer, .phase = OnRender) {
    cel_query(TUI_SurfaceConfig, TUI_DrawContext_Component);
    cel_each(TUI_SurfaceConfig, TUI_DrawContext_Component) {
        TUI_DrawContext ctx = TUI_DrawContext_Component->ctx;
        TUI_Style s = { .fg = tui_color_rgb(255, 255, 255) };
        tui_draw_text(&ctx, 1, 1, "Drawn at OnRender", s);
    }
}
```

You never need to call `update_panels()`, `doupdate()`, or `werase()`. The frame pipeline handles all of that.

## Frame Timing

Set target FPS in the window config:

```c
NCursesWindow(.title = "My App", .fps = 60) {}
```

Read actual timing from window state:

```c
const struct NCurses_WindowState* ws = cel_read(NCurses_WindowState);
float dt = ws->delta_time;     // seconds since last frame
float fps = ws->actual_fps;    // measured FPS
```

Pass `0` to `cels_step()` to use the module's FPS throttle (handled in `PostRender`). The throttle sleeps for the remaining frame budget after `doupdate()` completes.

## SIGWINCH Handling

Terminal resize (`SIGWINCH`) is blocked during the render phases to prevent partial draws:

1. `PreRender` — `sigprocmask(SIG_BLOCK)` blocks SIGWINCH
2. `OnRender` — your draw code runs with stable terminal dimensions
3. `PostRender` — `sigprocmask(SIG_UNBLOCK)` allows the signal through

If a resize happened while blocked, it is detected on the next frame by `NCurses_WindowUpdateSystem`. Fullscreen surfaces are automatically resized by `TUI_SurfaceSystem`. The `cel_watch(NCurses_WindowState)` in your composition triggers recomposition with the new dimensions.

## System Registration

Register your systems alongside NCurses:

```c
cels_main() {
    cels_register(NCurses);              // all NCurses systems
    cels_register(Renderer, GameLogic);  // your systems

    cels_session(World) {
        while (cels_running()) cels_step(0);
    }
}
```

CELS pipeline phases handle ordering. Your `OnUpdate` systems run after input is read. Your `OnRender` systems run after surfaces are cleared and before compositing.
