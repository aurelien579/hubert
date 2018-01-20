#include "ui.h"
#include <types.h>
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINES   100



/******************************************************************************
 *                              STRUCTURES
 ******************************************************************************/

enum line_type {
    LINE_REST_NAME,
    LINE_FOOD_NAME,
    LINE_VLINE
};

struct line {
    enum line_type  type;
    char           *content;
    int             quantity;
};

struct ui {
    struct line lines[MAX_LINES];
    int lines_count;
    int cur_line;
    const struct menu *menus;
    int menus_count;
    int running;
    WINDOW *win;
    
    on_refresh_t on_refresh;
    on_close_t on_close;
    on_command_t on_command;
};




/******************************************************************************
 *                          PRIVATE FUNCTIONS
 ******************************************************************************/

static inline void cli_nl(WINDOW *w)
{
    wmove(w, getcury(w) + 1, 0);
}

static WINDOW *cli_init()
{
    int width, height;
    WINDOW *win;
    
    initscr();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);
    noecho();
    
    getmaxyx(stdscr, height, width);
    win = newwin(height, width, 0, 0);
    box(win, 0, 0);
    wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    
    return win;
}

static void cli_center(WINDOW *win, const char *s, int attrs)
{
    int len = strlen(s);
    int max_x = getmaxx(win);
        
    int start = (max_x - len) / 2;
    for (int i = getcurx(win); i < start; i++) {
        waddch(win, ' ');
    }
    
    wattron(win, attrs);
    wprintw(win, "%s\n", s);
    wattroff(win, attrs);
}

static struct line line_new(enum line_type type, const char *s)
{
    struct line l;
    l.type = type;
    l.quantity = 0;
    
    if (s) {
        l.content = malloc(sizeof(char) * strlen(s));
        strcpy(l.content, s);
    } else {
        l.content = NULL;
    }
    
    return l;
}

static void line_free(struct line line)
{
    if (line.content) {
        free(line.content);
    }
}

/**
 * @brief Return true if the user has commanded foods in the restaurant at
 * line n (ie. if one of the foods int this restaurant has a quantity > 0.
 * @param n the line number of the restaurant
 * @return 
 */
static int rest_has_command(struct ui *u, int n)
{
    n++;
    while (u->lines[n].type == LINE_FOOD_NAME) {
        if (u->lines[n].quantity > 0) {
            return 1;
        }
        n++;
    }
    
    return 0;}

static int create_command(struct ui *u, struct command *c)
{
    int n = 0;
    
    for (int i = 0; i < u->lines_count; i++) {        if (u->lines[i].type == LINE_REST_NAME) {
            if (rest_has_command(u, i)) {                
                strncpy(c[n].name, u->lines[i].content, NAME_MAX);
                c[n].count = 0;
                while (u->lines[i + 1].type == LINE_FOOD_NAME) {
                    if (u->lines[i + 1].quantity > 0) {                        
                        strncpy(c[n].foods[c[n].count], u->lines[i + 1].content, NAME_MAX);
                        c[n].quantities[c[n].count] = u->lines[i + 1].quantity;
                        c[n].count++;
                    }
                    i++;
                }
                
                n++;
            }
        }
    }
    
    return n;}

static void add_line(struct ui *self, enum line_type type, const char *s)
{
    self->lines[self->lines_count++] = line_new(type, s);
}

static void line_print(WINDOW *win, struct line l)
{
    switch (l.type) {
    case LINE_VLINE:
        whline(win, ACS_HLINE, getmaxx(win));
        cli_nl(win);
        break;
    case LINE_REST_NAME:
        wattron(win, A_BOLD);    
        wprintw(win, " %s", l.content);
        for (int i = getcurx(win); i < getmaxx(win); i++) waddch(win, ' ');
        wattroff(win, A_BOLD);
        break;
    case LINE_FOOD_NAME:
        wprintw(win, " \t%s", l.content); 
        for (int i = getcurx(win); i < getmaxx(win) - 10; i++) {
            waddch(win, ' ');
        }
        
        wprintw(win, "<- %3d ->", l.quantity);
        
        for (int i = getcurx(win); i < getmaxx(win); i++) {
            waddch(win, ' ');
        }
        
        break;
    }
}

static void cli_update_lines(struct ui *self)
{
    for (int i = 0; i < self->lines_count; i++) {
        line_free(self->lines[i]);
    }
    
    self->lines_count = 0;
    self->cur_line = 2;
    
    for (int i = 0; i < self->menus_count; i++) {
        if (i + self->menus[i].foods_count + 2 < MAX_LINES) {
            add_line(self, LINE_VLINE, NULL);
            add_line(self, LINE_REST_NAME, self->menus[i].name);
            
            for (int j = 0; j < self->menus[i].foods_count; j++) {
                add_line(self, LINE_FOOD_NAME, self->menus[i].foods[j]);
            }
            
            add_line(self, LINE_VLINE, NULL);
        }
    }
}

static void cli_print_menus(struct ui *self)
{
    for (int i = 0; i < self->lines_count; i++) {
        if (i == self->cur_line) {
            wattron(self->win, A_STANDOUT);
            whline(self->win, A_STANDOUT, getmaxx(self->win));
        }
        
        line_print(self->win, self->lines[i]);
        wattroff(self->win, A_STANDOUT);
    }
}

static void cli_print_bottom_menu(WINDOW *w)
{
    wmove(w, getmaxy(w) - 1, 0);
    wprintw(w, "F1: Command, F2: Refresh, F10: Close");}

static void cli_down(struct ui *self)
{
    do {        
        self->cur_line++;
        if (self->cur_line >= self->lines_count) self->cur_line = 0;
    } while (self->lines[self->cur_line].type != LINE_FOOD_NAME);
}

static void cli_up(struct ui *self)
{
    do {
        self->cur_line--;
        if (self->cur_line < 0) self->cur_line = self->lines_count - 1;
    } while (self->lines[self->cur_line].type != LINE_FOOD_NAME);
}

static void cli_right(struct ui *self)
{
    if (self->lines[self->cur_line].type == LINE_FOOD_NAME) {
        self->lines[self->cur_line].quantity++;
    }
}

static void cli_left(struct ui *self)
{
    if (self->lines[self->cur_line].type == LINE_FOOD_NAME) {
        if (self->lines[self->cur_line].quantity > 0)
            self->lines[self->cur_line].quantity--;
    }
}

static void cli_close(struct ui *self)
{
    if (self->on_close)
        self->on_close();
        
    self->running = 0;}

static void cli_command(struct ui *self)
{
    struct command commands[RESTS_MAX];
    
    if (self->on_command) {
        int count = create_command(self, commands);
        self->on_command(commands, count);        
    }}

static void cli_refresh(struct ui *self)
{
    if (self->on_refresh) {
        self->on_refresh();
    }}

static void cli_render(struct ui *self)
{
    clear();
    wmove(self->win, 0, 0);
    
    cli_center(self->win, "=== HUBERT ===", A_BOLD);
    cli_print_menus(self);
    cli_print_bottom_menu(self->win);
    
    refresh();
    wrefresh(self->win);
}

static void cli_run(struct ui *self)
{
    self->win = cli_init();
    self->running = 1;
    int c;
    
    cli_render(self);
    
    while (self->running) {
     
        c = getch();
        switch (c) {
            case KEY_DOWN: cli_down(self); break;
            case KEY_UP: cli_up(self); break;
            case KEY_RIGHT: cli_right(self); break;
            case KEY_LEFT: cli_left(self); break;
            case KEY_F(1): cli_command(self); break;
            case KEY_F(2): cli_refresh(self); break;
            case KEY_F(10): cli_close(self); break;
        }
        
        cli_render(self);
    }
    
    delwin(self->win);
    endwin();
}





/******************************************************************************
 *                          PUBLIC FUNCTIONS
 ******************************************************************************/

struct ui *ui_new()
{
    struct ui *self = malloc(sizeof(struct ui));
    self->lines_count = 0;
    self->cur_line = 0;
    
    self->menus = NULL;
    self->menus_count = 0;
    self->running = 0;
    self->win = NULL;
    
    self->on_close = NULL;
    self->on_command = NULL;
    self->on_refresh = NULL;
    
    return self;}

void ui_start(struct ui *self)
{
    cli_run(self);
    if (self->on_close)
        self->on_close();}

void ui_free(struct ui *self)
{
    free(self);}

void ui_update_menus(struct ui *self, const struct menu *menus, int c)
{
    self->menus = menus;
    self->menus_count = c;
    
    cli_update_lines(self);
    if (self->running)
        cli_render(self);
}

void ui_set_on_refresh(struct ui *self, on_refresh_t f)
{
    self->on_refresh = f;}

void ui_set_on_command(struct ui *self, on_command_t f)
{
    self->on_command = f;
}

void ui_set_on_close(struct ui *self, on_close_t f)
{
    self->on_close = f;
}
