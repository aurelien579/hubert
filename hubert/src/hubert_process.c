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


static struct user *g_users = NULL;
static int g_updating_process_pid = -1;

static int permanent_queue;
static sem_t *sem_drivers;
static int mem_key;

static void on_close(int n)
{
    shmctl(mem_key, IPC_RMID, NULL);

    
    if (g_updating_process_pid > 0) {
        kill(g_updating_process_pid, SIGTERM);
    }

    struct user *cur = g_users;
    while (cur) {
        printf("Disconnecting %s\n", cur->name);
        msgctl(cur->id, IPC_RMID, NULL);
        cur = cur->next;
    }
    /* TODO: Disconnect users */

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
    int pid = user_del(users, name);    

    if (pid < 0) {
        PANIC("unregister");
    }

    kill(pid, SIGKILL);
}

void hubert_process(int updating_process_pid)
{   
    sem_t *sem_mutex = sem_open(SEM_MUTEX, 0);
    
    mem_key = shmget(1500, sizeof(struct shared_memory), IPC_CREAT | 0666);
    struct shared_memory *mem = shmat(mem_key, 0, 0); 
    g_updating_process_pid = updating_process_pid;
    
    signal(SIGINT, on_close);

    hubert_add_restaurant(mem, "Name test", 45);
    sem_wait(sem_mutex);
    struct restaurant *rest= hubert_find_restaurant(mem, "Name test");
    sem_post(sem_mutex);
    rest->stock.count = 2;
    strcpy(rest->stock.foods[0].name, "patate");
    rest->stock.foods[0].quantity = 10;
    strcpy(rest->stock.foods[1].name, "chips");
    rest->stock.foods[1].quantity = 250000;

    permanent_queue = msgget(UBERT_KEY, IPC_CREAT | 0666);

    if (permanent_queue < 0) {
        PANIC("msgget");
    }

    sem_drivers = malloc(sizeof(sem_t));
    sem_t *sem_drivers = sem_open(SEM_DRIVERS, O_CREAT, 0644, NB_DRIVERS);
    if ( sem_drivers == SEM_FAILED) {
        PANIC("sem_init");
    }

    struct msg_state msg;

    while (1) {
        printf("Reading...\n");
        msgrcv(permanent_queue, &msg, NAME_MAX * sizeof(char) + sizeof(long), 0, 0); // a tester option choisi
        int type = msg.type;
        msg.type = -1;

        if (type == MSG_USER_CONNECT) {
            printf("RECV MSG_USER_CONNECT\n");
                        
            int pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                user_process(permanent_queue, getpid());
                exit(0);
            } else {
                printf("pid : %d\n", pid);
                add_user(&g_users, msg.name, pid);
            }
        } else if (type == MSG_REST_REGISTER) {
            
            int id = mem->rests_number + 1;
            sem_wait(sem_mutex);                  
            hubert_add_restaurant(mem, msg.name, id);
            sem_post(sem_mutex);
            struct msg_long cur = {MSG_REST_STATUS, 1 };
            msgsnd(permament_queue, &cur, MSG_SIZE(long), 0);
            
        } else if (type == MSG_REST_UNREGISTER) {            
            sem_wait(sem_mutex);
            hubert_del_restaurant(mem, msg.name);
            sem_post(sem_mutex);
        } else if (type == MSG_USER_DISCONNECT) {
            kill_user(&g_users, msg.name);
        }
    }
    
    shmdt(mem);
    sem_destroy(sem_drivers);
    msgctl(permanent_queue, IPC_RMID, NULL);
    
    on_close(0);
}
