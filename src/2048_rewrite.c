#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <termios.h>
#include <unistd.h>
#include "2048_engine.h"

#define iterate(x, expr)\
    do {\
        int i;\
        for (i = 0; i < x; ++i) { expr; }\
    } while (0)

#ifdef HAVE_CURSES
void draw_screen(struct gamestate *g)
{
    static WINDOW *gamewin;
    static size_t wh;
    static size_t ww;

    if (!gamewin) {
        wh = g->opts->grid_height * (g->print_width + 2) + 3;
        ww = g->opts->grid_width * (g->print_width + 2) + 1;
        gamewin = newwin(wh, ww, 1, 1);
        keypad(gamewin, TRUE);
    }

    // mvwprintw will sometimes have a useless arg, this is warned, but doesn't affect the program
    char *scr = g->score_last ? "SCORE: %d (+%d)\n" : "SCORE: %d\n";
    mvwprintw(gamewin, 0, 0, scr, g->score, g->score_last);
    mvwprintw(gamewin, 1, 0, "HISCR: %d\n", g->score_high);

    iterate(g->opts->grid_width*(g->print_width + 2) + 1, waddch(gamewin, '-')); 
    int x, y, xps = 0, yps = 3;
    for (y = 0; y < g->opts->grid_height; y++, xps = 0, yps++) {
        mvwprintw(gamewin, yps, xps++, "|");
        for (x = 0; x < g->opts->grid_width; x++) {
            if (g->grid[x][y]) {
                mvwprintw(gamewin, yps, xps, "%*d", g->print_width, g->grid[x][y]);
                mvwprintw(gamewin, yps, xps + g->print_width, " |");
            }
            else {
                iterate(g->print_width + 1, waddch(gamewin, ' '));
                waddch(gamewin, '|');
            }
            xps += (g->print_width + 2);
        }
    }
    iterate(g->opts->grid_height*(g->print_width + 2) + 1, waddch(gamewin, '-')); 
    wrefresh(gamewin);
}

void drawstate_init(void)
{
    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);
    refresh();
}

void drawstate_clear(void)
{
    endwin();
}

int get_keypress(void)
{
    return getch();
}

#else /* vt100 and standard shared functions */
struct termios sattr;
void drawstate_clear()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &sattr);
}

void drawstate_init(void)
{
    tcgetattr(STDIN_FILENO, &sattr);

    /* alters terminal stdin to not echo and doesn't need \n before reading getchar */
    struct termios tattr;
    tcgetattr(STDIN_FILENO, &tattr);
    tattr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDOUT_FILENO, TCSANOW, &tattr);
}

int get_keypress(void)
{
    return fgetc(stdin);
}

void draw_screen(struct gamestate *g)
{
    /* Clear the screen each draw if we are able to */
#ifdef VT100_COMPATIBLE
    printf("\033[2J\033[H");
#endif
    char *scr = g->score_last ? "SCORE: %d (+%d)\n" : "SCORE: %d\n";
    printf(scr, g->score, g->score_last);
    printf("HISCR: %ld\n", g->score_high);

    // alter this grid_size + 1 to match abitrary grid size
    iterate((g->print_width + 2) * g->opts->grid_width + 1, printf("-"));
    printf("\n");
    int x, y;
    for (y = 0; y < g->opts->grid_height; y++) {
        printf("|");
        for (x = 0; x < g->opts->grid_width; x++) {
            if (g->grid[x][y])
                printf("%*ld |", g->print_width, g->grid[x][y]);
            else
                printf("%*s |", g->print_width, "");
        }
        printf("\n");
    }

    iterate((g->print_width + 2) * g->opts->grid_width + 1, printf("-"));
    printf("\n\n");
}
#endif /* CURSES */

void ddraw(struct gamestate *g)
{
    draw_screen(g);
    usleep(40000);
}

int main(int argc, char **argv)
{
    struct gamestate *g = gamestate_init(
                            parse_options(
                              gameoptions_default(), argc, argv));

    drawstate_init();

    while (1) {
        draw_screen(g);

        int e;
        if ((e = end_condition(g))) {
            drawstate_clear();
            printf(e > 0 ? "You win\n" : "You lose\n");
            goto endloop;
        }

        /* abstract getting keypress */
        int ch;
        do {
            ch = get_keypress();
            if (ch == 'q') { goto endloop; }
        } while (strchr("hjkl", ch) == NULL);

        gamestate_tick(g, ch, g->opts->animate ? ddraw : NULL);
    }
endloop:

    drawstate_clear();
    gamestate_clear(g);
    return 0;
}
