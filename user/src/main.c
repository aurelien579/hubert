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

#include "ui.h"

#define LOG_OUT         OUT_FILE
#include <error.h>

static int perm_queue = -1;
static int connected = 0;

static void client_close(int n);

LOG_FUNCTIONS(user)
PANIC_FUNCTION(user, client_close)

static struct menu menus[RESTS_MAX];
static int menus_count = 0;
static struct ui *ui;


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

static void client_close(int n)
{
    log_user("Closing User");
    
    if (connected) {
        disconnect(perm_queue);
    }
    
    exit(0);
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

static void on_command(struct command *c, int count)
{
    log_user("Command");
    for (int i = 0; i < count; i++) {
        fprintf(log_file, "\t\tCommand in : %s\n", c[i].name);
        for (int j = 0; j < c[i].count; j++) {
            fprintf(log_file, "\t\t\tFood : %s %d\n", c[i].foods[j], c[i].quantities[j]);
        }
    }}

int main(int argc, char **argv)
{
    log_file = fopen("user.log", "a");
    
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
    
    ui = ui_new();
    ui_update_menus(ui, menus, menus_count);
    ui_set_on_command(ui, on_command);
    ui_start(ui);
    ui_free(ui);
    return 0;
    
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
