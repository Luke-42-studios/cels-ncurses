#!/bin/sh
REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$REPO_ROOT" && cmake --build build --target space_invaders && \
    kitty --title "Space Invaders" "$REPO_ROOT/build/space_invaders"
