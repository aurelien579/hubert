#include <types.h>
#include "hubert.h"
#include "user_process.h"

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
#include <msg.h>


struct hubert_process {
    int permanent_queue;
    int mem_key;
    int updating_process_pid;
    struct user *users;
    struct shared_memory *mem;
    sem_t *sem_drivers;
};

static struct hubert_process hubert = { -1, -1, -1, NULL, NULL, NULL };

static void hubert_close(int n);

static void hubert_log(const char *str, ...)
{
    va_list args;

    va_start(args, str);
    printf("Hubert : ");
    vprintf(str, args);
    va_end(args);
}

static int user_del(const char *name)
{
    struct user **cur = &hubert.users;

    while (*cur) {
        if (strncmp((*cur)->name, name, NAME_MAX) == 0) {
            int temp = (*cur)->pid;
            *cur = (*cur)->next;

            return temp;
        }

        cur = &((*cur)->next);
    }

    return -1;
}

static void kill_user(const char *name)
{
    hubert_log("Killing user %s\n", name);
    
    int pid = user_del(name);
    int queue = msgget(pid, 0666);
    
    if (pid < 0) {
        PANIC(hubert_close, "Hubert : unregister");
    }

    kill(pid, SIGKILL);
    msgctl(queue, IPC_RMID, NULL);
}

static void hubert_close(int n)
{
    printf("\rClosing Hubert\n");

    struct user *cur = hubert.users;
    while (cur) {
        kill_user(cur->name);
        cur = cur->next;
    }
    
    if (hubert.mem) {
        for (int i = 0; i < hubert.mem->rests_number; i++) {            int rest_queue = msgget(hubert.mem->restaurants[i].id, 0);
            if (rest_queue >= 0) {                msgctl(rest_queue, IPC_RMID, NULL);
            }
        }
        
        shmdt(hubert.mem);
    }

    if (hubert.mem_key >= 0) {
        shmctl(hubert.mem_key, IPC_RMID, NULL);    }
    
    if (hubert.permanent_queue >= 0) {
        msgctl(hubert.permanent_queue, IPC_RMID, NULL);    }
    
    main_close(0);
}

void hubert_add_restaurant(struct shared_memory *ptr, const char *name, int id)
{
    ptr->restaurants[ptr->rests_number].id = id;
    ptr->restaurants[ptr->rests_number].stock.count = 0;
    strncpy(ptr->restaurants[ptr->rests_number].name, name, NAME_MAX);

    ptr->rests_number++;
}

void hubert_del_restaurant(struct shared_memory *ptr, const char *name)
{
    for (int i = 0; i < ptr->rests_number; i++) {
        if (strcmp(ptr->restaurants[i].name, name) == 0) {
            ptr->restaurants[i] = ptr->restaurants[ptr->rests_number - 1];
            ptr->rests_number--;
        }
    }
}

struct restaurant *hubert_find_restaurant(struct shared_memory *mem, char *name)
{
    for (int i = 0; i < mem->rests_number; i++) {
        if (strcmp(mem->restaurants[i].name, name) == 0) {
            return &mem->restaurants[i];
        }
    }

    return NULL;
}

static int generate_user_id()
{
    static int last_id = USER_FIRST_ID;
    return last_id++;
}

static int generate_rest_id()
{
    static int last_id = REST_FIRST_ID;
    return last_id++;
}

static int user_exists(const char *name)
{
    struct user *cur = hubert.users;
    while (cur) {        if (strncmp(name, cur->name, NAME_MAX) == 0) {            return 1;
        }
        cur = cur->next;
    }
    
    return 0;}

static void set_user_pid(int id, int pid)
{
    struct user *cur = hubert.users;
    while (cur) {
        if (cur->id == id) {
            cur->pid = pid;
        }
        cur = cur->next;
    }}

static int add_user(const char *name)
{
    if (user_exists(name)) {        return -1;
    }
    
    struct user *user = malloc(sizeof(struct user));
    user->id = generate_user_id();
    strncpy(user->name, name, NAME_MAX);
    
    user->next = hubert.users;
    hubert.users = user;
    
    return user->id;
}

void hubert_process(sem_t *sem_mutex)
{
    signal(SIGINT, hubert_close);
    
    hubert.mem_key = shmget(1500, sizeof(struct shared_memory), IPC_CREAT | 0666);
    if (hubert.mem_key < 0) {
        PANIC(hubert_close, "Hubert : fail shmget.");
    }
    
    hubert.mem = shmat(hubert.mem_key, 0, 0);
    if (hubert.mem == (void *) -1) {
        PANIC(hubert_close, "Hubert : fail in oppening shared_memory.");
    }

    hubert.permanent_queue = msgget(UBERT_KEY, IPC_CREAT | 0666);
    if (hubert.permanent_queue < 0) {
        PANIC(hubert_close, "Hubert : Failed to open permanent_queue.");
    }
    
    printf("permanent queue %d\n", hubert.permanent_queue);

    hubert.sem_drivers = sem_open(SEM_DRIVERS, O_CREAT, 0644, NB_DRIVERS);
    if (hubert.sem_drivers == SEM_FAILED) {
        PANIC(hubert_close, "Hubert : sem_init.");
    }

    struct msg_state msg;
    while (1) {
        hubert_log("Reading...\n");
        if (msgrcv(hubert.permanent_queue, &msg, MSG_STATE_SIZE, 0, 0) < 0) {
            printf("Hubert : msgrcv interrupted.\n");
            continue;
        }
        
        /* TODO: Try to remove these lines */
        int type = msg.type;
        msg.type = -1;

        if (type == MSG_USER_CONNECT) {
            hubert_log("RECV MSG_USER_CONNECT\n");
            
            int id = add_user(msg.name);
            send_long(hubert.permanent_queue, MSG_USER_STATUS, id);
            
            if (id > 0) {                int pid = fork();
                if (pid == 0) {
                    signal(SIGINT, SIG_DFL);
                    user_process(id);
                    fprintf(stderr, "[ERROR] User process didn't close.\n");
                    exit(0);
                } else {                    set_user_pid(id, pid);
                }
            }
        } else if (type == MSG_REST_REGISTER) {
            hubert_log("RECV MSG_REST_REGISTER\n");
            hubert_log("old rests : %d\n", hubert.mem->rests_number);
            int id = generate_rest_id();
            
            sem_wait(sem_mutex);
            hubert_log("ok sem_wait\n");
            hubert_add_restaurant(hubert.mem, msg.name, id);
            sem_post(sem_mutex);
            
            struct msg_long cur = {MSG_REST_STATUS, id};
            msgsnd(hubert.permanent_queue, &cur, MSG_SIZE(long), 0);

            for (int i = 0; i < hubert.mem->rests_number; i++) {
                print_rest(&hubert.mem->restaurants[i]);
            }

        } else if (type == MSG_REST_UNREGISTER) {
            hubert_log("RECV MSG_REST_UNREGISTER\n");
            sem_wait(sem_mutex);
            hubert_del_restaurant(hubert.mem, msg.name);
            sem_post(sem_mutex);
        } else if (type == MSG_USER_DISCONNECT) {
            hubert_log("RECV MSG_USER_DISCONNECT\n");
            kill_user(msg.name);
        }
    }
    
    hubert_close(0);
}

