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

static int perm_queue = -1;
static int connected = 0;

void user_log(const char *str, ...);
static int disconnect(int q);

static void client_close(int n)
{
    printf("\n");
    user_log("Closing User");
    
    if (connected) {
        disconnect(perm_queue);
    }
    
    exit(0);
}

void user_log(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    printf("USER : [INFO] ");
    vprintf(str, args);
    printf("\n");
    va_end(args);
}

void user_panic(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    fprintf(stderr, "USER : [PANIC] ");
    vfprintf(stderr, str, args);
    printf("\n");
    client_close(0);
}

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
        printf("Restaurant name : %s\n\n", m[i].name);
        
        printf("aliments disponibles :\n");
        for (int j=0; j < (m[i].foods_count); j++) {
            printf("%s     ", m[i].foods[j]);
        }
        
        printf("\n");
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, client_close);
    
    perm_queue = msgget(HUBERT_KEY, 0);
    if (perm_queue < 0) {
        user_panic("Can't connect to Hubert.");
    }
    
    int status = connect(perm_queue);
    if (status < 0) {
        user_panic("Can't communicate with Hubert.");
    } else if (status != STATUS_OK) {
        user_panic("Can't connect to hubert. Status : %d", status);
    }
    
    connected = 1;
    
    user_log("Connected");
    
    int q = msgget(getpid(), 0);
    if (q < 0) {        user_panic("Can't open dedicated queue.");
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
