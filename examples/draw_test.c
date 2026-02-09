/*
 * Interactive API Showcase Demo
 *
 * Scene-based interactive demo that exercises every API function in the
 * cels-ncurses module: colors, styles, drawing primitives, borders, layers,
 * scissors, frame pipeline, and text rendering.
 *
 * Controls:
 *   Menu:  1-7 select scene, q quit
 *   Scene: Escape/q return to menu, scene-specific keys shown in HUD
 */

#include <cels-ncurses/tui_types.h>
#include <cels-ncurses/tui_color.h>
#include <cels-ncurses/tui_draw_context.h>
#include <cels-ncurses/tui_draw.h>
#include <cels-ncurses/tui_scissor.h>
#include <cels-ncurses/tui_layer.h>
#include <cels-ncurses/tui_frame.h>
#include <ncurses.h>
#include <panel.h>
#include <locale.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

/* ============================================================================
 * Style Palette
 * ============================================================================ */

static TUI_Style s_bg;
static TUI_Style s_title;
static TUI_Style s_subtitle;
static TUI_Style s_normal;
static TUI_Style s_dim;
static TUI_Style s_bold;
static TUI_Style s_highlight;
static TUI_Style s_accent;
static TUI_Style s_error;
static TUI_Style s_success;
static TUI_Style s_info;
static TUI_Style s_bar_fill;
static TUI_Style s_bar_empty;
static TUI_Style s_menu_item;
static TUI_Style s_menu_sel;
static TUI_Style s_hud;

static void init_styles(void) {
    TUI_Color white  = tui_color_rgb(255, 255, 255);
    TUI_Color black  = tui_color_rgb(0, 0, 0);
    TUI_Color red    = tui_color_rgb(220, 50, 50);
    TUI_Color green  = tui_color_rgb(50, 200, 80);
    TUI_Color blue   = tui_color_rgb(60, 120, 220);
    TUI_Color yellow = tui_color_rgb(230, 200, 50);
    TUI_Color cyan   = tui_color_rgb(80, 220, 220);
    TUI_Color gray   = tui_color_rgb(100, 100, 100);
    TUI_Color dk_blue = tui_color_rgb(20, 30, 60);
    TUI_Color orange = tui_color_rgb(230, 130, 50);
    TUI_Color purple = tui_color_rgb(160, 80, 220);
    TUI_Color dk_gray = tui_color_rgb(40, 40, 40);

    TUI_Color bg_c = tui_color_rgb(18, 18, 28);

    s_bg        = (TUI_Style){ .fg = white,  .bg = bg_c,   .attrs = TUI_ATTR_NORMAL };
    s_title     = (TUI_Style){ .fg = cyan,   .bg = bg_c,   .attrs = TUI_ATTR_BOLD };
    s_subtitle  = (TUI_Style){ .fg = yellow, .bg = bg_c,   .attrs = TUI_ATTR_BOLD };
    s_normal    = (TUI_Style){ .fg = white,  .bg = bg_c,   .attrs = TUI_ATTR_NORMAL };
    s_dim       = (TUI_Style){ .fg = gray,   .bg = bg_c,   .attrs = TUI_ATTR_DIM };
    s_bold      = (TUI_Style){ .fg = white,  .bg = bg_c,   .attrs = TUI_ATTR_BOLD };
    s_highlight = (TUI_Style){ .fg = black,  .bg = cyan,   .attrs = TUI_ATTR_BOLD };
    s_accent    = (TUI_Style){ .fg = orange, .bg = bg_c,   .attrs = TUI_ATTR_BOLD };
    s_error     = (TUI_Style){ .fg = red,    .bg = bg_c,   .attrs = TUI_ATTR_BOLD };
    s_success   = (TUI_Style){ .fg = green,  .bg = bg_c,   .attrs = TUI_ATTR_BOLD };
    s_info      = (TUI_Style){ .fg = blue,   .bg = bg_c,   .attrs = TUI_ATTR_NORMAL };
    s_bar_fill  = (TUI_Style){ .fg = black,  .bg = green,  .attrs = TUI_ATTR_NORMAL };
    s_bar_empty = (TUI_Style){ .fg = gray,   .bg = dk_gray, .attrs = TUI_ATTR_DIM };
    s_menu_item = (TUI_Style){ .fg = white,  .bg = dk_blue, .attrs = TUI_ATTR_NORMAL };
    s_menu_sel  = (TUI_Style){ .fg = black,  .bg = cyan,   .attrs = TUI_ATTR_BOLD };
    s_hud       = (TUI_Style){ .fg = purple, .bg = bg_c,   .attrs = TUI_ATTR_DIM };
}

/* ============================================================================
 * Scene State
 * ============================================================================ */

#define SCENE_MENU 0
#define SCENE_COUNT 7

static int g_current_scene = SCENE_MENU;
static int g_running = 1;

/* Scene 1 — Color Palette */
static int s1_cursor = 0;

/* Scene 2 — Border Gallery */
static int s2_highlight = 0;
static float s2_timer = 0.0f;

/* Scene 3 — Bouncing Box */
static float s3_x = 5.0f, s3_y = 3.0f;
static float s3_vx = 18.0f, s3_vy = 10.0f;
#define S3_BOX_W 12
#define S3_BOX_H 5
#define S3_TRAIL_LEN 8
static int s3_trail_x[S3_TRAIL_LEN];
static int s3_trail_y[S3_TRAIL_LEN];
static int s3_trail_idx = 0;
static float s3_trail_timer = 0.0f;

/* Scene 4 — Layer Playground */
#define S4_LAYER_COUNT 3
static TUI_Layer* s4_layers[S4_LAYER_COUNT];
static int s4_active = 0;
static const char* s4_colors[] = { "Red", "Green", "Blue" };

/* Scene 5 — Scissor Demo */
static float s5_phase = 0.0f;

/* Scene 6 — Text & Typography */
static float s6_scroll = 0.0f;

/* Scene 7 — Dashboard */
static TUI_Layer* s7_popup = NULL;
static float s7_bars[4] = { 0.3f, 0.6f, 0.45f, 0.8f };
static float s7_bar_targets[4] = { 0.7f, 0.2f, 0.9f, 0.5f };
static float s7_log_scroll = 0.0f;
static int s7_popup_visible = 0;

/* ============================================================================
 * Helpers
 * ============================================================================ */

static void draw_hud(TUI_DrawContext* ctx, int cols, int rows,
                     const char* scene_name, const char* controls) {
    char buf[128];
    snprintf(buf, sizeof(buf), " [%d] %s ", g_current_scene, scene_name);
    tui_draw_text(ctx, 1, 0, buf, s_title);

    snprintf(buf, sizeof(buf), "FPS: %.0f  Frame: %lu  dt: %.3f",
             g_frame_state.fps, (unsigned long)g_frame_state.frame_count,
             g_frame_state.delta_time);
    int len = (int)strlen(buf);
    tui_draw_text(ctx, cols - len - 1, 0, buf, s_hud);

    tui_draw_text(ctx, 1, rows - 1, controls, s_dim);
    tui_draw_text(ctx, cols - 16, rows - 1, "ESC: back to menu", s_dim);
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ============================================================================
 * Scene 0 — Menu
 * ============================================================================ */

static void draw_menu(TUI_DrawContext* ctx, int cols, int rows) {
    static const char* scene_names[] = {
        "1. Color Palette",
        "2. Border Gallery",
        "3. Bouncing Box",
        "4. Layer Playground",
        "5. Scissor Demo",
        "6. Text & Typography",
        "7. Dashboard"
    };

    const char* title = "cels-ncurses API Showcase";
    int title_len = (int)strlen(title);
    int cx = (cols - title_len) / 2;
    int cy = rows / 2 - 6;
    tui_draw_text(ctx, cx, cy, title, s_title);

    /* Subtitle */
    const char* sub = "Interactive demo of all 40+ API functions";
    int sub_len = (int)strlen(sub);
    tui_draw_text(ctx, (cols - sub_len) / 2, cy + 1, sub, s_dim);

    /* Menu box */
    int box_w = 34;
    int box_h = SCENE_COUNT + 4;
    int box_x = (cols - box_w) / 2;
    int box_y = cy + 3;
    tui_draw_border_rect(ctx, (TUI_CellRect){box_x, box_y, box_w, box_h},
                         TUI_BORDER_ROUNDED, s_info);
    tui_draw_fill_rect(ctx, (TUI_CellRect){box_x + 1, box_y + 1, box_w - 2, box_h - 2},
                       ' ', s_normal);

    tui_draw_text(ctx, box_x + 2, box_y + 1, "Select a scene:", s_subtitle);
    tui_draw_hline(ctx, box_x + 1, box_y + 2, box_w - 2, TUI_BORDER_SINGLE, s_dim);

    for (int i = 0; i < SCENE_COUNT; i++) {
        tui_draw_text(ctx, box_x + 3, box_y + 3 + i, scene_names[i], s_normal);
    }

    /* Footer */
    const char* footer = "Press 1-7 to enter, q to quit";
    int flen = (int)strlen(footer);
    tui_draw_text(ctx, (cols - flen) / 2, box_y + box_h + 1, footer, s_dim);

    /* FPS in corner */
    char fps_buf[32];
    snprintf(fps_buf, sizeof(fps_buf), "FPS: %.0f", g_frame_state.fps);
    tui_draw_text(ctx, cols - (int)strlen(fps_buf) - 1, 0, fps_buf, s_hud);
}

/* ============================================================================
 * Scene 1 — Color Palette
 * ============================================================================ */

static void draw_scene_colors(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Color Palette",
             "Arrow keys: move cursor");

    /* Color grid — 6 levels per channel, 6x6x6 = 216 colors */
    int grid_x = 3;
    int grid_y = 2;
    tui_draw_text(ctx, grid_x, grid_y, "RGB Color Grid (tui_color_rgb):", s_subtitle);
    grid_y += 1;

    int swatch_w = 3;
    int max_per_row = (cols - grid_x - 2) / swatch_w;
    if (max_per_row > 36) max_per_row = 36;

    int idx = 0;
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                int row = idx / max_per_row;
                int col_off = idx % max_per_row;
                int sx = grid_x + col_off * swatch_w;
                int sy = grid_y + row;
                if (sy >= rows - 8) goto done_grid;

                uint8_t rv = (uint8_t)(r * 51);
                uint8_t gv = (uint8_t)(g * 51);
                uint8_t bv = (uint8_t)(b * 51);
                TUI_Color c = tui_color_rgb(rv, gv, bv);
                TUI_Style st = { .fg = c, .bg = c, .attrs = TUI_ATTR_NORMAL };

                /* Highlight the cursor position */
                if (idx == s1_cursor) {
                    st.attrs = TUI_ATTR_REVERSE;
                    tui_draw_fill_rect(ctx, (TUI_CellRect){sx, sy, swatch_w, 1}, '>', s_highlight);
                } else {
                    tui_draw_fill_rect(ctx, (TUI_CellRect){sx, sy, swatch_w, 1}, ' ', st);
                }
                idx++;
            }
        }
    }
done_grid:;

    /* Attribute showcase */
    int ay = rows - 6;
    tui_draw_text(ctx, 3, ay, "Attribute Flags:", s_subtitle);
    ay++;
    TUI_Color white = tui_color_rgb(255, 255, 255);
    TUI_Style attr_norm = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_NORMAL };
    TUI_Style attr_bold = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
    TUI_Style attr_dim  = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_DIM };
    TUI_Style attr_ul   = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_UNDERLINE };
    TUI_Style attr_rev  = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_REVERSE };

    tui_draw_text(ctx, 3,  ay, "NORMAL",    attr_norm);
    tui_draw_text(ctx, 13, ay, "BOLD",      attr_bold);
    tui_draw_text(ctx, 21, ay, "DIM",       attr_dim);
    tui_draw_text(ctx, 28, ay, "UNDERLINE", attr_ul);
    tui_draw_text(ctx, 41, ay, "REVERSE",   attr_rev);

    /* TUI_COLOR_DEFAULT demo */
    ay += 2;
    TUI_Style def_style = { .fg = TUI_COLOR_DEFAULT, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_NORMAL };
    tui_draw_text(ctx, 3, ay, "TUI_COLOR_DEFAULT fg+bg (terminal default colors)", def_style);
}

/* ============================================================================
 * Scene 2 — Border Gallery
 * ============================================================================ */

static void draw_scene_borders(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Border Gallery", "Auto-cycling highlight");

    s2_timer += g_frame_state.delta_time;
    if (s2_timer > 1.0f) {
        s2_timer = 0.0f;
        s2_highlight = (s2_highlight + 1) % 9;
    }

    int bx = 3, by = 2;
    tui_draw_text(ctx, bx, by, "Border Styles (tui_draw_border_rect):", s_subtitle);
    by += 2;

    TUI_BorderStyle styles[] = { TUI_BORDER_SINGLE, TUI_BORDER_DOUBLE, TUI_BORDER_ROUNDED };
    const char* names[] = { "SINGLE", "DOUBLE", "ROUNDED" };
    TUI_Style border_styles[] = { s_info, s_error, s_success };

    for (int i = 0; i < 3; i++) {
        int bx_off = bx + i * 18;
        TUI_Style st = (s2_highlight == i) ? s_highlight : border_styles[i];
        tui_draw_border_rect(ctx, (TUI_CellRect){bx_off, by, 16, 5}, styles[i], st);
        tui_draw_text(ctx, bx_off + 2, by + 2, names[i], st);
    }

    /* Per-side borders */
    by += 7;
    tui_draw_text(ctx, bx, by, "Per-Side Borders (tui_draw_border):", s_subtitle);
    by += 2;

    struct { uint8_t sides; const char* label; } combos[] = {
        { TUI_SIDE_TOP,                    "TOP" },
        { TUI_SIDE_BOTTOM,                 "BTM" },
        { TUI_SIDE_LEFT | TUI_SIDE_RIGHT,  "L+R" },
        { TUI_SIDE_TOP | TUI_SIDE_BOTTOM,  "T+B" },
        { TUI_SIDE_ALL,                    "ALL" },
    };
    int ncombos = 5;

    for (int i = 0; i < ncombos; i++) {
        int cx = bx + i * 12;
        TUI_Style st = (s2_highlight == 3 + i) ? s_highlight : s_normal;
        tui_draw_border(ctx, (TUI_CellRect){cx, by, 10, 4},
                        combos[i].sides, TUI_BORDER_SINGLE, st);
        tui_draw_text(ctx, cx + 1, by + 1, combos[i].label, st);
    }

    /* Lines */
    by += 6;
    tui_draw_text(ctx, bx, by, "Lines (tui_draw_hline / tui_draw_vline):", s_subtitle);
    by += 1;
    TUI_Style ln_st = (s2_highlight == 8) ? s_highlight : s_accent;
    tui_draw_hline(ctx, bx, by, 25, TUI_BORDER_SINGLE, ln_st);
    tui_draw_hline(ctx, bx, by + 1, 25, TUI_BORDER_DOUBLE, ln_st);
    tui_draw_hline(ctx, bx, by + 2, 25, TUI_BORDER_ROUNDED, ln_st);
    tui_draw_vline(ctx, bx + 30, by, 5, TUI_BORDER_SINGLE, ln_st);
    tui_draw_vline(ctx, bx + 32, by, 5, TUI_BORDER_DOUBLE, ln_st);
    tui_draw_vline(ctx, bx + 34, by, 5, TUI_BORDER_ROUNDED, ln_st);
    tui_draw_text(ctx, bx + 27, by, "S", s_dim);
    tui_draw_text(ctx, bx + 27, by + 1, "D", s_dim);
    tui_draw_text(ctx, bx + 27, by + 2, "R", s_dim);

    /* tui_border_chars_get showcase */
    by += 6;
    tui_draw_text(ctx, bx, by, "tui_border_chars_get() corner chars:", s_subtitle);
    by += 1;
    TUI_BorderChars bc = tui_border_chars_get(TUI_BORDER_ROUNDED);
    /* Just show that we can call it — draw corners manually */
    if (bc.ul) {
        tui_style_apply(ctx->win, s_success);
        mvwadd_wch(ctx->win, by, bx, bc.ul);
        mvwadd_wch(ctx->win, by, bx + 2, bc.ur);
        mvwadd_wch(ctx->win, by + 1, bx, bc.ll);
        mvwadd_wch(ctx->win, by + 1, bx + 2, bc.lr);
    }
    tui_draw_text(ctx, bx + 5, by, "UL  UR", s_dim);
    tui_draw_text(ctx, bx + 5, by + 1, "LL  LR  (rounded corners)", s_dim);
}

/* ============================================================================
 * Scene 3 — Bouncing Box
 * ============================================================================ */

static void draw_scene_bounce(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Bouncing Box",
             "Automatic physics  |  g_frame_state timing");

    float dt = g_frame_state.delta_time;
    int area_y = 2;
    int area_h = rows - 4;
    int area_w = cols - 2;

    /* Physics */
    s3_x += s3_vx * dt;
    s3_y += s3_vy * dt;

    if (s3_x < 1) { s3_x = 1; s3_vx = -s3_vx; }
    if (s3_y < area_y) { s3_y = (float)area_y; s3_vy = -s3_vy; }
    if (s3_x + S3_BOX_W > area_w) { s3_x = (float)(area_w - S3_BOX_W); s3_vx = -s3_vx; }
    if (s3_y + S3_BOX_H > area_y + area_h) { s3_y = (float)(area_y + area_h - S3_BOX_H); s3_vy = -s3_vy; }

    /* Ghost trail */
    s3_trail_timer += dt;
    if (s3_trail_timer > 0.05f) {
        s3_trail_timer = 0.0f;
        s3_trail_x[s3_trail_idx] = (int)s3_x;
        s3_trail_y[s3_trail_idx] = (int)s3_y;
        s3_trail_idx = (s3_trail_idx + 1) % S3_TRAIL_LEN;
    }

    for (int i = 0; i < S3_TRAIL_LEN; i++) {
        if (s3_trail_x[i] == 0 && s3_trail_y[i] == 0) continue;
        int age = (s3_trail_idx - i + S3_TRAIL_LEN) % S3_TRAIL_LEN;
        uint8_t bright = (uint8_t)(30 + age * 8);
        TUI_Color ghost_c = tui_color_rgb(bright, bright, bright);
        TUI_Style ghost_st = { .fg = ghost_c, .bg = ghost_c, .attrs = TUI_ATTR_DIM };
        tui_draw_fill_rect(ctx, (TUI_CellRect){s3_trail_x[i], s3_trail_y[i],
                           S3_BOX_W, S3_BOX_H}, ' ', ghost_st);
    }

    /* Main box using tui_rect_to_cells */
    TUI_Rect float_rect = { .x = s3_x, .y = s3_y, .w = S3_BOX_W, .h = S3_BOX_H };
    TUI_CellRect box = tui_rect_to_cells(float_rect);

    TUI_Color box_fg = tui_color_rgb(255, 255, 255);
    TUI_Color box_bg = tui_color_rgb(60, 120, 220);
    TUI_Style box_fill = { .fg = box_fg, .bg = box_bg, .attrs = TUI_ATTR_NORMAL };
    tui_draw_fill_rect(ctx, box, ' ', box_fill);
    tui_draw_border_rect(ctx, box, TUI_BORDER_ROUNDED, s_accent);

    /* Position text inside box */
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "%.1f,%.1f", s3_x, s3_y);
    tui_draw_text(ctx, box.x + 1, box.y + 1, pos_buf, s_bold);

    char vel_buf[32];
    snprintf(vel_buf, sizeof(vel_buf), "v:%.0f,%.0f", s3_vx, s3_vy);
    tui_draw_text(ctx, box.x + 1, box.y + 2, vel_buf, s_normal);

    /* Delta time / frame timing info */
    char dt_buf[64];
    snprintf(dt_buf, sizeof(dt_buf), "dt=%.4fs  frame=%lu",
             g_frame_state.delta_time, (unsigned long)g_frame_state.frame_count);
    tui_draw_text(ctx, 3, rows - 3, dt_buf, s_dim);
}

/* ============================================================================
 * Scene 4 — Layer Playground
 * ============================================================================ */

static void init_scene_layers(void) {
    TUI_Color colors_fg[] = {
        tui_color_rgb(255, 255, 255),
        tui_color_rgb(0, 0, 0),
        tui_color_rgb(255, 255, 255)
    };
    TUI_Color colors_bg[] = {
        tui_color_rgb(180, 50, 50),
        tui_color_rgb(50, 200, 80),
        tui_color_rgb(60, 80, 200)
    };

    for (int i = 0; i < S4_LAYER_COUNT; i++) {
        int lx = 5 + i * 15;
        int ly = 3 + i * 3;
        s4_layers[i] = tui_layer_create(s4_colors[i], lx, ly, 20, 8);
        if (s4_layers[i]) {
            TUI_DrawContext lctx = tui_layer_get_draw_context(s4_layers[i]);
            tui_scissor_reset(&lctx);
            TUI_Style lst = { .fg = colors_fg[i], .bg = colors_bg[i], .attrs = TUI_ATTR_NORMAL };
            tui_draw_fill_rect(&lctx, (TUI_CellRect){0, 0, 20, 8}, ' ', lst);
            tui_draw_border_rect(&lctx, (TUI_CellRect){0, 0, 20, 8},
                                 TUI_BORDER_ROUNDED, lst);
            char label[32];
            snprintf(label, sizeof(label), " %s Layer ", s4_colors[i]);
            tui_draw_text(&lctx, 1, 0, label, lst);
        }
    }
    s4_active = 0;
}

static void cleanup_scene_layers(void) {
    for (int i = S4_LAYER_COUNT - 1; i >= 0; i--) {
        if (s4_layers[i]) {
            tui_layer_destroy(s4_layers[i]);
            s4_layers[i] = NULL;
        }
    }
}

static void draw_scene_layers(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Layer Playground",
             "Arrows:move Tab:cycle R/L:raise/lower H:hide/show +/-:resize");

    /* Info panel on background */
    char info[64];
    snprintf(info, sizeof(info), "Active: %s  |  g_layer_count=%d  |  max=%d",
             s4_colors[s4_active], g_layer_count, TUI_LAYER_MAX);
    tui_draw_text(ctx, 3, 1, info, s_info);

    /* Redraw active layer content with highlight border */
    for (int i = 0; i < S4_LAYER_COUNT; i++) {
        if (!s4_layers[i] || !s4_layers[i]->win) continue;

        TUI_Color colors_fg[] = {
            tui_color_rgb(255, 255, 255),
            tui_color_rgb(0, 0, 0),
            tui_color_rgb(255, 255, 255)
        };
        TUI_Color colors_bg[] = {
            tui_color_rgb(180, 50, 50),
            tui_color_rgb(50, 200, 80),
            tui_color_rgb(60, 80, 200)
        };

        TUI_DrawContext lctx = tui_layer_get_draw_context(s4_layers[i]);
        tui_scissor_reset(&lctx);
        int lw = s4_layers[i]->width;
        int lh = s4_layers[i]->height;
        TUI_Style lst = { .fg = colors_fg[i], .bg = colors_bg[i], .attrs = TUI_ATTR_NORMAL };
        tui_draw_fill_rect(&lctx, (TUI_CellRect){0, 0, lw, lh}, ' ', lst);

        TUI_Style border_st = (i == s4_active) ? s_highlight : lst;
        tui_draw_border_rect(&lctx, (TUI_CellRect){0, 0, lw, lh},
                             TUI_BORDER_ROUNDED, border_st);

        char label[32];
        snprintf(label, sizeof(label), " %s ", s4_colors[i]);
        tui_draw_text(&lctx, 1, 0, label, border_st);

        char pos[32];
        snprintf(pos, sizeof(pos), "%dx%d @%d,%d",
                 s4_layers[i]->width, s4_layers[i]->height,
                 s4_layers[i]->x, s4_layers[i]->y);
        tui_draw_text(&lctx, 1, 2, pos, lst);

        tui_draw_text(&lctx, 1, 3, s4_layers[i]->visible ? "visible" : "HIDDEN", lst);
    }
}

static void handle_input_layers(int ch) {
    if (!s4_layers[s4_active]) return;
    TUI_Layer* active = s4_layers[s4_active];

    switch (ch) {
        case '\t':
            s4_active = (s4_active + 1) % S4_LAYER_COUNT;
            break;
        case KEY_UP:
            tui_layer_move(active, active->x, active->y - 1);
            break;
        case KEY_DOWN:
            tui_layer_move(active, active->x, active->y + 1);
            break;
        case KEY_LEFT:
            tui_layer_move(active, active->x - 1, active->y);
            break;
        case KEY_RIGHT:
            tui_layer_move(active, active->x + 1, active->y);
            break;
        case 'r': case 'R':
            tui_layer_raise(active);
            break;
        case 'l': case 'L':
            tui_layer_lower(active);
            break;
        case 'h': case 'H':
            if (active->visible)
                tui_layer_hide(active);
            else
                tui_layer_show(active);
            break;
        case '+': case '=':
            tui_layer_resize(active, active->width + 2, active->height + 1);
            break;
        case '-': case '_':
            if (active->width > 8 && active->height > 4)
                tui_layer_resize(active, active->width - 2, active->height - 1);
            break;
    }
}

/* ============================================================================
 * Scene 5 — Scissor Demo
 * ============================================================================ */

static void draw_scene_scissors(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Scissor Demo",
             "Animated clip regions  |  push/pop/reset/intersect/contains");

    float dt = g_frame_state.delta_time;
    s5_phase += dt * 1.5f;

    int cx = cols / 2;
    int cy = rows / 2;

    /* Outer region */
    int outer_w = 40;
    int outer_h = 14;
    TUI_CellRect outer = { cx - outer_w / 2, cy - outer_h / 2, outer_w, outer_h };

    tui_draw_text(ctx, outer.x, outer.y - 2, "tui_scissor_reset + tui_push_scissor + tui_pop_scissor:", s_subtitle);

    /* Draw outer border and label */
    tui_draw_border_rect(ctx, outer, TUI_BORDER_DOUBLE, s_dim);
    tui_draw_text(ctx, outer.x + 1, outer.y, " Outer Clip ", s_dim);

    /* Oscillating inner clip */
    float osc = sinf(s5_phase) * 0.5f + 0.5f; /* 0..1 */
    int inner_w = 10 + (int)(osc * 20.0f);
    int inner_h = 4 + (int)(osc * 6.0f);
    int inner_x = cx - inner_w / 2;
    int inner_y = cy - inner_h / 2;
    TUI_CellRect inner = { inner_x, inner_y, inner_w, inner_h };

    /* Show intersection */
    TUI_CellRect isect = tui_cell_rect_intersect(outer, inner);

    /* Push outer, then inner — drawing is clipped to intersection */
    tui_push_scissor(ctx, outer);
    tui_push_scissor(ctx, inner);

    /* Fill with pattern — only shows in intersection */
    TUI_Color fill_c = tui_color_rgb(40, 100, 180);
    TUI_Style fill_st = { .fg = tui_color_rgb(255, 255, 255), .bg = fill_c, .attrs = TUI_ATTR_NORMAL };
    tui_draw_fill_rect(ctx, (TUI_CellRect){0, 0, cols, rows}, '#', fill_st);
    tui_draw_text(ctx, cx - 6, cy, "CLIPPED AREA", s_highlight);

    tui_pop_scissor(ctx);
    tui_pop_scissor(ctx);

    /* Draw inner border (unclipped) */
    tui_draw_border_rect(ctx, inner, TUI_BORDER_SINGLE, s_accent);
    tui_draw_text(ctx, inner.x + 1, inner.y, " Inner Clip ", s_accent);

    /* tui_cell_rect_contains demo */
    int test_px = cx;
    int test_py = cy;
    int contains = tui_cell_rect_contains(isect, test_px, test_py);
    char cbuf[64];
    snprintf(cbuf, sizeof(cbuf), "contains(%d,%d) = %s", test_px, test_py,
             contains ? "true" : "false");
    tui_draw_text(ctx, outer.x, outer.y + outer_h + 1, cbuf, contains ? s_success : s_error);

    /* Intersection info */
    char ibuf[64];
    snprintf(ibuf, sizeof(ibuf), "intersect: {%d,%d,%d,%d}",
             isect.x, isect.y, isect.w, isect.h);
    tui_draw_text(ctx, outer.x, outer.y + outer_h + 2, ibuf, s_info);
}

/* ============================================================================
 * Scene 6 — Text & Typography
 * ============================================================================ */

static void draw_scene_text(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Text & Typography",
             "Scrolling marquee  |  tui_draw_text + tui_draw_text_bounded");

    float dt = g_frame_state.delta_time;
    s6_scroll += dt * 15.0f;

    int tx = 3, ty = 2;

    tui_draw_text(ctx, tx, ty, "Text Rendering (tui_draw_text):", s_subtitle);
    ty += 2;

    /* All attribute styles */
    TUI_Color white = tui_color_rgb(255, 255, 255);
    TUI_Style st_norm = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_NORMAL };
    TUI_Style st_bold = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_BOLD };
    TUI_Style st_dim  = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_DIM };
    TUI_Style st_ul   = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_UNDERLINE };
    TUI_Style st_rev  = { .fg = white, .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_REVERSE };
    TUI_Style st_combo = { .fg = tui_color_rgb(230, 200, 50), .bg = TUI_COLOR_DEFAULT,
                           .attrs = TUI_ATTR_BOLD | TUI_ATTR_UNDERLINE };

    tui_draw_text(ctx, tx, ty,     "NORMAL: The quick brown fox jumps", st_norm);
    tui_draw_text(ctx, tx, ty + 1, "BOLD:   The quick brown fox jumps", st_bold);
    tui_draw_text(ctx, tx, ty + 2, "DIM:    The quick brown fox jumps", st_dim);
    tui_draw_text(ctx, tx, ty + 3, "UNDER:  The quick brown fox jumps", st_ul);
    tui_draw_text(ctx, tx, ty + 4, "REVERSE: The quick brown fox jumps", st_rev);
    tui_draw_text(ctx, tx, ty + 5, "BOLD+UL: The quick brown fox jumps", st_combo);

    /* Bounded text demo */
    ty += 8;
    tui_draw_text(ctx, tx, ty, "Bounded Text (tui_draw_text_bounded):", s_subtitle);
    ty += 2;

    int bounds[] = { 10, 20, 30, 40 };
    const char* long_str = "This string is much longer than the bounded width allows!";
    for (int i = 0; i < 4; i++) {
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%2d cols:", bounds[i]);
        tui_draw_text(ctx, tx, ty + i, lbl, s_dim);
        tui_draw_text_bounded(ctx, tx + 10, ty + i, long_str, bounds[i], s_normal);
        /* Boundary marker */
        tui_draw_text(ctx, tx + 10 + bounds[i], ty + i, "|", s_error);
    }

    /* Scrolling marquee */
    ty += 6;
    tui_draw_text(ctx, tx, ty, "Scrolling Marquee:", s_subtitle);
    ty += 1;

    const char* marquee = "--- Welcome to the cels-ncurses API showcase! "
                          "This demonstrates every drawing primitive. ---   ";
    int mlen = (int)strlen(marquee);
    int scroll_off = (int)s6_scroll % mlen;
    int marquee_w = cols - tx - 3;

    /* Use scissor to clip marquee */
    TUI_CellRect marquee_clip = { tx, ty, marquee_w, 1 };
    tui_push_scissor(ctx, marquee_clip);

    /* Draw two copies for seamless wrap */
    for (int i = 0; i < 2; i++) {
        int off = -scroll_off + i * mlen;
        tui_draw_text(ctx, tx + off, ty, marquee, s_accent);
    }

    tui_pop_scissor(ctx);

    /* TUI_COLOR_DEFAULT demo */
    ty += 3;
    TUI_Style def_fg = { .fg = TUI_COLOR_DEFAULT, .bg = tui_color_rgb(40, 40, 80), .attrs = TUI_ATTR_NORMAL };
    TUI_Style def_bg = { .fg = tui_color_rgb(255, 200, 50), .bg = TUI_COLOR_DEFAULT, .attrs = TUI_ATTR_NORMAL };
    tui_draw_text(ctx, tx, ty, "Default FG on custom BG", def_fg);
    tui_draw_text(ctx, tx + 30, ty, "Custom FG on default BG", def_bg);
}

/* ============================================================================
 * Scene 7 — Dashboard
 * ============================================================================ */

static void init_scene_dashboard(void) {
    s7_popup = tui_layer_create("popup", 0, 0, 30, 10);
    if (s7_popup) {
        tui_layer_hide(s7_popup);
        s7_popup_visible = 0;
    }
}

static void cleanup_scene_dashboard(void) {
    if (s7_popup) {
        tui_layer_destroy(s7_popup);
        s7_popup = NULL;
    }
    s7_popup_visible = 0;
}

static void draw_scene_dashboard(TUI_DrawContext* ctx, int cols, int rows) {
    draw_hud(ctx, cols, rows, "Dashboard",
             "P:toggle popup  |  Combined API demo");

    float dt = g_frame_state.delta_time;

    /* Animate bars toward targets */
    for (int i = 0; i < 4; i++) {
        s7_bars[i] = lerp(s7_bars[i], s7_bar_targets[i], dt * 2.0f);
        /* Slowly change targets */
        s7_bar_targets[i] += (sinf(s5_phase + (float)i * 1.5f) * 0.3f) * dt;
        s7_bar_targets[i] = clampf(s7_bar_targets[i], 0.05f, 0.95f);
    }
    s7_log_scroll += dt * 3.0f;

    int panel_w = cols / 2 - 2;
    int panel_h = rows / 2 - 2;

    /* ---- Top-left: CPU bars ---- */
    {
        TUI_CellRect p = { 1, 2, panel_w, panel_h };
        tui_draw_border_rect(ctx, p, TUI_BORDER_ROUNDED, s_info);
        tui_draw_text(ctx, p.x + 1, p.y, " CPU Usage ", s_info);

        const char* bar_names[] = { "Core 0", "Core 1", "Core 2", "Core 3" };
        int bar_w = panel_w - 14;
        for (int i = 0; i < 4; i++) {
            int by = p.y + 2 + i * 2;
            if (by >= p.y + panel_h - 1) break;
            tui_draw_text(ctx, p.x + 2, by, bar_names[i], s_dim);

            int fill_w = (int)(s7_bars[i] * (float)bar_w);
            if (fill_w < 0) fill_w = 0;
            if (fill_w > bar_w) fill_w = bar_w;

            /* Filled portion */
            TUI_Color bar_c = tui_color_rgb(
                (uint8_t)(50 + s7_bars[i] * 200),
                (uint8_t)(200 - s7_bars[i] * 150),
                50);
            TUI_Style bfill = { .fg = tui_color_rgb(0, 0, 0), .bg = bar_c, .attrs = TUI_ATTR_NORMAL };
            if (fill_w > 0)
                tui_draw_fill_rect(ctx, (TUI_CellRect){p.x + 10, by, fill_w, 1}, ' ', bfill);
            /* Empty portion */
            if (fill_w < bar_w)
                tui_draw_fill_rect(ctx, (TUI_CellRect){p.x + 10 + fill_w, by, bar_w - fill_w, 1}, ' ', s_bar_empty);

            char pct[8];
            snprintf(pct, sizeof(pct), "%3d%%", (int)(s7_bars[i] * 100));
            tui_draw_text(ctx, p.x + 10 + bar_w + 1, by, pct, s_normal);
        }
    }

    /* ---- Top-right: Frame timing ---- */
    {
        TUI_CellRect p = { cols / 2, 2, panel_w, panel_h };
        tui_draw_border_rect(ctx, p, TUI_BORDER_ROUNDED, s_accent);
        tui_draw_text(ctx, p.x + 1, p.y, " Frame Timing ", s_accent);

        char buf[64];
        int ly = p.y + 2;
        snprintf(buf, sizeof(buf), "FPS:       %.1f", g_frame_state.fps);
        tui_draw_text(ctx, p.x + 2, ly++, buf, s_bold);
        snprintf(buf, sizeof(buf), "Delta:     %.4f s", g_frame_state.delta_time);
        tui_draw_text(ctx, p.x + 2, ly++, buf, s_normal);
        snprintf(buf, sizeof(buf), "Frame:     %lu", (unsigned long)g_frame_state.frame_count);
        tui_draw_text(ctx, p.x + 2, ly++, buf, s_normal);
        snprintf(buf, sizeof(buf), "Layers:    %d / %d", g_layer_count, TUI_LAYER_MAX);
        tui_draw_text(ctx, p.x + 2, ly++, buf, s_normal);

        /* Draw hline separator */
        tui_draw_hline(ctx, p.x + 1, ly, panel_w - 2, TUI_BORDER_SINGLE, s_dim);
        ly++;

        snprintf(buf, sizeof(buf), "in_frame:  %s", g_frame_state.in_frame ? "true" : "false");
        tui_draw_text(ctx, p.x + 2, ly++, buf, g_frame_state.in_frame ? s_success : s_error);
    }

    /* ---- Bottom-left: Scrolling log with scissor ---- */
    {
        TUI_CellRect p = { 1, rows / 2 + 1, panel_w, panel_h };
        tui_draw_border_rect(ctx, p, TUI_BORDER_ROUNDED, s_success);
        tui_draw_text(ctx, p.x + 1, p.y, " Event Log (scissored) ", s_success);

        /* Clip to inside of panel */
        TUI_CellRect log_clip = { p.x + 1, p.y + 1, p.w - 2, p.h - 2 };
        tui_push_scissor(ctx, log_clip);

        static const char* log_msgs[] = {
            "[INFO]  Frame pipeline initialized",
            "[INFO]  Background layer created at z=0",
            "[OK]    Color system: alloc_pair active",
            "[INFO]  Layer 'popup' created",
            "[WARN]  Layer 'popup' hidden",
            "[INFO]  Scissor stack reset",
            "[OK]    Border chars resolved (rounded)",
            "[INFO]  tui_frame_invalidate_all called",
            "[INFO]  Terminal resize detected",
            "[OK]    All layers resized",
        };
        int nmsgs = 10;
        int scroll_i = (int)s7_log_scroll;

        for (int i = 0; i < p.h - 2; i++) {
            int msg_idx = (scroll_i + i) % nmsgs;
            TUI_Style msg_st = s_normal;
            if (strncmp(log_msgs[msg_idx], "[WARN]", 6) == 0)
                msg_st = s_error;
            else if (strncmp(log_msgs[msg_idx], "[OK]", 4) == 0)
                msg_st = s_success;
            else
                msg_st = s_dim;
            tui_draw_text(ctx, p.x + 1, p.y + 1 + i, log_msgs[msg_idx], msg_st);
        }

        tui_pop_scissor(ctx);
    }

    /* ---- Bottom-right: Color gradient + vline ---- */
    {
        TUI_CellRect p = { cols / 2, rows / 2 + 1, panel_w, panel_h };
        tui_draw_border_rect(ctx, p, TUI_BORDER_ROUNDED, s_error);
        tui_draw_text(ctx, p.x + 1, p.y, " Color Gradient ", s_error);

        int grad_w = p.w - 4;
        int grad_y = p.y + 2;
        for (int x = 0; x < grad_w && x < 48; x++) {
            uint8_t r = (uint8_t)(x * 255 / (grad_w > 1 ? grad_w - 1 : 1));
            TUI_Color gc = tui_color_rgb(r, 50, (uint8_t)(255 - r));
            TUI_Style gs = { .fg = gc, .bg = gc, .attrs = TUI_ATTR_NORMAL };
            tui_draw_fill_rect(ctx, (TUI_CellRect){p.x + 2 + x, grad_y, 1, 2}, ' ', gs);
        }

        /* Vertical separators */
        tui_draw_vline(ctx, p.x + p.w / 3, p.y + 1, p.h - 2, TUI_BORDER_SINGLE, s_dim);
        tui_draw_vline(ctx, p.x + 2 * p.w / 3, p.y + 1, p.h - 2, TUI_BORDER_SINGLE, s_dim);
    }

    /* ---- Popup layer ---- */
    if (s7_popup && s7_popup_visible) {
        int pw = 30, ph = 8;
        int px = (cols - pw) / 2;
        int py = (rows - ph) / 2;
        tui_layer_move(s7_popup, px, py);
        tui_layer_resize(s7_popup, pw, ph);
        tui_layer_raise(s7_popup);

        TUI_DrawContext pctx = tui_layer_get_draw_context(s7_popup);
        tui_scissor_reset(&pctx);

        TUI_Color pop_bg = tui_color_rgb(30, 30, 50);
        TUI_Style pop_fill = { .fg = tui_color_rgb(255, 255, 255), .bg = pop_bg, .attrs = TUI_ATTR_NORMAL };
        tui_draw_fill_rect(&pctx, (TUI_CellRect){0, 0, pw, ph}, ' ', pop_fill);
        tui_draw_border_rect(&pctx, (TUI_CellRect){0, 0, pw, ph}, TUI_BORDER_DOUBLE, s_accent);
        tui_draw_text(&pctx, 1, 0, " Popup Layer ", s_accent);
        tui_draw_text(&pctx, 2, 2, "This is a layer popup!", pop_fill);
        tui_draw_text(&pctx, 2, 3, "Drawn on its own PANEL", pop_fill);
        tui_draw_text(&pctx, 2, 5, "Press P to close", s_dim);
    }
}

static void handle_input_dashboard(int ch) {
    if ((ch == 'p' || ch == 'P') && s7_popup) {
        s7_popup_visible = !s7_popup_visible;
        if (s7_popup_visible)
            tui_layer_show(s7_popup);
        else
            tui_layer_hide(s7_popup);
    }
}

/* ============================================================================
 * Scene Transitions
 * ============================================================================ */

static void cleanup_current_scene(void) {
    switch (g_current_scene) {
        case 4: cleanup_scene_layers(); break;
        case 7: cleanup_scene_dashboard(); break;
        default: break;
    }
}

static void transition_scene(int new_scene) {
    cleanup_current_scene();
    g_current_scene = new_scene;

    switch (new_scene) {
        case 4: init_scene_layers(); break;
        case 7: init_scene_dashboard(); break;
        default: break;
    }

    tui_frame_invalidate_all();
}

/* ============================================================================
 * Input Handling
 * ============================================================================ */

static void handle_input(int ch) {
    if (ch == ERR) return;

    if (ch == KEY_RESIZE) {
        /* Resize background layer to new terminal size */
        TUI_Layer* bg = tui_frame_get_background();
        if (bg) {
            tui_layer_resize(bg, COLS, LINES);
        }
        tui_frame_invalidate_all();
        return;
    }

    if (g_current_scene == SCENE_MENU) {
        if (ch == 'q' || ch == 'Q') {
            g_running = 0;
        } else if (ch >= '1' && ch <= '7') {
            transition_scene(ch - '0');
        }
    } else {
        /* In a scene — escape or q returns to menu */
        if (ch == 27 || ch == 'q' || ch == 'Q') {
            transition_scene(SCENE_MENU);
        } else {
            /* Scene-specific input */
            switch (g_current_scene) {
                case 1: {
                    int max_idx = 216 - 1;
                    if (ch == KEY_RIGHT) s1_cursor = (s1_cursor + 1) % 216;
                    else if (ch == KEY_LEFT) s1_cursor = (s1_cursor - 1 + 216) % 216;
                    else if (ch == KEY_DOWN) s1_cursor = (s1_cursor + 36) > max_idx ? s1_cursor : s1_cursor + 36;
                    else if (ch == KEY_UP) s1_cursor = (s1_cursor - 36) < 0 ? s1_cursor : s1_cursor - 36;
                    break;
                }
                case 4:
                    handle_input_layers(ch);
                    break;
                case 7:
                    handle_input_dashboard(ch);
                    break;
            }
        }
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    set_escdelay(25);

    /* Initialize frame pipeline — creates background layer */
    tui_frame_init();

    /* Initialize color/style palette */
    init_styles();

    /* Initialize trail */
    memset(s3_trail_x, 0, sizeof(s3_trail_x));
    memset(s3_trail_y, 0, sizeof(s3_trail_y));

    while (g_running) {
        /* Input — drain all pending keys */
        int ch;
        while ((ch = getch()) != ERR) {
            handle_input(ch);
            if (!g_running) break;
        }
        if (!g_running) break;

        /* Frame begin */
        tui_frame_begin();

        /* Get background draw context */
        TUI_Layer* bg = tui_frame_get_background();
        if (bg) {
            TUI_DrawContext ctx = tui_layer_get_draw_context(bg);
            tui_scissor_reset(&ctx);

            int cols = bg->width;
            int rows = bg->height;

            /* Fill entire background with solid color */
            tui_draw_fill_rect(&ctx, (TUI_CellRect){0, 0, cols, rows}, ' ', s_bg);

            switch (g_current_scene) {
                case SCENE_MENU: draw_menu(&ctx, cols, rows); break;
                case 1: draw_scene_colors(&ctx, cols, rows); break;
                case 2: draw_scene_borders(&ctx, cols, rows); break;
                case 3: draw_scene_bounce(&ctx, cols, rows); break;
                case 4: draw_scene_layers(&ctx, cols, rows); break;
                case 5: draw_scene_scissors(&ctx, cols, rows); break;
                case 6: draw_scene_text(&ctx, cols, rows); break;
                case 7: draw_scene_dashboard(&ctx, cols, rows); break;
            }
        }

        /* Frame end — composite + doupdate */
        tui_frame_end();

        usleep(16000); /* ~60fps */
    }

    /* Cleanup */
    cleanup_current_scene();
    endwin();
    return 0;
}
