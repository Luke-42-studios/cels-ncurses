/*
 * Space Invaders - ECS Component Definitions
 *
 * Atomic components for the Space Invaders game.
 * All game entities are composed from these building blocks.
 */

#ifndef SPACE_INVADERS_COMPONENTS_H
#define SPACE_INVADERS_COMPONENTS_H

#include <cels/cels.h>
#include <stdbool.h>

/* ============================================================================
 * Game Enums
 * ============================================================================ */

typedef enum GameScreen {
    SCREEN_TITLE = 0,
    SCREEN_PLAYING,
    SCREEN_WAVE_CLEAR,
    SCREEN_GAMEOVER
} GameScreen;

/* ============================================================================
 * Atomic Components
 * ============================================================================ */

CEL_Define(Position, {
    float x;
    float y;
});

CEL_Define(Velocity, {
    float dx;
    float dy;
});

CEL_Define(Sprite, {
    char ch[4];     /* 1-3 char sprite + NUL */
    int style_id;   /* index into style palette */
});

CEL_Define(PlayerTag, {
    float shoot_cooldown;
});

CEL_Define(EnemyTag, {
    int type;       /* row-based type 0-4 */
    int points;     /* score value */
    int col;        /* grid column */
    int row;        /* grid row */
});

CEL_Define(BulletTag, {
    bool from_player;
    bool active;
});

CEL_Define(ShieldBlock, {
    int health;     /* 1-3, degrades on hit */
});

/* Render provider marker component */
CEL_Define(GameCanvas, {
    int _unused;
});

/* ============================================================================
 * Play Area Constants (shared between renderer and game)
 * ============================================================================ */

#define PLAY_X 1
#define PLAY_Y 2
#define PLAY_W 160
#define PLAY_H 30
#define HUD_Y 1

/* ============================================================================
 * Game State (shared between renderer and game systems)
 * ============================================================================ */

CEL_State(SI_GameState, {
    int score;
    int lives;
    int wave;
    int enemies_alive;
    float enemy_speed;
    int enemy_dir;          /* +1 right, -1 left */
    bool enemy_should_drop;
    float enemy_shoot_timer;
    float player_bullet_active;
    GameScreen screen;
});

#endif /* SPACE_INVADERS_COMPONENTS_H */
