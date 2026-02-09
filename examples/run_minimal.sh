#!/bin/sh
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$REPO_ROOT" && cmake --build build --target minimal && kitty --title "CELS Minimal" "$REPO_ROOT/build/modules/cels-ncurses/minimal"
