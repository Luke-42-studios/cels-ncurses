/*
 * Space Invaders - Main Game
 *
 * A Space Invaders clone built with the CELS ECS framework and cels-ncurses
 * drawing API. Demonstrates real ECS game architecture using systems,
 * compositions, state, and the render provider pattern.
 *
 * Controls:
 *   Arrow keys / A,D  - Move player left/right
 *   Space / Enter      - Fire bullet
 *   Q                  - Quit
 */

#include "components.h"
#include "renderer.h"
#include <cels-ncurses/tui_engine.h>
#include <flecs.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ============================================================================
 * Game Constants
 * ============================================================================ */

#define ENEMY_COLS 11
#define ENEMY_ROWS 5
#define ENEMY_TOTAL (ENEMY_COLS * ENEMY_ROWS)
#define ENEMY_SPACING_X 4
#define ENEMY_SPACING_Y 2
#define ENEMY_START_X 4
#define ENEMY_START_Y 2

#define PLAYER_Y (PLAY_H - 2)
#define PLAYER_SPEED 50.0f
#define BULLET_SPEED 25.0f
#define ENEMY_BULLET_SPEED 12.0f

#define SHIELD_COUNT 4
#define SHIELD_BLOCKS_PER 6
#define SHIELD_Y (PLAY_H - 6)

#define SHOOT_COOLDOWN 0.3f
#define ENEMY_SHOOT_INTERVAL 1.2f

#define BASE_ENEMY_SPEED 3.0f
#define SPEED_PER_WAVE 0.8f
#define DROP_AMOUNT 1

/* SI_GameState defined in components.h */

/* ============================================================================
 * Lifecycles
 * ============================================================================ */

CEL_Observer(TitleVisible, SI_GameState) {
    return SI_GameState.screen == SCREEN_TITLE;
}

CEL_Observer(PlayingVisible, SI_GameState) {
    return SI_GameState.screen == SCREEN_PLAYING;
}

CEL_Observer(PlayingInactive, SI_GameState) {
    return SI_GameState.screen != SCREEN_PLAYING;
}

CEL_Observer(GameOverVisible, SI_GameState) {
    return SI_GameState.screen == SCREEN_GAMEOVER;
}

CEL_Observer(WaveClearVisible, SI_GameState) {
    return SI_GameState.screen == SCREEN_WAVE_CLEAR;
}

CEL_Lifecycle(TitleLC, .systemVisibility = &TitleVisible);
CEL_Lifecycle(PlayingLC, .systemVisibility = &PlayingVisible, .destroy = &PlayingInactive);
CEL_Lifecycle(GameOverLC, .systemVisibility = &GameOverVisible);
CEL_Lifecycle(WaveClearLC, .systemVisibility = &WaveClearVisible);

/* ============================================================================
 * Simple RNG (seeded once)
 * ============================================================================ */

static unsigned int g_rng_state = 0;

static void rng_seed(void) {
    g_rng_state = (unsigned int)time(NULL);
}

static int rng_int(int min, int max) {
    if (min >= max) return min;
    g_rng_state = g_rng_state * 1103515245u + 12345u;
    return min + (int)((g_rng_state >> 16) % (unsigned int)(max - min + 1));
}

/* ============================================================================
 * Previous input for edge detection
 * ============================================================================ */

static CELS_Input g_prev_input = {0};

/* ============================================================================
 * Bullet Pool
 * ============================================================================
 *
 * Pre-allocated bullet entities created in composition context.
 * Systems activate/deactivate by mutating components on existing entities.
 */

#define BULLET_POOL_MAX 32

static ecs_entity_t g_bullet_pool[BULLET_POOL_MAX];
static int g_bullet_pool_count = 0;

/* Activate a pooled bullet. Returns the entity, or 0 if pool exhausted. */
static ecs_entity_t bullet_pool_fire(ecs_world_t* world,
                                     float x, float y, float dx, float dy,
                                     const char* ch, int style_id, bool from_player) {
    for (int i = 0; i < g_bullet_pool_count; i++) {
        ecs_entity_t e = g_bullet_pool[i];
        if (!ecs_is_alive(world, e)) continue;
        const BulletTag* bt = ecs_get_id(world, e, BulletTagID);
        if (bt && bt->active) continue;

        /* Activate this slot */
        Position* pos = (Position*)ecs_get_mut_id(world, e, PositionID);
        Velocity* vel = (Velocity*)ecs_get_mut_id(world, e, VelocityID);
        Sprite* spr = (Sprite*)ecs_get_mut_id(world, e, SpriteID);
        BulletTag* tag = (BulletTag*)ecs_get_mut_id(world, e, BulletTagID);
        if (!pos || !vel || !spr || !tag) continue;

        pos->x = x;  pos->y = y;
        vel->dx = dx; vel->dy = dy;
        spr->ch[0] = ch[0]; spr->ch[1] = '\0';
        spr->style_id = style_id;
        tag->from_player = from_player;
        tag->active = true;
        return e;
    }
    return 0;
}

/* Deactivate a pooled bullet (move off-screen, mark inactive). */
static void bullet_pool_deactivate(ecs_world_t* world, ecs_entity_t e) {
    Position* pos = (Position*)ecs_get_mut_id(world, e, PositionID);
    Velocity* vel = (Velocity*)ecs_get_mut_id(world, e, VelocityID);
    BulletTag* tag = (BulletTag*)ecs_get_mut_id(world, e, BulletTagID);
    if (pos) { pos->x = -999; pos->y = -999; }
    if (vel) { vel->dx = 0; vel->dy = 0; }
    if (tag) { tag->active = false; }
}

/* ============================================================================
 * Systems
 * ============================================================================ */

/* --- Title Input System --- */
CEL_System(TitleInputSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    const CELS_Input* input = cels_input_get(ctx);

    if (input->button_accept && !g_prev_input.button_accept) {
        CEL_Update(SI_GameState) {
            SI_GameState.screen = SCREEN_PLAYING;
        }
    }

    memcpy((void*)&g_prev_input, input, sizeof(CELS_Input));
}

/* --- GameOver Input System --- */
CEL_System(GameOverInputSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    const CELS_Input* input = cels_input_get(ctx);

    if (input->button_accept && !g_prev_input.button_accept) {
        CEL_Update(SI_GameState) {
            SI_GameState.screen = SCREEN_PLAYING;
            SI_GameState.score = 0;
            SI_GameState.lives = 3;
            SI_GameState.wave = 1;
            SI_GameState.enemy_speed = BASE_ENEMY_SPEED;
        }
    }

    memcpy((void*)&g_prev_input, input, sizeof(CELS_Input));
}

/* --- Player Input System --- */
CEL_System(PlayerInputSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);
    const CELS_Input* input = cels_input_get(ctx);
    float dt = g_frame_state.delta_time;

    if (PlayerTagID == 0 || PositionID == 0) goto done;

    {
        ecs_iter_t pit = ecs_each_id(world, PlayerTagID);
        while (ecs_each_next(&pit)) {
            PlayerTag* tags = ecs_field(&pit, PlayerTag, 0);
            for (int i = 0; i < pit.count; i++) {
                ecs_entity_t e = pit.entities[i];
                if (!ecs_has_id(world, e, PositionID)) continue;
                Position* pos = (Position*)ecs_get_mut_id(world, e, PositionID);
                if (!pos) continue;

                /* Move left/right */
                if (input->axis_left[0] < -0.5f) {
                    pos->x -= PLAYER_SPEED * dt;
                }
                if (input->axis_left[0] > 0.5f) {
                    pos->x += PLAYER_SPEED * dt;
                }

                /* Clamp to play area */
                if (pos->x < 0) pos->x = 0;
                if (pos->x > PLAY_W - 2) pos->x = (float)(PLAY_W - 2);

                /* Shoot */
                tags[i].shoot_cooldown -= dt;
                if (tags[i].shoot_cooldown < 0) tags[i].shoot_cooldown = 0;

                if (input->button_accept && !g_prev_input.button_accept
                    && tags[i].shoot_cooldown <= 0
                    && SI_GameState.player_bullet_active <= 0) {

                    tags[i].shoot_cooldown = SHOOT_COOLDOWN;

                    /* Activate a pooled bullet above player */
                    if (bullet_pool_fire(world, pos->x, pos->y - 1,
                                         0, -BULLET_SPEED, "|", STYLE_BULLET, true)) {
                        SI_GameState.player_bullet_active = 1.0f;
                    }
                }
            }
        }
    }

done:
    memcpy((void*)&g_prev_input, input, sizeof(CELS_Input));
}

/* --- Enemy Movement System --- */
CEL_System(EnemyMovementSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);
    float dt = g_frame_state.delta_time;

    if (EnemyTagID == 0 || PositionID == 0) return;

    float move_x = SI_GameState.enemy_speed * (float)SI_GameState.enemy_dir * dt;
    bool should_reverse = false;

    /* Check if any enemy hits the wall */
    {
        ecs_iter_t eit = ecs_each_id(world, EnemyTagID);
        while (ecs_each_next(&eit)) {
            for (int i = 0; i < eit.count; i++) {
                ecs_entity_t e = eit.entities[i];
                if (!ecs_has_id(world, e, PositionID)) continue;
                const Position* pos = ecs_get_id(world, e, PositionID);
                if (!pos) continue;

                float nx = pos->x + move_x;
                if (nx < 0 || nx > PLAY_W - 3) {
                    should_reverse = true;
                    break;
                }
            }
            if (should_reverse) break;
        }
    }

    if (should_reverse) {
        SI_GameState.enemy_dir = -SI_GameState.enemy_dir;
        SI_GameState.enemy_should_drop = true;
        move_x = 0;
    }

    /* Apply movement and drop */
    {
        ecs_iter_t eit = ecs_each_id(world, EnemyTagID);
        while (ecs_each_next(&eit)) {
            for (int i = 0; i < eit.count; i++) {
                ecs_entity_t e = eit.entities[i];
                if (!ecs_has_id(world, e, PositionID)) continue;
                Position* pos = (Position*)ecs_get_mut_id(world, e, PositionID);
                if (!pos) continue;

                pos->x += move_x;
                if (SI_GameState.enemy_should_drop) {
                    pos->y += DROP_AMOUNT;
                }
            }
        }
    }

    SI_GameState.enemy_should_drop = false;
}

/* --- Bullet Movement System --- */
CEL_System(BulletMovementSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);
    float dt = g_frame_state.delta_time;

    if (BulletTagID == 0 || PositionID == 0 || VelocityID == 0) return;

    ecs_iter_t bit = ecs_each_id(world, BulletTagID);
    while (ecs_each_next(&bit)) {
        BulletTag* tags = ecs_field(&bit, BulletTag, 0);
        for (int i = 0; i < bit.count; i++) {
            if (!tags[i].active) continue;
            ecs_entity_t e = bit.entities[i];
            if (!ecs_has_id(world, e, PositionID) || !ecs_has_id(world, e, VelocityID))
                continue;
            Position* pos = (Position*)ecs_get_mut_id(world, e, PositionID);
            const Velocity* vel = ecs_get_id(world, e, VelocityID);
            if (!pos || !vel) continue;

            pos->x += vel->dx * dt;
            pos->y += vel->dy * dt;
        }
    }
}

/* --- Enemy Shoot System --- */
CEL_System(EnemyShootSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);
    float dt = g_frame_state.delta_time;

    SI_GameState.enemy_shoot_timer -= dt;
    if (SI_GameState.enemy_shoot_timer > 0) return;
    SI_GameState.enemy_shoot_timer = ENEMY_SHOOT_INTERVAL;

    if (EnemyTagID == 0 || PositionID == 0) return;
    if (SI_GameState.enemies_alive <= 0) return;

    /* Pick a random alive enemy */
    int target = rng_int(0, SI_GameState.enemies_alive - 1);
    int count = 0;

    ecs_iter_t eit = ecs_each_id(world, EnemyTagID);
    while (ecs_each_next(&eit)) {
        for (int i = 0; i < eit.count; i++) {
            ecs_entity_t e = eit.entities[i];
            if (!ecs_has_id(world, e, PositionID)) continue;
            const Position* pos = ecs_get_id(world, e, PositionID);
            if (!pos) continue;

            if (count == target) {
                /* Activate a pooled bullet below enemy */
                bullet_pool_fire(world, pos->x, pos->y + 1,
                                 0, ENEMY_BULLET_SPEED, "!", STYLE_BULLET, false);
                return;
            }
            count++;
        }
    }
}

/* --- Collision System --- */
CEL_System(CollisionSystem,
    .phase = CELS_Phase_PostUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);

    if (BulletTagID == 0 || PositionID == 0) return;

    /* Collect active bullets (avoid modifying world during iteration) */
    #define MAX_BULLETS 64
    ecs_entity_t bullet_ents[MAX_BULLETS];
    Position bullet_pos[MAX_BULLETS];
    BulletTag bullet_tags[MAX_BULLETS];
    int bullet_count = 0;

    {
        ecs_iter_t bit = ecs_each_id(world, BulletTagID);
        while (ecs_each_next(&bit)) {
            BulletTag* tags = ecs_field(&bit, BulletTag, 0);
            for (int i = 0; i < bit.count; i++) {
                if (bullet_count >= MAX_BULLETS) break;
                if (!tags[i].active) continue;
                ecs_entity_t e = bit.entities[i];
                if (!ecs_has_id(world, e, PositionID)) continue;
                const Position* pos = ecs_get_id(world, e, PositionID);
                if (!pos) continue;

                bullet_ents[bullet_count] = e;
                bullet_pos[bullet_count] = *pos;
                bullet_tags[bullet_count] = tags[i];
                bullet_count++;
            }
        }
    }

    /* Track which bullets to delete */
    bool bullet_dead[MAX_BULLETS];
    memset(bullet_dead, 0, sizeof(bullet_dead));

    /* Player bullets vs enemies */
    if (EnemyTagID != 0) {
        for (int bi = 0; bi < bullet_count; bi++) {
            if (!bullet_tags[bi].from_player || bullet_dead[bi]) continue;
            int bx = (int)bullet_pos[bi].x;
            int by = (int)bullet_pos[bi].y;

            ecs_iter_t eit = ecs_each_id(world, EnemyTagID);
            while (ecs_each_next(&eit)) {
                EnemyTag* etags = ecs_field(&eit, EnemyTag, 0);
                for (int ei = 0; ei < eit.count; ei++) {
                    ecs_entity_t ee = eit.entities[ei];
                    if (!ecs_has_id(world, ee, PositionID)) continue;
                    const Position* epos = ecs_get_id(world, ee, PositionID);
                    if (!epos) continue;

                    int ex = (int)epos->x;
                    int ey = (int)epos->y;

                    /* Hit detection: bullet within 1 cell of enemy */
                    if (abs(bx - ex) <= 1 && by == ey) {
                        /* Destroy enemy and bullet */
                        ecs_delete(world, ee);
                        bullet_dead[bi] = true;
                        SI_GameState.enemies_alive--;
                        SI_GameState.score += etags[ei].points;
                        SI_GameState.player_bullet_active = 0;
                        break;
                    }
                }
                if (bullet_dead[bi]) break;
            }
        }
    }

    /* Enemy bullets vs player */
    if (PlayerTagID != 0) {
        for (int bi = 0; bi < bullet_count; bi++) {
            if (bullet_tags[bi].from_player || bullet_dead[bi]) continue;
            int bx = (int)bullet_pos[bi].x;
            int by = (int)bullet_pos[bi].y;

            ecs_iter_t pit = ecs_each_id(world, PlayerTagID);
            while (ecs_each_next(&pit)) {
                for (int pi = 0; pi < pit.count; pi++) {
                    ecs_entity_t pe = pit.entities[pi];
                    if (!ecs_has_id(world, pe, PositionID)) continue;
                    const Position* ppos = ecs_get_id(world, pe, PositionID);
                    if (!ppos) continue;

                    int px = (int)ppos->x;
                    int py = (int)ppos->y;

                    if (abs(bx - px) <= 1 && by == py) {
                        bullet_dead[bi] = true;
                        SI_GameState.lives--;
                        if (SI_GameState.lives <= 0) {
                            CEL_Update(SI_GameState) {
                                SI_GameState.screen = SCREEN_GAMEOVER;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    /* Player bullets vs shields */
    if (ShieldBlockID != 0) {
        for (int bi = 0; bi < bullet_count; bi++) {
            if (bullet_dead[bi]) continue;
            int bx = (int)bullet_pos[bi].x;
            int by = (int)bullet_pos[bi].y;

            ecs_iter_t sit = ecs_each_id(world, ShieldBlockID);
            while (ecs_each_next(&sit)) {
                ShieldBlock* sblocks = ecs_field(&sit, ShieldBlock, 0);
                for (int si = 0; si < sit.count; si++) {
                    ecs_entity_t se = sit.entities[si];
                    if (!ecs_has_id(world, se, PositionID)) continue;
                    const Position* spos = ecs_get_id(world, se, PositionID);
                    if (!spos) continue;

                    int sx = (int)spos->x;
                    int sy = (int)spos->y;

                    if (bx == sx && by == sy) {
                        bullet_dead[bi] = true;
                        sblocks[si].health--;
                        if (sblocks[si].health <= 0) {
                            ecs_delete(world, se);
                        }
                        if (bullet_tags[bi].from_player) {
                            SI_GameState.player_bullet_active = 0;
                        }
                        break;
                    }
                }
                if (bullet_dead[bi]) break;
            }
        }
    }

    /* Enemies reaching bottom -> game over */
    if (EnemyTagID != 0) {
        ecs_iter_t eit = ecs_each_id(world, EnemyTagID);
        while (ecs_each_next(&eit)) {
            for (int i = 0; i < eit.count; i++) {
                ecs_entity_t e = eit.entities[i];
                if (!ecs_has_id(world, e, PositionID)) continue;
                const Position* pos = ecs_get_id(world, e, PositionID);
                if (!pos) continue;
                if ((int)pos->y >= PLAY_H - 3) {
                    CEL_Update(SI_GameState) {
                        SI_GameState.screen = SCREEN_GAMEOVER;
                    }
                    return;
                }
            }
        }
    }

    /* Deactivate dead bullets back to pool */
    for (int bi = 0; bi < bullet_count; bi++) {
        if (bullet_dead[bi] && ecs_is_alive(world, bullet_ents[bi])) {
            bullet_pool_deactivate(world, bullet_ents[bi]);
        }
    }

    /* Check wave clear — transition to WAVE_CLEAR to destroy/recreate GameWorld */
    if (SI_GameState.enemies_alive <= 0 && SI_GameState.screen == SCREEN_PLAYING) {
        CEL_Update(SI_GameState) {
            SI_GameState.wave++;
            SI_GameState.enemy_speed = BASE_ENEMY_SPEED + SPEED_PER_WAVE * (float)SI_GameState.wave;
            SI_GameState.enemy_shoot_timer = ENEMY_SHOOT_INTERVAL;
            SI_GameState.screen = SCREEN_WAVE_CLEAR;
        }
    }
    #undef MAX_BULLETS
}

/* --- Cleanup System --- */
CEL_System(CleanupSystem,
    .phase = CELS_Phase_PostUpdate
) {
    (void)it;
    CELS_Context* ctx = cels_get_context();
    ecs_world_t* world = cels_get_world(ctx);

    if (BulletTagID == 0 || PositionID == 0) return;

    /* Deactivate off-screen bullets back to pool */
    #define MAX_DEACTIVATE 32
    ecs_entity_t to_deactivate[MAX_DEACTIVATE];
    bool was_player[MAX_DEACTIVATE];
    int deact_count = 0;

    ecs_iter_t bit = ecs_each_id(world, BulletTagID);
    while (ecs_each_next(&bit)) {
        BulletTag* tags = ecs_field(&bit, BulletTag, 0);
        for (int i = 0; i < bit.count; i++) {
            if (!tags[i].active) continue;
            ecs_entity_t e = bit.entities[i];
            if (!ecs_has_id(world, e, PositionID)) continue;
            const Position* pos = ecs_get_id(world, e, PositionID);
            if (!pos) continue;

            if (pos->y < -1 || pos->y > PLAY_H + 1 || pos->x < -1 || pos->x > PLAY_W + 1) {
                if (deact_count < MAX_DEACTIVATE) {
                    to_deactivate[deact_count] = e;
                    was_player[deact_count] = tags[i].from_player;
                    deact_count++;
                }
            }
        }
    }

    for (int i = 0; i < deact_count; i++) {
        if (ecs_is_alive(world, to_deactivate[i])) {
            bullet_pool_deactivate(world, to_deactivate[i]);
            if (was_player[i]) {
                SI_GameState.player_bullet_active = 0;
            }
        }
    }
    #undef MAX_DEACTIVATE
}

/* --- Wave Clear System (one-frame transition back to PLAYING) --- */
CEL_System(WaveClearSystem,
    .phase = CELS_Phase_OnUpdate
) {
    (void)it;
    CEL_Update(SI_GameState) {
        SI_GameState.screen = SCREEN_PLAYING;
    }
}

/* ============================================================================
 * Compositions
 * ============================================================================ */

/* Helper: spawn the enemy grid and shields */
static void spawn_game_entities(void) {
    /* Spawn player */
    CEL_Entity({.name = "Player"}) {
        CEL_Has(Position, .x = (float)(PLAY_W / 2), .y = (float)PLAYER_Y);
        CEL_Has(Sprite, .ch = "A", .style_id = STYLE_PLAYER);
        CEL_Has(PlayerTag, .shoot_cooldown = 0);
    }

    /* Spawn enemy grid */
    int enemy_count = 0;
    for (int row = 0; row < ENEMY_ROWS; row++) {
        int style_id;
        int points;
        char ch[4];

        switch (row) {
            case 0: case 1:
                style_id = STYLE_ENEMY_0;
                points = 10;
                ch[0] = 'W'; ch[1] = '\0';
                break;
            case 2: case 3:
                style_id = STYLE_ENEMY_1;
                points = 20;
                ch[0] = 'M'; ch[1] = '\0';
                break;
            default:
                style_id = STYLE_ENEMY_2;
                points = 30;
                ch[0] = 'V'; ch[1] = '\0';
                break;
        }

        for (int col = 0; col < ENEMY_COLS; col++) {
            float ex = (float)(ENEMY_START_X + col * ENEMY_SPACING_X);
            float ey = (float)(ENEMY_START_Y + row * ENEMY_SPACING_Y);

            CEL_Entity({.name = "Enemy"}) {
                CEL_Has(Position, .x = ex, .y = ey);
                CEL_Has(Velocity, .dx = 0, .dy = 0);
                {
                    Sprite _spr = { .ch = {ch[0], '\0'}, .style_id = style_id };
                    cels_component_add(cels_get_context(), Sprite_ensure(), &_spr, sizeof(Sprite));
                }
                CEL_Has(EnemyTag, .type = row, .points = points, .col = col, .row = row);
            }
            enemy_count++;
        }
    }

    /* Spawn shields */
    int shield_positions[SHIELD_COUNT];
    int spacing = PLAY_W / (SHIELD_COUNT + 1);
    for (int s = 0; s < SHIELD_COUNT; s++) {
        shield_positions[s] = spacing * (s + 1) - 2;
    }

    for (int s = 0; s < SHIELD_COUNT; s++) {
        int base_x = shield_positions[s];
        /* 3-wide, 2-tall shield cluster */
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                CEL_Entity({.name = "Shield"}) {
                    CEL_Has(Position, .x = (float)(base_x + dx), .y = (float)(SHIELD_Y + dy));
                    CEL_Has(Sprite, .ch = "#", .style_id = STYLE_SHIELD);
                    CEL_Has(ShieldBlock, .health = 3);
                }
            }
        }
    }

    /* Pre-allocate bullet pool (inactive, off-screen) */
    g_bullet_pool_count = 0;
    for (int i = 0; i < BULLET_POOL_MAX; i++) {
        CEL_Entity({.name = "Bullet"}) {
            CEL_Has(Position, .x = -999, .y = -999);
            CEL_Has(Velocity, .dx = 0, .dy = 0);
            CEL_Has(Sprite, .ch = " ", .style_id = STYLE_BULLET);
            CEL_Has(BulletTag, .from_player = false, .active = false);
            g_bullet_pool[g_bullet_pool_count] = cels_get_current_entity();
        }
        g_bullet_pool_count++;
    }

    SI_GameState.enemies_alive = enemy_count;
    SI_GameState.enemy_dir = 1;
    SI_GameState.enemy_should_drop = false;
    SI_GameState.enemy_shoot_timer = ENEMY_SHOOT_INTERVAL;
    SI_GameState.player_bullet_active = 0;
}

/* --- GameWorld composition --- */
#define GameWorld(...) CEL_Init(GameWorld, __VA_ARGS__)
CEL_Composition(GameWorld) {
    (void)props;

    CEL_Has(GameCanvas, ._unused = 0);

    CEL_Use(PlayerInputSystem);
    CEL_Use(EnemyMovementSystem);
    CEL_Use(BulletMovementSystem);
    CEL_Use(EnemyShootSystem);
    CEL_Use(CollisionSystem);
    CEL_Use(CleanupSystem);

    spawn_game_entities();
}

/* --- TitleComp --- */
#define TitleComp(...) CEL_Init(TitleComp, __VA_ARGS__)
CEL_Composition(TitleComp) {
    (void)props;
    CEL_Has(GameCanvas, ._unused = 0);
    CEL_Use(TitleInputSystem);
}

/* --- GameOverComp --- */
#define GameOverComp(...) CEL_Init(GameOverComp, __VA_ARGS__)
CEL_Composition(GameOverComp) {
    (void)props;
    CEL_Has(GameCanvas, ._unused = 0);
    CEL_Use(GameOverInputSystem);
}

/* --- WaveClearComp (one-frame bridge: destroys GameWorld, transitions to PLAYING) --- */
#define WaveClearComp(...) CEL_Init(WaveClearComp, __VA_ARGS__)
CEL_Composition(WaveClearComp) {
    (void)props;
    CEL_Use(WaveClearSystem);
}

/* ============================================================================
 * Root Composition
 * ============================================================================ */

CEL_Root(AppUI, TUI_EngineContext) {
    TUI_WindowState_t* win = CEL_WatchId(ctx.windowState, TUI_WindowState_t);

    if (win->state == WINDOW_STATE_READY) {
        SI_GameState_t* gs = CEL_Watch(SI_GameState);

        switch (gs->screen) {
            case SCREEN_TITLE:
                TitleComp(.lifecycle = TitleLC) {}
                break;
            case SCREEN_PLAYING:
                GameWorld(.lifecycle = PlayingLC) {}
                break;
            case SCREEN_WAVE_CLEAR:
                WaveClearComp(.lifecycle = WaveClearLC) {}
                break;
            case SCREEN_GAMEOVER:
                GameOverComp(.lifecycle = GameOverLC) {}
                break;
        }
    }
}

/* ============================================================================
 * Init
 * ============================================================================ */

static void InitGameState(void) {
    SI_GameState = (SI_GameState_t){
        .score = 0,
        .lives = 3,
        .wave = 1,
        .enemies_alive = 0,
        .enemy_speed = BASE_ENEMY_SPEED,
        .enemy_dir = 1,
        .enemy_should_drop = false,
        .enemy_shoot_timer = ENEMY_SHOOT_INTERVAL,
        .player_bullet_active = 0,
        .screen = SCREEN_TITLE
    };
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

CEL_Build(SpaceInvaders) {
    (void)props;

    rng_seed();

    TUI_Engine_use((TUI_EngineConfig){
        .title = "Space Invaders",
        .version = "1.0.0",
        .fps = 60,
        .root = AppUI
    });

    si_renderer_init();
    InitGameState();
}
