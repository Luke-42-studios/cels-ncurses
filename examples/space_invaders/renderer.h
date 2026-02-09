/*
 * Space Invaders - Render Provider
 *
 * Single render callback registered via _CEL_Provides() at CELS_Phase_OnStore.
 * Draws all game entities by iterating Position + Sprite via ecs_each_id().
 * Also draws HUD, title screen, and game-over screen.
 *
 * All drawing goes through TUI_DrawContext from the background layer.
 * No stdscr writes.
 */

#ifndef SPACE_INVADERS_RENDERER_H
#define SPACE_INVADERS_RENDERER_H

#include <cels/cels.h>
#include <cels-ncurses/tui_frame.h>
#include <cels-ncurses/tui_layer.h>
#include <cels-ncurses/tui_draw.h>
#include <cels-ncurses/tui_scissor.h>
#include <cels-ncurses/tui_window.h>
#include <flecs.h>
#include <string.h>
#include <stdio.h>
#include "components.h"

/* ============================================================================
 * Feature Definition
 * ============================================================================ */

_CEL_DefineFeature(GameRenderable, .phase = CELS_Phase_OnStore, .priority = 0);

/* ============================================================================
 * Style Palette
 * ============================================================================ */

/* Style IDs for Sprite.style_id */
#define STYLE_PLAYER     0
#define STYLE_ENEMY_0    1
#define STYLE_ENEMY_1    2
#define STYLE_ENEMY_2    3
#define STYLE_BULLET     4
#define STYLE_SHIELD     5
#define STYLE_COUNT      6

static TUI_Style g_styles[STYLE_COUNT];
static TUI_Style g_bg_style;
static TUI_Style g_hud_style;
static TUI_Style g_title_style;
static TUI_Style g_dim_style;
static TUI_Style g_gameover_style;
static TUI_Style g_border_style;
static TUI_Style g_score_style;

static inline void si_styles_init(void) {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    TUI_Color bg_c = tui_color_rgb(8, 8, 18);

    g_bg_style = (TUI_Style){
        .fg = tui_color_rgb(255, 255, 255), .bg = bg_c, .attrs = TUI_ATTR_NORMAL
    };

    /* Player: cyan */
    g_styles[STYLE_PLAYER] = (TUI_Style){
        .fg = tui_color_rgb(0, 255, 255), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    /* Enemy row 0-1: green */
    g_styles[STYLE_ENEMY_0] = (TUI_Style){
        .fg = tui_color_rgb(0, 220, 80), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    /* Enemy row 2-3: yellow */
    g_styles[STYLE_ENEMY_1] = (TUI_Style){
        .fg = tui_color_rgb(230, 200, 50), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    /* Enemy row 4: red */
    g_styles[STYLE_ENEMY_2] = (TUI_Style){
        .fg = tui_color_rgb(220, 50, 50), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    /* Bullets: white */
    g_styles[STYLE_BULLET] = (TUI_Style){
        .fg = tui_color_rgb(255, 255, 255), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    /* Shields: blue */
    g_styles[STYLE_SHIELD] = (TUI_Style){
        .fg = tui_color_rgb(60, 120, 220), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };

    g_hud_style = (TUI_Style){
        .fg = tui_color_rgb(200, 200, 200), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    g_title_style = (TUI_Style){
        .fg = tui_color_rgb(0, 255, 255), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    g_dim_style = (TUI_Style){
        .fg = tui_color_rgb(120, 120, 120), .bg = bg_c, .attrs = TUI_ATTR_DIM
    };
    g_gameover_style = (TUI_Style){
        .fg = tui_color_rgb(220, 50, 50), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
    g_border_style = (TUI_Style){
        .fg = tui_color_rgb(60, 60, 100), .bg = bg_c, .attrs = TUI_ATTR_NORMAL
    };
    g_score_style = (TUI_Style){
        .fg = tui_color_rgb(255, 255, 0), .bg = bg_c, .attrs = TUI_ATTR_BOLD
    };
}

/* SI_GameState and play area constants are defined in components.h */

/* ============================================================================
 * Render Callback
 * ============================================================================ */

static void si_render_screen(CELS_Iter* it) {
    (void)it;
    si_styles_init();

    TUI_Layer* bg = tui_frame_get_background();
    if (!bg) return;
    TUI_DrawContext ctx = tui_layer_get_draw_context(bg);
    tui_scissor_reset(&ctx);

    int cols = bg->width;
    int rows = bg->height;

    /* Fill background */
    tui_draw_fill_rect(&ctx, (TUI_CellRect){0, 0, cols, rows}, ' ', g_bg_style);

    if (SI_GameState.screen == SCREEN_TITLE) {
        /* Title screen */
        const char* title = "SPACE INVADERS";
        int tlen = (int)strlen(title);
        int cx = (cols - tlen) / 2;
        int cy = rows / 2 - 4;
        tui_draw_text(&ctx, cx, cy, title, g_title_style);

        const char* sub = "A CELS + ncurses demo";
        int slen = (int)strlen(sub);
        tui_draw_text(&ctx, (cols - slen) / 2, cy + 2, sub, g_dim_style);

        const char* prompt = "Press ENTER to start";
        int plen = (int)strlen(prompt);
        tui_draw_text(&ctx, (cols - plen) / 2, cy + 5, prompt, g_hud_style);

        const char* quit = "Q to quit";
        int qlen = (int)strlen(quit);
        tui_draw_text(&ctx, (cols - qlen) / 2, cy + 7, quit, g_dim_style);
        return;
    }

    if (SI_GameState.screen == SCREEN_GAMEOVER) {
        const char* go = "GAME OVER";
        int glen = (int)strlen(go);
        int cx = (cols - glen) / 2;
        int cy = rows / 2 - 3;
        tui_draw_text(&ctx, cx, cy, go, g_gameover_style);

        char score_buf[64];
        snprintf(score_buf, sizeof(score_buf), "Final Score: %d", SI_GameState.score);
        int slen = (int)strlen(score_buf);
        tui_draw_text(&ctx, (cols - slen) / 2, cy + 2, score_buf, g_score_style);

        char wave_buf[64];
        snprintf(wave_buf, sizeof(wave_buf), "Wave reached: %d", SI_GameState.wave);
        int wlen = (int)strlen(wave_buf);
        tui_draw_text(&ctx, (cols - wlen) / 2, cy + 3, wave_buf, g_hud_style);

        const char* prompt = "Press ENTER to restart";
        int plen = (int)strlen(prompt);
        tui_draw_text(&ctx, (cols - plen) / 2, cy + 6, prompt, g_hud_style);

        const char* quit = "Q to quit";
        int qlen = (int)strlen(quit);
        tui_draw_text(&ctx, (cols - qlen) / 2, cy + 8, quit, g_dim_style);
        return;
    }

    /* Playing screen */

    /* Draw bordered play area */
    int play_w = PLAY_W;
    int play_h = PLAY_H;
    if (play_w > cols - 2) play_w = cols - 2;
    if (play_h > rows - 3) play_h = rows - 3;

    tui_draw_border_rect(&ctx,
        (TUI_CellRect){PLAY_X - 1, PLAY_Y - 1, play_w + 2, play_h + 2},
        TUI_BORDER_DOUBLE, g_border_style);

    /* HUD line */
    char hud_buf[128];
    snprintf(hud_buf, sizeof(hud_buf),
        " SCORE: %06d  |  LIVES: %d  |  WAVE: %d  |  ENEMIES: %d ",
        SI_GameState.score, SI_GameState.lives,
        SI_GameState.wave, SI_GameState.enemies_alive);
    tui_draw_text(&ctx, PLAY_X, HUD_Y - 1, hud_buf, g_hud_style);

    /* Clip drawing to play area */
    tui_push_scissor(&ctx, (TUI_CellRect){PLAY_X, PLAY_Y, play_w, play_h});

    /* Iterate all entities with Position + Sprite and render them */
    CELS_Context* cctx = cels_get_context();
    ecs_world_t* world = cels_get_world(cctx);
    if (!world) { tui_pop_scissor(&ctx); return; }

    if (PositionID != 0 && SpriteID != 0) {
        ecs_iter_t eit = ecs_each_id(world, PositionID);
        while (ecs_each_next(&eit)) {
            Position* positions = ecs_field(&eit, Position, 0);
            for (int i = 0; i < eit.count; i++) {
                ecs_entity_t e = eit.entities[i];
                if (!ecs_has_id(world, e, SpriteID)) continue;
                const Sprite* spr = ecs_get_id(world, e, SpriteID);
                if (!spr) continue;

                /* Skip inactive pooled bullets */
                if (BulletTagID != 0 && ecs_has_id(world, e, BulletTagID)) {
                    const BulletTag* bt = ecs_get_id(world, e, BulletTagID);
                    if (bt && !bt->active) continue;
                }

                int sx = PLAY_X + (int)positions[i].x;
                int sy = PLAY_Y + (int)positions[i].y;

                /* Select style based on style_id */
                TUI_Style st = g_bg_style;
                if (spr->style_id >= 0 && spr->style_id < STYLE_COUNT) {
                    st = g_styles[spr->style_id];
                }

                /* For shields, dim based on health */
                if (ShieldBlockID != 0 && ecs_has_id(world, e, ShieldBlockID)) {
                    const ShieldBlock* sb = ecs_get_id(world, e, ShieldBlockID);
                    if (sb && sb->health <= 1) {
                        st.attrs = TUI_ATTR_DIM;
                    }
                }

                tui_draw_text(&ctx, sx, sy, spr->ch, st);
            }
        }
    }

    tui_pop_scissor(&ctx);
}

/* ============================================================================
 * Renderer Initialization
 * ============================================================================ */

static inline void si_renderer_init(void) {
    _CEL_Feature(GameCanvas, GameRenderable);
    _CEL_Provides(TUI, GameRenderable, GameCanvas, si_render_screen);
    _CEL_ProviderConsumes(Position, Sprite, PlayerTag, EnemyTag, BulletTag, ShieldBlock);
}

#endif /* SPACE_INVADERS_RENDERER_H */
