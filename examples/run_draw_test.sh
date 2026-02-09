#!/bin/sh
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$REPO_ROOT" && cmake --build build --target draw_test && kitty --title "Draw Test" "$REPO_ROOT/build/modules/cels-ncurses/draw_test"
