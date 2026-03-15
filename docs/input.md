# Input

NCurses reads raw keyboard and mouse input each frame at the `OnLoad` phase. The developer reads input state in their own systems — no callbacks, no event queues. Just query the state.

## Reading Input State

```c
CEL_System(HandleInput, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_InputState* input = cel_read(NCurses_InputState);
        if (!input) return;

        // input->keys[], input->mouse_x, etc.
    }
}
```

`cel_read(NCurses_InputState)` returns a pointer to the current frame's input. Per-frame fields reset each frame. Persistent fields (mouse position, held state) carry over.

## Fields

| Field | Type | Lifetime | Description |
|-------|------|----------|-------------|
| `keys[16]` | `int` | per-frame | Raw ncurses key codes pressed this frame |
| `key_count` | `int` | per-frame | Number of keys in the array |
| `mouse_x` | `int` | persistent | Mouse column (-1 before first event) |
| `mouse_y` | `int` | persistent | Mouse row (-1 before first event) |
| `mouse_button` | `int` | per-frame | 0=none, 1=left, 2=middle, 3=right |
| `mouse_pressed` | `bool` | per-frame | Button pressed this frame |
| `mouse_released` | `bool` | per-frame | Button released this frame |
| `mouse_left_held` | `bool` | persistent | Left button currently held |
| `mouse_middle_held` | `bool` | persistent | Middle button currently held |
| `mouse_right_held` | `bool` | persistent | Right button currently held |

## Keyboard

Keys are raw ncurses codes. Loop over the `keys` array to check what was pressed:

```c
for (int i = 0; i < input->key_count; i++) {
    switch (input->keys[i]) {
        case 'q':     /* quit */         break;
        case '\n':    /* enter */        break;
        case KEY_UP:  /* arrow up */     break;
        case KEY_F(2): /* F2 */          break;
    }
}
```

Up to 16 keys per frame. If the user mashes the keyboard, excess keys are dropped.

## Mouse

Mouse position persists across frames. Button events are per-frame:

```c
// Click detection
if (input->mouse_pressed && input->mouse_button == 1) {
    // Left click at (input->mouse_x, input->mouse_y)
}

// Drag detection
if (input->mouse_left_held) {
    // Left button held, mouse at (input->mouse_x, input->mouse_y)
}
```

## Special Keys

| Key | Behavior |
|-----|----------|
| `KEY_RESIZE` | Consumed internally — resize handled by `NCurses_WindowUpdateSystem` + `TUI_SurfaceSystem` |
| `KEY_F(1)` | Toggles pause mode (freezes frame loop for text selection/copy) |
| `CELS_KEY_CTRL_UP/DOWN/LEFT/RIGHT` (600-603) | Ctrl+Arrow keys |
| `CELS_KEY_SHIFT_LEFT/RIGHT` (604-605) | Shift+Arrow keys |

The Ctrl/Shift arrow codes are custom — defined by cels-ncurses for xterm-compatible terminals.

## Quit Pattern

The standard quit pattern checks for 'q' or 'Q':

```c
CEL_System(GameInput, .phase = OnUpdate) {
    cel_run {
        const struct NCurses_InputState* input = cel_read(NCurses_InputState);
        if (!input) return;
        for (int i = 0; i < input->key_count; i++) {
            if (input->keys[i] == 'q' || input->keys[i] == 'Q') {
                cels_request_quit();
                return;
            }
        }
    }
}
```

`cels_request_quit()` sets the session running flag to false. `cels_running()` returns false on the next iteration.
