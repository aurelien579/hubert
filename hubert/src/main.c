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

struct rest {
    int     key;
    sem_t  *mutex;
};

struct user {
    int key;
    int pid;
};

static int running = 1;
static int perm_queue = -1;

static struct user users[USER_MAX];
static int users_count = 0;

static struct rest rests[RESTS_MAX];
static int rests_count = 0;

void main_close(int n)
{    
    exit(0);
}

void hubert_log(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    printf("HUBERT : [INFO] ");
    vprintf(str, args);
    printf("\n");
    va_end(args);
}

void hubert_panic(const char *str)
{    
    fprintf(stderr, "HUBERT : [PANIC] %s\n", str);
    main_close(0);
}


int connect_rest(int key)
{
    if (rests_count >= RESTS_MAX) {
        return STATUS_FULL;
    }
    
    for (int i = 0; i < rests_count; i++) {
        if (rests[i].key == key) {
            return STATUS_EXISTS;
        }
    }
    
    rests[rests_count++].key = key;
    return STATUS_OK;
}

void disconnect_rest(int key)
{
    for (int i = 0; i < rests_count; i++) {
        if (rests[i].key == key) {
            rests[i] = rests[--rests_count];
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


int main(int argc, char **argv)
{    
    perm_queue = msgget(HUBERT_KEY, IPC_CREAT | 0666);
    if (perm_queue <= 0) {
        hubert_panic("Can't open perm_queue");
    }
    
    struct msg_state state;
    while (running) {
        hubert_log("Reading...");
        if (msgrcv(perm_queue, &state, MSG_STATE_SIZE, HUBERT_DEST, 0) < 0) {
            fprintf(stderr, "HUBERT : [ERROR] msgrcv error\n");
            continue;
        }
        
        switch (state.type) {
        case USER_CONNECT:
            hubert_log("Connecting user %d", state.key);
            int status = connect_user(state.key);
            if (status == STATUS_OK) {
                
            }
            
            send_status(state.key, status);
            break;
        case USER_DISCONNECT:
            hubert_log("Disconnecting user %d", state.key);
            
            disconnect_user(state.key);
            break;
        case REST_CONNECT:
            hubert_log("Connecting restaurant %d", state.key);
            send_status(state.key, connect_rest(state.key));
            break;
        case REST_DISCONNECT:
            hubert_log("Disconnecting restaurant %d", state.key);
            disconnect_rest(state.key);
            break;
        }
    }
    
    return 0;
}
