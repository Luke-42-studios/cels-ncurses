/*
 * TUI Widgets - ncurses renderers for cels-widgets components
 *
 * Each renderer draws into the background layer's WINDOW (not stdscr).
 * The frame pipeline handles erase (frame_begin) and composite (frame_end).
 *
 * Rendering order is guaranteed by the ECS phase: all widget renderers run
 * at CELS_Phase_OnRender (mapped to EcsOnStore), which is bracketed by
 * TUI_FrameBeginSystem (EcsPreStore) and TUI_FrameEndSystem (EcsPostFrame).
 *
 * New widget renderers use the DrawContext API for styled drawing:
 *   W_Button, W_Text, W_InfoBox, W_Canvas, W_Hint,
 *   W_Toggle, W_Cycle, W_RadioButton, W_RadioGroup,
 *   W_ListView, W_ListItem
 */

#include <cels-ncurses/tui_widgets.h>
#include <cels-ncurses/tui_frame.h>
#include <cels-ncurses/tui_layer.h>
#include <cels-ncurses/tui_draw.h>
#include <cels-ncurses/tui_color.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Feature Definition
 * ============================================================================ */

_CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnRender);

/* ============================================================================
 * Theme -- Semantic styles for widget rendering
 * ============================================================================ */

static TUI_Style s_theme_highlight;
static TUI_Style s_theme_active;
static TUI_Style s_theme_inactive;
static TUI_Style s_theme_muted;
static TUI_Style s_theme_accent;
static TUI_Style s_theme_normal;
static bool s_theme_initialized = false;

static void theme_init(void) {
    if (s_theme_initialized) return;
    s_theme_initialized = true;

    s_theme_highlight = (TUI_Style){
        .fg = tui_color_rgb(255, 255, 0),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_BOLD
    };
    s_theme_active = (TUI_Style){
        .fg = tui_color_rgb(0, 255, 0),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_BOLD
    };
    s_theme_inactive = (TUI_Style){
        .fg = tui_color_rgb(255, 0, 0),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_BOLD
    };
    s_theme_muted = (TUI_Style){
        .fg = TUI_COLOR_DEFAULT,
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_DIM
    };
    s_theme_accent = (TUI_Style){
        .fg = tui_color_rgb(0, 255, 255),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_BOLD
    };
    s_theme_normal = (TUI_Style){
        .fg = tui_color_rgb(255, 255, 255),
        .bg = TUI_COLOR_DEFAULT,
        .attrs = TUI_ATTR_NORMAL
    };
}

/* ============================================================================
 * Helpers
 * ============================================================================ */

/*
 * Get the background layer's WINDOW for drawing.
 * Returns NULL if the frame pipeline is not initialized.
 */
static WINDOW* get_bg_win(void) {
    TUI_Layer* bg = tui_frame_get_background();
    if (!bg) return NULL;
    bg->dirty = true;
    return bg->win;
}

/*
 * Get a DrawContext for the background layer.
 * Returns false if the frame pipeline is not initialized.
 */
static bool get_bg_draw_ctx(TUI_DrawContext* out) {
    TUI_Layer* bg = tui_frame_get_background();
    if (!bg) return false;
    *out = tui_layer_get_draw_context(bg);
    return true;
}

/* ============================================================================
 * Row Tracker -- Sequential vertical layout for entity-iterated widgets
 * ============================================================================
 *
 * Widgets like Button, Canvas, Hint, etc. render at sequential rows.
 * The row tracker provides a simple auto-advancing y position.
 * Reset at the start of each frame by the canvas renderer (first widget).
 */

static int g_widget_row = 1;
static int g_widget_frame = 0;

/* ============================================================================
 * TabBar Renderer
 * ============================================================================ */

static void render_tab_bar(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_TabBar* bars = (W_TabBar*)cels_iter_column(it, W_TabBarID, sizeof(W_TabBar));
    if (!bars || count == 0) return;

    WINDOW* win = get_bg_win();
    if (!win) return;

    W_TabBar* bar = &bars[0];

    wattron(win, A_BOLD);
    wmove(win, 0, 0);
    wclrtoeol(win);

    int x = 1;
    int cols = getmaxx(win);

    for (int i = 0; i < bar->count; i++) {
        const char* name = (bar->labels && bar->labels[i]) ? bar->labels[i] : "?";
        int label_len = (int)strlen(name);

        if (i == bar->active) {
            wattron(win, A_REVERSE);
        }

        if (x + label_len + 5 < cols) {
            mvwprintw(win, 0, x, " %d:%s ", i + 1, name);
            x += label_len + 5;
        }

        if (i == bar->active) {
            wattroff(win, A_REVERSE);
        }

        if (i < bar->count - 1 && x < cols) {
            mvwaddch(win, 0, x, ACS_VLINE);
            x += 1;
        }
    }

    wattroff(win, A_BOLD);
}

/* ============================================================================
 * TabContent Renderer
 * ============================================================================ */

static void render_tab_content(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_TabContent* contents = (W_TabContent*)cels_iter_column(it, W_TabContentID, sizeof(W_TabContent));
    if (!contents || count == 0) return;

    WINDOW* win = get_bg_win();
    if (!win) return;

    W_TabContent* content = &contents[0];
    int lines = getmaxy(win);
    int cols = getmaxx(win);
    int center_y = lines / 2;

    /* Centered placeholder text */
    if (content->text) {
        int text_len = (int)strlen(content->text);
        int center_x = (cols - text_len) / 2;
        if (center_x < 0) center_x = 0;

        wattron(win, A_DIM);
        mvwprintw(win, center_y, center_x, "%s", content->text);
        wattroff(win, A_DIM);
    }

    /* Hint below */
    if (content->hint) {
        int hint_len = (int)strlen(content->hint);
        int hint_x = (cols - hint_len) / 2;
        if (hint_x < 0) hint_x = 0;
        mvwprintw(win, center_y + 2, hint_x, "%s", content->hint);
    }
}

/* ============================================================================
 * StatusBar Renderer
 * ============================================================================ */

static void render_status_bar(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_StatusBar* bars = (W_StatusBar*)cels_iter_column(it, W_StatusBarID, sizeof(W_StatusBar));
    if (!bars || count == 0) return;

    WINDOW* win = get_bg_win();
    if (!win) return;

    W_StatusBar* bar = &bars[0];
    int lines = getmaxy(win);
    int cols = getmaxx(win);
    int y = lines - 1;

    wattron(win, A_REVERSE);
    wmove(win, y, 0);
    wclrtoeol(win);

    /* Left section */
    if (bar->left) {
        mvwprintw(win, y, 1, " %s ", bar->left);
    }

    /* Right section */
    if (bar->right) {
        int right_len = (int)strlen(bar->right);
        if (cols > right_len + 2) {
            mvwprintw(win, y, cols - right_len - 1, "%s", bar->right);
        }
    }

    wattroff(win, A_REVERSE);
}

/* ============================================================================
 * Canvas Renderer -- Header box with centered title
 * ============================================================================ */

#define DEFAULT_CANVAS_WIDTH 43

static void render_canvas(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Canvas* canvases = (W_Canvas*)cels_iter_column(it, W_CanvasID, sizeof(W_Canvas));
    if (!canvases || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    /* Reset row tracker for each new frame's canvas render */
    g_widget_frame++;
    g_widget_row = 1;

    for (int i = 0; i < count; i++) {
        W_Canvas* c = &canvases[i];
        int w = (c->width > 0) ? c->width : DEFAULT_CANVAS_WIDTH;
        TUI_CellRect box = { 0, g_widget_row, w, 3 };
        tui_draw_border_rect(&ctx, box, TUI_BORDER_SINGLE, s_theme_normal);

        if (c->title) {
            int title_len = (int)strlen(c->title);
            int inner_width = w - 2;
            int title_x = 1 + (inner_width - title_len) / 2;
            tui_draw_text(&ctx, title_x, g_widget_row + 1, c->title, s_theme_accent);
        }

        g_widget_row += 3;
    }

    /* Blank line after canvas */
    g_widget_row++;
}

/* ============================================================================
 * Button Renderer -- Selectable button with highlight state
 * ============================================================================ */

static void render_button(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Button* buttons = (W_Button*)cels_iter_column(it, W_ButtonID, sizeof(W_Button));
    if (!buttons || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_Button* btn = &buttons[i];
        if (!btn->label) continue;

        if (btn->selected) {
            char buf[64];
            snprintf(buf, sizeof(buf), "> %-20s <", btn->label);
            tui_draw_text(&ctx, 0, g_widget_row, buf, s_theme_highlight);
        } else {
            tui_draw_text(&ctx, 2, g_widget_row, btn->label, s_theme_muted);
        }
        g_widget_row++;
    }
}

/* ============================================================================
 * Slider Renderer -- Labeled value bar
 * ============================================================================ */

static void render_slider(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Slider* sliders = (W_Slider*)cels_iter_column(it, W_SliderID, sizeof(W_Slider));
    if (!sliders || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_Slider* s = &sliders[i];
        if (!s->label) continue;

        char label_buf[32];
        snprintf(label_buf, sizeof(label_buf), "%-12s ", s->label);
        tui_draw_text(&ctx, 0, g_widget_row, label_buf, s_theme_normal);

        /* Draw slider bar [====    ] */
        int bar_x = 13;
        int bar_width = 20;
        float range = (s->max > s->min) ? (s->max - s->min) : 1.0f;
        float norm = (s->value - s->min) / range;
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        int filled = (int)(norm * bar_width);

        char bar_buf[32];
        bar_buf[0] = '[';
        for (int j = 0; j < bar_width; j++) {
            bar_buf[j + 1] = (j < filled) ? '=' : ' ';
        }
        bar_buf[bar_width + 1] = ']';
        bar_buf[bar_width + 2] = '\0';
        tui_draw_text(&ctx, bar_x, g_widget_row, bar_buf, s_theme_accent);

        g_widget_row++;
    }
}

/* ============================================================================
 * Text Renderer -- Simple text display
 * ============================================================================ */

static void render_text(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Text* texts = (W_Text*)cels_iter_column(it, W_TextID, sizeof(W_Text));
    if (!texts || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_Text* t = &texts[i];
        if (!t->text) continue;
        tui_draw_text(&ctx, 0, g_widget_row, t->text, s_theme_normal);
        g_widget_row++;
    }
}

/* ============================================================================
 * Hint Renderer -- Dim hint text
 * ============================================================================ */

static void render_hint(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Hint* hints = (W_Hint*)cels_iter_column(it, W_HintID, sizeof(W_Hint));
    if (!hints || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_Hint* h = &hints[i];
        if (!h->text) continue;
        /* Blank line before hint */
        g_widget_row++;
        tui_draw_text(&ctx, 0, g_widget_row, h->text, s_theme_muted);
        g_widget_row++;
    }
}

/* ============================================================================
 * InfoBox Renderer -- Bordered box with title and content
 * ============================================================================ */

#define DEFAULT_INFOBOX_WIDTH 43

static void render_info_box(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_InfoBox* boxes = (W_InfoBox*)cels_iter_column(it, W_InfoBoxID, sizeof(W_InfoBox));
    if (!boxes || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_InfoBox* box = &boxes[i];

        /* Blank line before info box */
        g_widget_row++;

        if (box->border) {
            TUI_CellRect rect = { 0, g_widget_row, DEFAULT_INFOBOX_WIDTH, 3 };
            tui_draw_border_rect(&ctx, rect, TUI_BORDER_SINGLE, s_theme_normal);

            int cx = 2;
            int cy = g_widget_row + 1;

            if (box->title) {
                tui_draw_text(&ctx, cx, cy, "Title: ", s_theme_muted);
                cx += 7;
                char title_buf[20];
                snprintf(title_buf, sizeof(title_buf), "%-16s", box->title);
                tui_draw_text(&ctx, cx, cy, title_buf, s_theme_accent);
                cx += 16;
            }

            if (box->content) {
                tui_draw_text(&ctx, cx, cy, box->content, s_theme_normal);
            }

            g_widget_row += 3;
        } else {
            if (box->title) {
                tui_draw_text(&ctx, 0, g_widget_row, box->title, s_theme_accent);
                g_widget_row++;
            }
            if (box->content) {
                tui_draw_text(&ctx, 0, g_widget_row, box->content, s_theme_normal);
                g_widget_row++;
            }
        }
    }
}

/* ============================================================================
 * Toggle Renderer -- ON/OFF toggle with label
 * ============================================================================ */

static void render_toggle(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Toggle* toggles = (W_Toggle*)cels_iter_column(it, W_ToggleID, sizeof(W_Toggle));
    if (!toggles || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_Toggle* t = &toggles[i];
        if (!t->label) continue;

        /* Label */
        char label_buf[32];
        snprintf(label_buf, sizeof(label_buf), "%-12s ", t->label);
        if (t->selected) {
            tui_draw_text(&ctx, 0, g_widget_row, label_buf, s_theme_highlight);
        } else {
            tui_draw_text(&ctx, 0, g_widget_row, label_buf, s_theme_normal);
        }

        int toggle_x = 13;
        if (t->value) {
            tui_draw_text(&ctx, toggle_x, g_widget_row, "[ OFF ]", s_theme_muted);
            tui_draw_text(&ctx, toggle_x + 7, g_widget_row, "[= ON =]", s_theme_active);
        } else {
            tui_draw_text(&ctx, toggle_x, g_widget_row, "[= OFF =]", s_theme_inactive);
            tui_draw_text(&ctx, toggle_x + 9, g_widget_row, "[ ON ]", s_theme_muted);
        }

        g_widget_row++;
    }
}

/* ============================================================================
 * Cycle Renderer -- Left/right cycle selector with label
 * ============================================================================ */

static void render_cycle(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_Cycle* cycles = (W_Cycle*)cels_iter_column(it, W_CycleID, sizeof(W_Cycle));
    if (!cycles || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_Cycle* c = &cycles[i];
        if (!c->label) continue;

        char label_buf[32];
        snprintf(label_buf, sizeof(label_buf), "%-12s ", c->label);

        if (c->selected) {
            tui_draw_text(&ctx, 0, g_widget_row, label_buf, s_theme_highlight);

            int lx = 13;
            tui_draw_text(&ctx, lx, g_widget_row, "[", s_theme_normal);
            tui_draw_text(&ctx, lx + 1, g_widget_row, "<", s_theme_accent);
            tui_draw_text(&ctx, lx + 2, g_widget_row, "] ", s_theme_normal);

            TUI_Style value_style = s_theme_normal;
            value_style.attrs = TUI_ATTR_BOLD;
            char val_buf[20];
            snprintf(val_buf, sizeof(val_buf), "%-15s", c->value ? c->value : "");
            tui_draw_text(&ctx, lx + 4, g_widget_row, val_buf, value_style);

            int vx = lx + 4 + 15;
            tui_draw_text(&ctx, vx, g_widget_row, " [", s_theme_normal);
            tui_draw_text(&ctx, vx + 2, g_widget_row, ">", s_theme_accent);
            tui_draw_text(&ctx, vx + 3, g_widget_row, "]", s_theme_normal);
        } else {
            tui_draw_text(&ctx, 0, g_widget_row, label_buf, s_theme_normal);

            int lx = 13;
            tui_draw_text(&ctx, lx, g_widget_row, "[<]", s_theme_muted);
            tui_draw_text(&ctx, lx + 3, g_widget_row, " ", s_theme_normal);

            TUI_Style value_style = s_theme_normal;
            value_style.attrs = TUI_ATTR_BOLD;
            char val_buf[20];
            snprintf(val_buf, sizeof(val_buf), "%-15s", c->value ? c->value : "");
            tui_draw_text(&ctx, lx + 4, g_widget_row, val_buf, value_style);

            int vx = lx + 4 + 15;
            tui_draw_text(&ctx, vx, g_widget_row, " ", s_theme_normal);
            tui_draw_text(&ctx, vx + 1, g_widget_row, "[>]", s_theme_muted);
        }

        g_widget_row++;
    }
}

/* ============================================================================
 * RadioButton Renderer -- Individual radio option
 * ============================================================================ */

static void render_radio_button(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_RadioButton* radios = (W_RadioButton*)cels_iter_column(it, W_RadioButtonID, sizeof(W_RadioButton));
    if (!radios || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_RadioButton* r = &radios[i];
        if (!r->label) continue;

        char buf[64];
        if (r->selected) {
            snprintf(buf, sizeof(buf), "(*) %s", r->label);
            tui_draw_text(&ctx, 2, g_widget_row, buf, s_theme_highlight);
        } else {
            snprintf(buf, sizeof(buf), "( ) %s", r->label);
            tui_draw_text(&ctx, 2, g_widget_row, buf, s_theme_muted);
        }
        g_widget_row++;
    }
}

/* ============================================================================
 * RadioGroup Renderer -- Container header for radio group
 * ============================================================================ */

static void render_radio_group(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_RadioGroup* groups = (W_RadioGroup*)cels_iter_column(it, W_RadioGroupID, sizeof(W_RadioGroup));
    if (!groups || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_RadioGroup* g = &groups[i];
        char buf[64];
        snprintf(buf, sizeof(buf), "Radio Group %d (%d/%d)", g->group_id, g->selected_index + 1, g->count);
        tui_draw_text(&ctx, 0, g_widget_row, buf, s_theme_accent);
        g_widget_row++;
    }
}

/* ============================================================================
 * ListView Renderer -- Scrollable list container
 * ============================================================================ */

static void render_list_view(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_ListView* lists = (W_ListView*)cels_iter_column(it, W_ListViewID, sizeof(W_ListView));
    if (!lists || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_ListView* lv = &lists[i];
        char buf[64];
        snprintf(buf, sizeof(buf), "List (%d items, showing from %d)", lv->item_count, lv->scroll_offset);
        tui_draw_text(&ctx, 0, g_widget_row, buf, s_theme_muted);
        g_widget_row++;
    }
}

/* ============================================================================
 * ListItem Renderer -- Individual list item
 * ============================================================================ */

static void render_list_item(CELS_Iter* it) {
    int count = cels_iter_count(it);
    W_ListItem* items = (W_ListItem*)cels_iter_column(it, W_ListItemID, sizeof(W_ListItem));
    if (!items || count == 0) return;

    theme_init();
    TUI_DrawContext ctx;
    if (!get_bg_draw_ctx(&ctx)) return;

    for (int i = 0; i < count; i++) {
        W_ListItem* item = &items[i];
        if (!item->label) continue;

        if (item->selected) {
            char buf[64];
            snprintf(buf, sizeof(buf), "> %s", item->label);
            tui_draw_text(&ctx, 2, g_widget_row, buf, s_theme_highlight);
        } else {
            tui_draw_text(&ctx, 4, g_widget_row, item->label, s_theme_normal);
        }
        g_widget_row++;
    }
}

/* ============================================================================
 * Registration
 * ============================================================================ */

void tui_widgets_register(void) {
    /* Declare features */
    _CEL_Feature(W_TabBar, Renderable);
    _CEL_Feature(W_TabContent, Renderable);
    _CEL_Feature(W_StatusBar, Renderable);
    _CEL_Feature(W_Button, Renderable);
    _CEL_Feature(W_Slider, Renderable);
    _CEL_Feature(W_Text, Renderable);
    _CEL_Feature(W_InfoBox, Renderable);
    _CEL_Feature(W_Canvas, Renderable);
    _CEL_Feature(W_Hint, Renderable);
    _CEL_Feature(W_Toggle, Renderable);
    _CEL_Feature(W_Cycle, Renderable);
    _CEL_Feature(W_RadioButton, Renderable);
    _CEL_Feature(W_RadioGroup, Renderable);
    _CEL_Feature(W_ListView, Renderable);
    _CEL_Feature(W_ListItem, Renderable);

    /* Register providers */
    _CEL_Provides(TUI, Renderable, W_TabBar, render_tab_bar);
    _CEL_Provides(TUI, Renderable, W_TabContent, render_tab_content);
    _CEL_Provides(TUI, Renderable, W_StatusBar, render_status_bar);
    _CEL_Provides(TUI, Renderable, W_Canvas, render_canvas);
    _CEL_Provides(TUI, Renderable, W_Button, render_button);
    _CEL_Provides(TUI, Renderable, W_Slider, render_slider);
    _CEL_Provides(TUI, Renderable, W_Text, render_text);
    _CEL_Provides(TUI, Renderable, W_InfoBox, render_info_box);
    _CEL_Provides(TUI, Renderable, W_Hint, render_hint);
    _CEL_Provides(TUI, Renderable, W_Toggle, render_toggle);
    _CEL_Provides(TUI, Renderable, W_Cycle, render_cycle);
    _CEL_Provides(TUI, Renderable, W_RadioButton, render_radio_button);
    _CEL_Provides(TUI, Renderable, W_RadioGroup, render_radio_group);
    _CEL_Provides(TUI, Renderable, W_ListView, render_list_view);
    _CEL_Provides(TUI, Renderable, W_ListItem, render_list_item);

    /* Declare consumed components */
    _CEL_ProviderConsumes(W_TabBar, W_TabContent, W_StatusBar, W_Button,
                          W_Slider, W_Text, W_InfoBox, W_Canvas);
    _CEL_ProviderConsumes(W_Hint, W_Toggle, W_Cycle, W_RadioButton,
                          W_RadioGroup, W_ListView, W_ListItem);
}
