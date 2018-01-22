#include <types.h>
#include <msg.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <error.h>

#include "driver.h"

#define DRIVER_MAX  1

struct rest {
    char    name[NAME_MAX];
    int     key;
    sem_t  *mutex;
};

struct user {
    int key;
    int pid;
};

#define HUBERT_MEM_SEM  "hubert_mem"
#define HUBERT_MEM_KEY  3212
struct hubert_mem {    
    struct rest rests[RESTS_MAX];
    int rests_count;
    struct driver drivers[DRIVER_MAX];};

static int running = 1;
static int perm_queue = -1;
static int hubert_mem_key = -1;

static struct user users[USER_MAX];
static int users_count = 0;
static struct hubert_mem *hubert_mem = NULL;

LOG_FUNCTIONS(hubert)
LOG_FUNCTIONS(user)

void main_close(int n)
{
    printf("\n");
    log_hubert("Closing hubert.");
    
    if (hubert_mem) {        shmdt(hubert_mem);
    }
    
    if (perm_queue >= 0) {
        msgctl(perm_queue, IPC_RMID, 0);
    }
    
    if (hubert_mem_key >= 0) {
        shmctl(hubert_mem_key, IPC_RMID, 0);
    }
    
    for (int i = 0; i < users_count; i++) {        int key = msgget(users[i].key, 0);
        if (key >= 0) {
            msgctl(key, IPC_RMID, 0);
        }
        
        if (users[i].pid > 1) {
            kill(users[i].pid, SIGTERM);            
        }
    }
    
    for (int i = 0; i < DRIVER_MAX; i++) {
        driver_terminate(&hubert_mem->drivers[i]);
    }
    
    sem_unlink(HUBERT_MEM_SEM);
    
    exit(0);
}

PANIC_FUNCTION(hubert, main_close)

static struct driver *get_ready_driver()
{
    for (int i = 0; i < DRIVER_MAX; i++) {
        if (driver_is_ready(&hubert_mem->drivers[i])) {
            return &hubert_mem->drivers[i];
        }
    }
    
    return NULL;
}

static int get_rest_pid(char *name)
{
    struct hubert_mem *mem = shmat(hubert_mem_key, 0, 0);
    if (mem == (void *) -1) {
        return -1;
    }
    
    for (int i = 0; i < mem->rests_count; i++) {
        if (strncmp(mem->rests[i].name, name, NAME_MAX) == 0) {
            return mem->rests[i].key;
        }
    }
    
    return 0;
}

int connect_rest(int key)
{
    if (hubert_mem->rests_count >= RESTS_MAX) {
        return STATUS_FULL;
    }
    
    for (int i = 0; i < hubert_mem->rests_count; i++) {
        if (hubert_mem->rests[i].key == key) {
            return STATUS_EXISTS;
        }
    }
    
    int n = hubert_mem->rests_count;
    
    char key_str[NAME_MAX];
    sprintf(key_str, "%d", key);
    hubert_mem->rests[n].mutex = sem_open(key_str, 0);
    if (hubert_mem->rests[n].mutex == SEM_FAILED) {        return -1;
    }
    
    int rest_key = shmget(key, sizeof(struct menu), 0);
    if (rest_key < 0) {
        log_user_error("Can't get shared memory of rest %d", key);
        return -1;
    }
    
    struct menu *rest_menu = shmat(rest_key, 0, 0);
    if (rest_menu == (void *) -1) {
        log_user_error("Can't attach shared memory of rest %d", key);
        return -1;
    }
    
    strncpy(hubert_mem->rests[n].name, rest_menu->name, NAME_MAX);
    shmdt(rest_menu);
    
    hubert_mem->rests[n].key = key;
    hubert_mem->rests_count++;
    
    return STATUS_OK;
}

void disconnect_rest(int key)
{
    for (int i = 0; i < hubert_mem->rests_count; i++) {
        if (hubert_mem->rests[i].key == key) {
            hubert_mem->rests[i] = hubert_mem->rests[--hubert_mem->rests_count];
        }
    }
}

int connect_user(int key)
{
    if (users_count >= USER_MAX) {
        return STATUS_FULL;
    }
    
    for (int i = 0; i < users_count; i++) {
        if (users[i].key == key) {
            return STATUS_EXISTS;
        }
    }
    
    users[users_count++].key = key;
    return STATUS_OK;
}

void disconnect_user(int key)
{
    for (int i = 0; i < users_count; i++) {
        if (users[i].key == key) {
            users[i] = users[--users_count];
        }
    }
}

void set_user_pid(int key, int pid)
{
    for (int i = 0; i < users_count; i++) {
        if (users[i].key == key) {
            users[i].pid = pid;
        }
    }
}

void send_status(int dest, int status)
{
    struct msg_status msg = { dest, status };
    msgsnd(perm_queue, &msg, MSG_STATUS_SIZE, 0);
}

static int send_menu(int q, int dest)
{   
    struct msg_menus menus;
    int count = 0;
    
    for (int i = 0; i < hubert_mem->rests_count; i++) {        int rest_key = shmget(hubert_mem->rests[i].key, sizeof(struct menu), 0);
        if (rest_key < 0) {
            log_user_error("Can't get shared memory of rest %d",
                           hubert_mem->rests[i].key);
            continue;
        }
        
        struct menu *rest_menu = shmat(rest_key, 0, 0);
        if (rest_menu == (void *) -1) {
            log_user_error("Can't attach shared memory of rest %d",
                           hubert_mem->rests[i].key);
            continue;
        }
        
        sem_wait(hubert_mem->rests[i].mutex);
        menus.menus[i] = *rest_menu;
        sem_post(hubert_mem->rests[i].mutex);
        
        count++;
        
        shmdt(rest_menu);
    }
    
    menus.menus_count = count;
    menus.dest = dest;

    if (msgsnd(q, &menus, MSG_MENUS_SIZE, 0) < 0) {
        return 0;    }
    
    return 1;}

static int user_recv_command(int q, struct command *cmd)
{
    log_hubert("user_recv_command");
    struct msg_command msg;
    if (msgrcv(q, &msg, MSG_COMMAND_SIZE, HUBERT_DEST, 0) < 0) {
        return 0;
    }
    
    *cmd = msg.command;
    return 1;
}

static int send_command_to_rest(int pid, struct command *cmd)
{
    log_hubert("send_command_to_rest %d", cmd->count);
    struct msg_command msg = { pid, *cmd };
    if (msgsnd(perm_queue, &msg, MSG_COMMAND_SIZE, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}

static int recv_command_status_from_rest(int user_id, struct msg_command_status *msg)
{
    if (msgrcv(perm_queue, msg, MSG_STATUS_COMMAND_SIZE, user_id, 0) < 0) {
        return 0;
    } else {
        log_hubert("recv_command_status_from_rest %d", msg->status);
        return 1;
    }    
}

static int send_command_status_to_user(int queue, struct msg_command_status *msg)
{
    log_hubert("send_command_status_to_user %d", msg->status);
    if (msgsnd(queue, msg, MSG_STATUS_COMMAND_SIZE, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}

/**
 * @brief Deliver the command to the user
 */
static void deliver_command(struct command *cmd)
{
    struct driver *driver = NULL;
    do {        
        driver = get_ready_driver();
        sleep(1);
    } while (driver == NULL);
    
    driver_deliver(driver, *cmd);
}

/* TODO: Correct memory leak (make a user_close function that close the mutex */
static void user(int key)
{
    int user_q = msgget(key, 0);
    
    if (user_q < 0) {        log_user_error("Can't open user queue");
        return;
    }
    
    struct msg_request request;
    
    while (1) {
        if (msgrcv(user_q, &request, MSG_REQUEST_SIZE, HUBERT_DEST, 0) < 0) {
            log_user_error("Error while reading");
            continue;
        }
        
        switch (request.type) {        case MENU_REQUEST:
            log_user("MENU_REQUEST");
            if (!send_menu(user_q, key)) {                log_user_error("Error while sending menus");
            }
            break;
        case COMMAND_REQUEST:
            log_user("COMMAND_REQUEST");
            struct command cmd;
            if (!user_recv_command(user_q, &cmd)) {
                log_user_error("user_recv_command error");
                break;
            }
            
            int rest_pid = get_rest_pid(cmd.name);
            if (rest_pid == 0) {
                log_user_error("Can't find rest %s", cmd.name);
                break;
            } else if (rest_pid < 0) {
                log_user_error("Error while getting rest pid");
                break;
            }
            
            if (!send_command(perm_queue, rest_pid, &cmd)) {
                log_user_error("send_command_to_rest");
                break;
            }
            
            
            /* COMMAND_RECEIVED STATUS */
            struct msg_command_status msg;
            if (!recv_command_status(perm_queue, cmd.user_pid, &msg)) {
                log_user_error("recv_command_status_from_rest");
                break;
            }
            
            if (!send_command_status(user_q, cmd.user_pid, msg.status, msg.time, msg.name)) {
                log_user_error("send_command_status_to_user");
                break;
            }
            
            /* COMMAND_START STATUS */
            if (!recv_command_status(perm_queue, cmd.user_pid, &msg)) {
                log_user_error("recv_command_status_from_rest");
                break;
            }
            
            if (!send_command_status(user_q, cmd.user_pid, msg.status, msg.time, msg.name)) {
                log_user_error("send_command_status_to_user");
                break;
            }
            
            /* COMMAND_COOKED STATUS */
            if (!recv_command(perm_queue, cmd.user_pid, &msg)) {
                log_user_error("recv_command_status_from_rest");
                break;
            }
            
            deliver_command(&cmd);
            
            break;
        }    }}

int main(int argc, char **argv)
{
    signal(SIGINT, main_close);
    perm_queue = msgget(HUBERT_KEY, IPC_CREAT | 0666);
    if (perm_queue < 0) {
        hubert_panic("Can't open perm_queue");
    }
    
    hubert_mem_key = shmget(HUBERT_MEM_KEY, sizeof(struct hubert_mem), IPC_CREAT | 0666);
    if (hubert_mem_key < 0) {
        hubert_panic("Can't get shared memory key");    }
    
    hubert_mem = shmat(hubert_mem_key, 0, 0);
    if (hubert_mem == (void *) -1) {        hubert_panic("Can't attach shared memory");
    }
    
    for (int i = 0; i < DRIVER_MAX; i++) {
        driver_new(&hubert_mem->drivers[i]);
    }
    
    log_hubert("Starting...");
    struct msg_state state;
    while (running) {
        if (msgrcv(perm_queue, &state, MSG_STATE_SIZE, HUBERT_DEST, 0) < 0) {
            log_hubert_error("Can't receive message");
            continue;
        }
        
        int status = 0;
        switch (state.type) {
        case USER_CONNECT:
            log_hubert("Connecting user %d", state.key);
            status = connect_user(state.key);

            if (msgget(state.key, IPC_CREAT | 0666) < 0) {                log_hubert("Can't create user queue.");
                send_status(state.key, STATUS_ERROR);
                break;
            }
            
            if (status == STATUS_OK) {
                int pid = fork();
                if (pid == 0) {
                    signal(SIGINT, SIG_DFL);
                    user(state.key);
                    return 0;
                }
                
                set_user_pid(state.key, status);
            }
            
            send_status(state.key, status);            
            break;
        case USER_DISCONNECT:
            log_hubert("Disconnecting user %d", state.key);
            
            disconnect_user(state.key);
            break;
        case REST_CONNECT:
            log_hubert("Connecting restaurant %d", state.key);
            status = connect_rest(state.key);
            if (status < 0) {                log_hubert("Can't connect restaurant %d", state.key);
            }
            
            send_status(state.key, status);
            break;
        case REST_DISCONNECT:
            log_hubert("Disconnecting restaurant %d", state.key);
            disconnect_rest(state.key);
            break;
        }
    }
    
    return 0;
}
