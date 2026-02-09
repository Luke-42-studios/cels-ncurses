/*
 * TUI Widgets - ncurses renderers for standard widget components
 *
 * Each renderer draws into the background layer's WINDOW (not stdscr).
 * The frame pipeline handles erase (frame_begin) and composite (frame_end).
 *
 * Rendering order is guaranteed by the ECS phase: all widget renderers run
 * at CELS_Phase_OnRender (mapped to EcsOnStore), which is bracketed by
 * TUI_FrameBeginSystem (EcsPreStore) and TUI_FrameEndSystem (EcsPostFrame).
 */

#include <cels-ncurses/tui_widgets.h>
#include <cels-ncurses/tui_frame.h>
#include <cels-ncurses/tui_layer.h>
#include <ncurses.h>
#include <string.h>

/* ============================================================================
 * Feature Definition
 * ============================================================================ */

_CEL_DefineFeature(Renderable, .phase = CELS_Phase_OnRender);

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

/* ============================================================================
 * TabBar Renderer
 * ============================================================================ */

static void render_tab_bar(CELS_Iter* it) {
    int count = cels_iter_count(it);
    TUI_TabBar* bars = (TUI_TabBar*)cels_iter_column(it, TUI_TabBarID, sizeof(TUI_TabBar));
    if (!bars || count == 0) return;

    WINDOW* win = get_bg_win();
    if (!win) return;

    TUI_TabBar* bar = &bars[0];

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
    TUI_TabContent* contents = (TUI_TabContent*)cels_iter_column(it, TUI_TabContentID, sizeof(TUI_TabContent));
    if (!contents || count == 0) return;

    WINDOW* win = get_bg_win();
    if (!win) return;

    TUI_TabContent* content = &contents[0];
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
    TUI_StatusBar* bars = (TUI_StatusBar*)cels_iter_column(it, TUI_StatusBarID, sizeof(TUI_StatusBar));
    if (!bars || count == 0) return;

    WINDOW* win = get_bg_win();
    if (!win) return;

    TUI_StatusBar* bar = &bars[0];
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
 * Registration
 * ============================================================================ */

void tui_widgets_register(void) {
    /* Declare features */
    _CEL_Feature(TUI_TabBar, Renderable);
    _CEL_Feature(TUI_TabContent, Renderable);
    _CEL_Feature(TUI_StatusBar, Renderable);

    /* Register providers */
    _CEL_Provides(TUI, Renderable, TUI_TabBar, render_tab_bar);
    _CEL_Provides(TUI, Renderable, TUI_TabContent, render_tab_content);
    _CEL_Provides(TUI, Renderable, TUI_StatusBar, render_status_bar);

    /* Declare consumed components */
    _CEL_ProviderConsumes(TUI_TabBar, TUI_TabContent, TUI_StatusBar);
}
