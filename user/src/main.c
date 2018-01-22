#include <types.h>
#include <msg.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <stdarg.h>

#include "ui.h"

#define LOG_OUT         OUT_FILE
#include <error.h>

struct menu menus[RESTS_MAX];
int menus_count = 0;
sem_t *mutex = NULL;
int command_progress = 0;

static int user_queue = -1;
static int connected = 0;
static struct ui *ui;
static pthread_t thread;

static void client_close(int n);

LOG_FUNCTIONS(user)
PANIC_FUNCTION(user, client_close)

static int send_connect(int queue)
{
    int key = getpid();
    
    struct msg_state msg = { HUBERT_DEST, USER_CONNECT, key };
    if (msgsnd(queue, &msg, MSG_STATE_SIZE, 0) < 0) {
        return -1;
    }
    
    struct msg_status status;
    if (msgrcv(queue, &status, MSG_STATUS_SIZE, key, 0) < 0) {
        return -1;
    }
    
    return status.status;
}

static int connect()
{    
    log_user("Connecting...");
    int perm_queue = msgget(HUBERT_KEY, 0);
    if (perm_queue < 0) {
        log_user_error("Can't connect to Hubert");
    }
    
    int status = send_connect(perm_queue);
    if (status < 0) {
        log_user_error("Can't communicate with Hubert");
        return 0;
    } else if (status != STATUS_OK) {
        log_user_error("Can't connect to hubert. Status : %d", status);
        return 0;
    }
    
    user_queue = msgget(getpid(), 0);
    if (user_queue < 0) {
        return 0;
        log_user_error("Can't open dedicated queue");
    }
    
    connected = 1;
    
    log_user("Connected");
    
    return 1;
}

static int disconnect()
{
    int perm_queue = msgget(HUBERT_KEY, 0);
    if (perm_queue < 0) {
        log_user_error("Can't open perm queue");
    }
    
    struct msg_state msg = { HUBERT_DEST, USER_DISCONNECT, getpid() };
    if (msgsnd(perm_queue, &msg, MSG_STATE_SIZE, 0) < 0) {
        user_queue = -1;
        connected = 0;
        return 0;
    }
    
    connected = 0;
    user_queue = -1;
    return 1;
}

static void client_close(int n)
{
    log_user("Closing User");
    
    if (connected) {
        disconnect();
    }
    
    if (ui) {
        ui_stop(ui);
        
        pthread_join(thread, NULL);
    }
    
    if (mutex) {
        sem_destroy(mutex);
        char sem_name[NAME_MAX];
        sprintf(sem_name, "%d", getpid());
        sem_unlink(sem_name);
    }
    
    exit(0);
}

static int get_menus(struct menu m[], int *count)
{
    struct msg_request request = { HUBERT_DEST, MENU_REQUEST };
    if (msgsnd(user_queue, &request, MSG_REQUEST_SIZE, 0) < 0) {
        return 0;
    }
    
    struct msg_menus menus;
    if (msgrcv(user_queue, &menus, MSG_MENUS_SIZE, getpid(), 0) < 0) {
        return 0;
    }
        
    memcpy(m, menus.menus, menus.menus_count * sizeof(struct menu));
    (*count) = menus.menus_count;
    
    return 1;
}

static void print_menus(struct menu m[], int n)
{
    for (int i = 0; i < n; i++ ) {
        log_user("Restaurant name : %s\n", m[i].name);
        
        log_user("aliments disponibles :\n");
        for (int j=0; j < (m[i].foods_count); j++) {
            log_user("%s     ", m[i].foods[j]);
        }
        
        log_user("\n");
    }
}

static int send_command(const struct command *c)
{
    if (!msg_send_request(user_queue, HUBERT_DEST, COMMAND_REQUEST)) {
        log_user_error("Can't send request to hubert");
        return 0;
    }
       
    if (!msg_send_command(user_queue, HUBERT_DEST, c)) {
        log_user_error("Can't send command to huber");
        return 0;
    }
    
    return 1;
}

static void on_connect()
{
    if (connected) {
        disconnect();
    }
    
    if (!connect()) {
        ui_set_state(ui, DISCONNECTED);
    } else {
        ui_set_state(ui, CONNECTED);
    }
}

static int recv_command_status(char *rest, int *status, int *time)
{
    struct msg_command_status msg;
    if (msgrcv(user_queue, &msg, MSG_STATUS_COMMAND_SIZE, getpid(), 0) < 0) {
        log_user_error("Can't recv command status");
        return 0;
    }
    
    strncpy(rest, msg.rest_name, NAME_MAX);
    *status = msg.status;
    *time = msg.time;
    
    return 1;
}

static void on_command(struct command *c, int count)
{
    log_user("Command");
    for (int i = 0; i < count; i++) {
        fprintf(log_file, "\t\tCommand in : %s\n", c[i].name);
        for (int j = 0; j < c[i].count; j++) {
            fprintf(log_file, "\t\t\tFood : %s %d\n", c[i].foods[j], c[i].quantities[j]);
        }
    }
    
    for (int i = 0; i < count; i++) {
        if (send_command(&c[i]) < 0) {
            ui_set_state(ui, DISCONNECTED);
        }
    }
    
    char name[NAME_MAX];
    int status;
    int time;
    for (int i = 0; i < count * 4; i++) {
        if (recv_command_status(name, &status, &time)) {
            ui_set_command_status(ui, name, status, time);
        }
    }
    
    log_user("Command DONE");}

static void on_refresh()
{    
    if (!get_menus(menus, &menus_count)) {
        ui_set_state(ui, DISCONNECTED);
        log_user_error("Can't get menus from hubert");
    }
    
    print_menus(menus, menus_count);
    
    ui_update_menus(ui);
}

static void on_close()
{
    client_close(0);
}

static void *ui_thread(void *data)
{

    
    return NULL;
}

int main(int argc, char **argv)
{
    log_file = fopen("user.log", "a");

    if (log_file == NULL) {
        fprintf(stderr, "[FATAL] Can't open log file\n");
        return -1;
    }
    
    signal(SIGINT, client_close);
       
    char sem_name[NAME_MAX];
    sprintf(sem_name, "%d", getpid());
    mutex = sem_open(sem_name, O_CREAT | O_EXCL, 0666, 1);
    if (mutex == SEM_FAILED) {
        user_panic("Can't open sempahore");        
    }
    
    ui = ui_new();
    ui_set_state(ui, DISCONNECTED);
    ui_set_on_command(ui, on_command);
    ui_set_on_refresh(ui, on_refresh);
    ui_set_on_connect(ui, on_connect);
    ui_set_on_close(ui, on_close);
    ui_start(ui);

    ui_free(ui);
    /*if (pthread_create(&thread, NULL, ui_thread, NULL) < 0) {
        user_panic("Can't create ui thread");
    }    
    
    
    while (1) {
        
    }*/
    
    client_close(0);

    return 0;
}
