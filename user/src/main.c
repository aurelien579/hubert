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

void user_log(const char *str, ...);

static void client_close(int n)
{
    user_log("\rClosing User");
    
    /*if (connected) {
        int queue = msgget(UBERT_KEY, 0666);
        if (queue >= 0) {
            struct msg_state disconnect_msg;
            disconnect_msg.type = MSG_USER_DISCONNECT;
            strncpy(disconnect_msg.name, username, NAME_MAX);
            
            msgsnd(queue, &disconnect_msg, MSG_STATE_SIZE, 0);            
        }
    }*/
    
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
    msgsnd(q, &msg, MSG_STATE_SIZE, 0);
    
    struct msg_status status;
    msgrcv(q, &status, MSG_STATUS_SIZE, key, 0);
    
    return status.status;
}

static void disconnect(int q)
{    
    struct msg_state msg = { HUBERT_DEST, USER_DISCONNECT, getpid() };
    msgsnd(q, &msg, MSG_STATE_SIZE, 0);
}

static void ask_menu(int queue, struct menu *m)
{
    
}

void main(int argc, char **argv)
{
    signal(SIGINT, client_close);
    
    int perm_queue = msgget(HUBERT_KEY, 0);
    if (perm_queue <= 0) {
        user_panic("Can't connect to Hubert.");
    }
    
    int status = connect(perm_queue);
    if (status != STATUS_OK) {
        user_panic("Can't connect to hubert. Status : %d", status);
    }
    
    user_log("Connected");
    
    sleep(1);
    
    disconnect(perm_queue);
    
    client_close(0);
}
