#include <types.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

#define UPDATE_STOCK_DELAY  1   /* In seconds */
 
 
struct shared_memory {
    
};

static int updating_process_pid = -1;

struct restaurant *restaurants = NULL;
struct user       *users       = NULL;

static int count_restaurant = 100;
static int permanent_queue;
static sem_t *sem_drivers;

static void list_print(struct list *l)
{
    struct list *cur = l;
    
    while (cur) {
        printf("%s\n", cur->name);
        cur = cur->next;
    }
}

static void on_close(int n)
{
    
    if (updating_process_pid > 0) {
        kill(updating_process_pid, SIGTERM);
    }
    
    /* TODO: Disconnect users */

    exit(0);
}

static int unregister(struct list **list, const char *name)
{
    struct list **cur = list;
    
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

static void register_rest(const char *name, int id)
{
    struct restaurant *rest = malloc(sizeof(struct restaurant));
    rest->id = id;
    rest->stock.foods = NULL;
    rest->stock.count = 0;
    rest->next = restaurants;
    strncpy(rest->name, name, NAME_MAX);
    
    restaurants = rest;        
}

static void add_user(const char *name, int id)
{
    struct user *user = malloc(sizeof(struct user));
    user->id = id;
    strncpy(user->name, name, NAME_MAX);
    user->next = users;
    
    users = user;
}

static void kill_user(const char *name)
{
    int pid = unregister((struct list **) &users, name);
    
    if (pid < 0) {
        PANIC("unregister");
    }
    
    kill(pid, SIGKILL);
}

static void handle_user(int id)
{    
    int user_queue = msgget(id, IPC_CREAT | 0666);
    if (user_queue < 0) {
        PANIC("opening user queue");
    }
    
    while (1) {
        
    }
}

static void update_stock()
{
    printf("updating stocks...\n");
}

void print_rests(struct restaurant *rests)
{
    struct restaurant *cur = rests;
    
    while (cur) {
        printf("%s\n", cur->name);
        cur = cur->next;
    }
}

void test()
{
    
    while(1);
}

static void updating_process()
{    
    while (1) {
        update_stock();
        sleep(UPDATE_STOCK_DELAY);
    }
}

static void hubert_process()
{
    signal(SIGINT, on_close);
    
    permanent_queue = msgget(UBERT_KEY, IPC_CREAT | 0666);
    
    if (permanent_queue < 0) {
        PANIC("msgget");
    }
    
    sem_drivers = malloc(sizeof(sem_t));
    if (sem_init(sem_drivers, 1, DRIVER_COUNT) < 0) {
        PANIC("sem_init");
    }
    
    struct msg_state *msg = malloc(sizeof(struct msg_state));
    
    while (1) {
        msgrcv(permanent_queue, msg, NAME_MAX * sizeof(char), 0, 0); // a tester option choisi
        int type = msg->type;
        msg->type = -1;
        
        if (type == MSG_USER_CONNECT) {
            int pid = fork();      
            if (pid == 0) {
                handle_user(getpid());
                exit(0);
            } else {
                add_user(msg->name, pid);                
            }
        } else if (type == MSG_REST_REGISTER) {
            int id = count_restaurant++;
            register_rest(msg->name, id);
        } else if (type == MSG_REST_UNREGISTER) {
            unregister((struct list **) &restaurants, msg->name);
        } else if (type == MSG_USER_DISCONNECT) {
            kill_user(msg->name);
        }
    }
  
    sem_destroy(sem_drivers);
    msgctl(permanent_queue, IPC_RMID, NULL);
}

int main(int argc, char **argv)
{
    //test();
    int pid = fork();
    
    if (pid == 0) {
        updating_process();
    } else {
        updating_process_pid = pid;
        hubert_process();
    }
    
    return 0;
}
