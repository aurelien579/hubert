#include <types.h>
#include "hubert_process.h"
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

#define START_ID    2010

static struct user *g_users = NULL;
static int g_updating_process_pid = -1;

static int permanent_queue;
static sem_t *sem_drivers;
static int mem_key;

static void on_close(int n);

static void hubert_log(const char *str, ...)
{
    va_list args;

    va_start(args, str);
    printf("Hubert : ");
    vprintf(str, args);
    va_end(args);
}


static int user_del(struct user **list, const char *name)
{
    struct user **cur = list;

    while (*cur) {
        if (strncmp((*cur)->name, name, NAME_MAX) == 0) {
            int temp = (*cur)->id;
            *cur = (*cur)->next;

            return temp;
        }

        cur = &((*cur)->next);
    }

    return -1;
}

static void kill_user(struct user **users, const char *name)
{
    hubert_log("Killing user %s\n", name);
    
    int pid = user_del(users, name);
    int queue = msgget(pid, 0666);
    
    if (pid < 0) {
        PANIC("Hubert : unregister");
    }
	
    kill(pid, SIGKILL);
    msgctl(queue, IPC_RMID, NULL);
}

static void on_close(int n)
{
    shmctl(mem_key, IPC_RMID, NULL);

    printf("\rClosing Hubert\n");

    if (g_updating_process_pid > 0) {
        kill(g_updating_process_pid, SIGTERM);
    }

    struct user *cur = g_users;
    while (cur) {
        kill_user(&g_users, cur->name);
        cur = cur->next;
    }
    msgctl(permanent_queue, IPC_RMID, NULL);
    sem_close(sem_mutex);
    sem_unlink(SEM_MUTEX);
    exit(0);
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

static void add_user(struct user **users, const char *name, int id)
{
    struct user *user = malloc(sizeof(struct user));
    user->id = id;
    strncpy(user->name, name, NAME_MAX);
    user->next = *users;

    *users = user;
}



void hubert_process(int updating_process_pid)
{
    mem_key = shmget(1500, sizeof(struct shared_memory), IPC_CREAT | 0666);
    struct shared_memory *mem = shmat(mem_key, 0, 0);
    g_updating_process_pid = updating_process_pid;

    signal(SIGINT, on_close);

    permanent_queue = msgget(UBERT_KEY, IPC_CREAT | 0666);

    if (permanent_queue < 0) {
        PANIC("Hubert : msgget");
    }

    sem_drivers = malloc(sizeof(sem_t));
    sem_t *sem_drivers = sem_open(SEM_DRIVERS, O_CREAT, 0644, NB_DRIVERS);
    if (sem_drivers == SEM_FAILED) {
        PANIC("Hubert : sem_init");
    }

    struct msg_state msg;

    while (1) {
        hubert_log("Reading...\n");
        msgrcv(permanent_queue, &msg, NAME_MAX * sizeof(char) + sizeof(long), 0, 0); // a tester option choisi
        int type = msg.type;
        msg.type = -1;

        if (type == MSG_USER_CONNECT) {
            hubert_log("RECV MSG_USER_CONNECT\n");

            int pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                user_process(permanent_queue, getpid(), msg.name);
                exit(0);
            } else {
                hubert_log("pid : %d\n", pid);
                add_user(&g_users, msg.name, pid);
            }
        } else if (type == MSG_REST_REGISTER) {
            hubert_log("RECV MSG_REST_REGISTER\n");
            hubert_log("old rests : %d\n", mem->rests_number);
            int id = mem->rests_number + 1 + START_ID;
            
            sem_wait(sem_mutex);
            hubert_log("ok sem_wait\n");
            hubert_add_restaurant(mem, msg.name, id);
            sem_post(sem_mutex);
            
            struct msg_long cur = {MSG_REST_STATUS, id};
            msgsnd(permanent_queue, &cur, MSG_SIZE(long), 0);

            for (int i = 0; i < mem->rests_number; i++) {
                print_rest(&mem->restaurants[i]);
            }

        } else if (type == MSG_REST_UNREGISTER) {
            hubert_log("RECV MSG_REST_UNREGISTER\n");
            sem_wait(sem_mutex);
            hubert_del_restaurant(mem, msg.name);
            sem_post(sem_mutex);
        } else if (type == MSG_USER_DISCONNECT) {
            hubert_log("RECV MSG_USER_DISCONNECT\n");
            kill_user(&g_users, msg.name);
        }
    }

    shmdt(mem);
    sem_destroy(sem_drivers);
    msgctl(permanent_queue, IPC_RMID, NULL);

    on_close(0);
}

