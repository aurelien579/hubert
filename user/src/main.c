#include <types.h>
#include <msg.h>

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>

#define LOG_OUT         OUT_FILE
#include <error.h>

static int perm_queue = -1;
static int connected = 0;

static int disconnect(int q);

LOG_FUNCTIONS(user)

static void client_close(int n)
{
    log_user("Closing User");
    
    if (connected) {
        disconnect(perm_queue);
    }
    
    exit(0);
}

PANIC_FUNCTION(user, client_close)

struct menu menus[RESTS_MAX];
int menus_count = 0;

static int connect(int q)
{
    int key = getpid();
    
    struct msg_state msg = { HUBERT_DEST, USER_CONNECT, key };
    if (msgsnd(q, &msg, MSG_STATE_SIZE, 0) < 0) {
        return -1;
    }
    
    struct msg_status status;
    if (msgrcv(q, &status, MSG_STATUS_SIZE, key, 0) < 0) {
        return -1;
    }
    
    return status.status;
}

static int disconnect(int q)
{    
    struct msg_state msg = { HUBERT_DEST, USER_DISCONNECT, getpid() };
    if (msgsnd(q, &msg, MSG_STATE_SIZE, 0) < 0) {
        return 0;
    }
    return 1;
}

static int get_menus(int q, struct menu m[], int *count)
{
    struct msg_request request = { HUBERT_DEST, MENU_REQUEST };
    if (msgsnd(q, &request, MSG_REQUEST_SIZE, 0) < 0) {
        return 0;
    }
    
    struct msg_menus menus;
    if (msgrcv(q, &menus, MSG_MENUS_SIZE, getpid(), 0) < 0) {
        return 0;
    }
        
    memcpy(m, menus.menus, menus.menus_count * sizeof(struct menu));
    (*count) = menus.menus_count;
    
    return 1;
}

static void print_menus(struct menu m[], int n)
{
    for (int i=0; i < n; i++ ) {
        log_user("Restaurant name : %s\n", m[i].name);
        
        log_user("aliments disponibles :\n");
        for (int j=0; j < (m[i].foods_count); j++) {
            log_user("%s     ", m[i].foods[j]);
        }
        
        log_user("\n");
    }
}

#define MAX_LINES   100

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

static struct line lines[MAX_LINES];
static int lines_count = 0;
static int cur_line = 0;

static inline void cli_nl(WINDOW *w)
{
    wmove(w, getcury(w) + 1, 0);}

WINDOW *cli_init()
{
    int width, height;
    WINDOW *win;
    
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    noecho();
    
    getmaxyx(stdscr, height, width);
    win = newwin(height, width, 0, 0);
    box(win, 0, 0);
    wborder(win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    
    return win;}

void cli_center(WINDOW *win, const char *s, int attrs)
{
    int len = strlen(s);
    int cur_y = getcury(win);
    int max_x = getmaxx(win);
        
    int start = (max_x - len) / 2;
    wmove(win, cur_y, start);
    wattron(win, attrs);
    wprintw(win, "%s\n", s);
    wattroff(win, attrs);
}

struct line line_new(enum line_type type, const char *s)
{
    struct line l;
    l.type = type;
    l.quantity = 0;
    
    if (s) {        l.content = malloc(sizeof(char) * strlen(s));
        strcpy(l.content, s);
    } else {        l.content = NULL;
    }
    
    return l;}

void line_free(struct line line)
{
    if (line.content) {        free(line.content);
    }}

void add_line(enum line_type type, const char *s)
{
    lines[lines_count++] = line_new(type, s);}

void line_print(WINDOW *win, struct line l)
{
    switch (l.type) {    case LINE_VLINE:
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
        for (int i = getcurx(win); i < getmaxx(win) - 8; i++) {
            waddch(win, ' ');
        }
        
        wprintw(win, "<- %d ->", l.quantity);
        
        for (int i = getcurx(win); i < getmaxx(win); i++) {
            waddch(win, ' ');
        }
        
        break;
    }}

void cli_update_lines()
{
    for (int i = 0; i < lines_count; i++) {        line_free(lines[i]);
    }
    
    lines_count = 0;
    cur_line = 2;
    
    for (int i = 0; i < menus_count; i++) {
        if (i + menus[i].foods_count + 2 < MAX_LINES) {
            add_line(LINE_VLINE, NULL);
            add_line(LINE_REST_NAME, menus[i].name);
            
            for (int j = 0; j < menus[i].foods_count; j++) {
                add_line(LINE_FOOD_NAME, menus[i].foods[j]);
            }
            
            add_line(LINE_VLINE, NULL);        }    }}


void cli_print_menus(WINDOW *win)
{    for (int i = 0; i < lines_count; i++) {
        if (i == cur_line) {
            wattron(win, A_STANDOUT);            whline(win, A_STANDOUT, getmaxx(win));
        }
        line_print(win, lines[i]);
        
        wattroff(win, A_STANDOUT);
    }
    
    refresh();
    wrefresh(win);
}

void cli_down()
{
    do {        
        cur_line++;
        if (cur_line >= lines_count) cur_line = 0;    } while (lines[cur_line].type != LINE_FOOD_NAME);}

void cli_up()
{
    do {
        cur_line--;
        if (cur_line < 0) cur_line = lines_count - 1;
    } while (lines[cur_line].type != LINE_FOOD_NAME);
}

void cli_right()
{
    if (lines[cur_line].type == LINE_FOOD_NAME) {        lines[cur_line].quantity++;
    }}

void cli_left()
{
    if (lines[cur_line].type == LINE_FOOD_NAME) {
        lines[cur_line].quantity--;
    }}

void cli_render(WINDOW *win)
{
    clear();
    wmove(win, 0, 0);
    cli_center(win, "=== HUBERT ===", A_BOLD);
    cli_print_menus(win); }

void cli_ui(int q)
{
    WINDOW *win = cli_init();
    int running = 1;
    int c;
    
    //clear();    
    
    cli_update_lines();   
    
    cli_render(win);
    while (running) {
        
        c = getch();
        switch (c) {
        case KEY_DOWN:
            cli_down();
            break;
        case KEY_UP:
            cli_up();
            break;
        case KEY_RIGHT:
            cli_right();
            break;
        case KEY_LEFT:
            cli_left();
            break;
        }
        cli_render(win);    }
    
    delwin(win);
    endwin();
}

int main(int argc, char **argv)
{
    menus_count++;
    strcpy(menus[0].name, "Name test");
    menus[0].foods_count = 3;
    strcpy(menus[0].foods[0], "Pizza !");
    strcpy(menus[0].foods[1], "Carrotes !");
    strcpy(menus[0].foods[2], "Langoustes !");
    
    menus_count++;
    strcpy(menus[1].name, "OTacos");
    menus[1].foods_count = 3;
    strcpy(menus[1].foods[0], "Pizza !");
    strcpy(menus[1].foods[1], "Tacos !");
    strcpy(menus[1].foods[2], "Kebab !");
    cli_ui(0);
    log_file = fopen("user.log", "a");
    if (log_file == NULL) {
        fprintf(stderr, "[FATAL] Can't open log file\n");
        return -1;
    }
    
    signal(SIGINT, client_close);
    
    perm_queue = msgget(HUBERT_KEY, 0);
    if (perm_queue < 0) {
        user_panic("Can't connect to Hubert");
    }
    
    int status = connect(perm_queue);
    if (status < 0) {
        user_panic("Can't communicate with Hubert");
    } else if (status != STATUS_OK) {
        user_panic("Can't connect to hubert. Status : %d", status);
    }
    
    connected = 1;
    
    log_user("Connected");
    
    int q = msgget(getpid(), 0);
    if (q < 0) {        user_panic("Can't open dedicated queue");
    }
    
    int count = 0;
    struct menu m[RESTS_MAX];
    if (!get_menus(q, m, &count)) {
        user_panic("Can't get menu");
    }
    
    print_menus(m, count);
    client_close(0);    
    return 0;
}
