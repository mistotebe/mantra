/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * Mantra: Man page bookmarker                                           *
 * Copyright (C) 2015  Tom Pickering                                     *
 *                                                                       *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                       *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "win.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <ncurses.h>

#include "helpbar.h"
#include "../file.h"
#include "../pty.h"

const int WIN_COL_PAIR_NORMAL      = 0;
const int WIN_COL_PAIR_ACTIVE      = 1;
const int WIN_COL_PAIR_BOOKMARK_HL = 2;
const int WIN_COL_PAIR_PAGE_HL     = 3;

const int WIN_IDX_BOOKMARKS = 0;
const int WIN_IDX_PAGES     = 1;
const int WIN_IDX_HELPBAR   = 2;
const int NWIN              = 3;

Win **wins;
int win_act_idx;

/**
 * Default input handler for windows. It should never be called,
 * but if a bug allows it to be then it's better than a segfault!
 */
void input_default(int n) {
    fprintf(stderr, "Error: This window does not take input.\n");
    exit(1);
}

void win_init_all() {
    int i;
    wins = (Win **)calloc(NWIN, sizeof(Win *));
    for (i = 0; i < NWIN; ++i) {
        wins[i] = (Win *)calloc(1, sizeof(Win));
        wins[i]->win = newwin(0, 0, 0, 0);
    }

    wins[WIN_IDX_BOOKMARKS]->draw = draw_win_bookmarks;
    wins[WIN_IDX_PAGES    ]->draw = draw_win_pages;
    wins[WIN_IDX_HELPBAR  ]->draw = draw_win_helpbar;

    wins[WIN_IDX_BOOKMARKS]->input = input_win_bookmarks;
    wins[WIN_IDX_PAGES    ]->input = input_win_pages;
    wins[WIN_IDX_HELPBAR  ]->input = input_win_helpbar;

    wins[WIN_IDX_BOOKMARKS]->update = update_win_bookmarks;
    wins[WIN_IDX_PAGES    ]->update = update_win_pages;
    wins[WIN_IDX_HELPBAR  ]->update = NULL;

    wins[WIN_IDX_BOOKMARKS]->can_be_active = true;
    wins[WIN_IDX_PAGES    ]->can_be_active = true;
    wins[WIN_IDX_HELPBAR  ]->can_be_active = false;

    init_pair(WIN_COL_PAIR_NORMAL     , COLOR_WHITE, COLOR_BLACK);
    init_pair(WIN_COL_PAIR_ACTIVE     , COLOR_GREEN, COLOR_BLACK);
    init_pair(WIN_COL_PAIR_BOOKMARK_HL, COLOR_BLUE , COLOR_BLACK);
    init_pair(WIN_COL_PAIR_PAGE_HL    , COLOR_GREEN, COLOR_BLACK);

    if (bookmarks) win_act_idx = WIN_IDX_BOOKMARKS;
    else win_act_idx = WIN_IDX_PAGES;

    bar_init();
}

/**
 * Update a window's position and size attributes.
 */
void win_update(Win *window, int x, int y, int r, int c) {
    WINDOW *win = window->win;
    wresize(win, r, c);
    mvwin(win, y, x);
    window->r = r;
    window->c = c;
    if (window->update) window->update();
}

/**
 * Overwrite a row of characters in a window with spaces.
 */
void win_clear_row(Win *win, int r) {
    char wipe_char = ' ';
    char *wiper = (char *)calloc(win->c + 1, sizeof(char));
    memset(wiper, wipe_char, win->c);
    wiper[win->c] = '\0';
    mvwprintw(win->win, r, 0, wiper);
    free(wiper);
}

/**
 * Completely overwrite a window's area with space characters.
 * This does not call win_clear_row for memory allocation
 * efficiency.
 */
void win_clear_all() {
    int i, j, r, c;
    WINDOW *win;
    char *wiper;
    char wipe_char = ' ';
    getmaxyx(stdscr, r, c);
    wiper = (char *)malloc((c + 1) * sizeof(char));
    memset(wiper, wipe_char, c);
    wiper[c] = '\0';
    for (i = 0; i < NWIN; ++i) {
        win = wins[i]->win;
        getmaxyx(win, r, c);
        for (j = 0; j < r; ++j)
            mvwprintw(win, j, 0, wiper);
    }
    free(wiper);
}

/**
 * Find the next Win for which can_be_active is true
 * and make it the currently-active window.
 */
void win_cycle_active() {
    int idx;
    for (idx = (win_act_idx + 1) % NWIN; idx != win_act_idx; idx = (idx + 1) % NWIN) {
        if (wins[idx]->can_be_active) {
            win_act_idx = idx;
            break;
        }
    }
}

void win_set_active(int idx) {
    win_act_idx = idx;
}

int win_active() {
    return win_act_idx;
}

void win_draw_border(Win *win) {
    int col_pair = WIN_COL_PAIR_NORMAL;
    if (wins[win_act_idx] == win)
        col_pair = WIN_COL_PAIR_ACTIVE;
    wattron(win->win, COLOR_PAIR(col_pair));
    box(win->win, 0, 0);
    wattroff(win->win, COLOR_PAIR(col_pair));
}

/**
 * Iterate over all windows and call their draw method.
 */
void win_draw_all() {
    int i;
    int r, c;
    for (i = 0; i < NWIN; ++i) {
        getmaxyx(wins[i]->win, r, c);
        wins[i]->r = r;
        wins[i]->c = c;
        wins[i]->draw();
    }
}

Win *active_win() {
    return wins[win_act_idx];
}

/**
 * Copy a string into a buffer to a max of len bytes.
 * Furthermore, ensure that the buffer is clean and null-terminated.
 */
char *string_clean_buffer(char *buf, char *src, unsigned int len) {
    int src_len = strlen(src);
    int i, c;
    strncpy(buf, src, len);
    if (src_len < len)
        memset(buf + src_len, ' ', len - src_len);
    if (src_len > len) {
        for(i = len - 1, c = 0; i > 0 && c < 3; i--, c++)
            buf[i] = '.';
    }
    buf[len] = '\0';
    return buf;
}

/**
 * Spawn a 'man' process displaying the requested page.
 */
int open_page(char *sect, char *page, char *line) {
    char *cmd[] = { "man", "--pager=less", NULL, NULL, NULL };
    int line_str_len;
    char *line_str = "0g";
    char *cr;
    int ext_code;

    cmd[2] = sect;
    cmd[3] = page;

    if (!cmd[2]) cmd[2] = "";

    if (line) {
        line_str_len = strlen(line);

        /* 2 extra characters for 'g' and \0 */
        line_str = (char *)malloc(line_str_len + 2);
        cr = line_str;
        cr = stpcpy(cr, line);
        cr = stpcpy(cr, "g");
    }

    endwin();
    ext_code = run_pty(cmd, line_str);

    if (line)
        free(line_str);
    refresh();
    win_clear_all();

    return ext_code;
}

