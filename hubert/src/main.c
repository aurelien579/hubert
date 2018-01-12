#include <types.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>

#define UPDATE_STOCK_DELAY  1   /* In seconds */

struct restaurant *restaurants = NULL;
struct user       *users       = NULL;

static int count_restaurant = 100;
int permanent_queue;
sem_t *sem_drivers;

static void register_rest(char *name, int id)
{
    
}

static void unregister_rest(char *name)
{
    
}

static void add_user(char *name, int id)
{
    
}

static void kill_user(char *name)
{
    
}

static void handle_user(int id)
{
    
}

static void update_stock(int n)
{
    printf("updating stocks...\n");
    alarm(UPDATE_STOCK_DELAY);
}

int main(int argc, char **argv)
{
    permanent_queue = msgget(UBERT_KEY, IPC_CREAT | 0666);
    
    if (permanent_queue < 0) {
        PANIC("msgget");
    }
    
    sem_drivers = malloc(sizeof(sem_t));
    if (sem_init(sem_drivers, 1, DRIVER_COUNT) < 0) {
        PANIC("sem_init");
    }
    
    if (signal(SIGALRM, update_stock) == SIG_ERR) {
        PANIC("signal");
    }    
    
    alarm(UPDATE_STOCK_DELAY);
    struct msg_state *msg = NULL;
    
    while(1) {
        msgrcv(permanent_queue, msg, NAME_MAX * sizeof(char), 0, 0); // a tester option choisi
        
        if (msg->type == MSG_USER_CONNECT) {
            int pid = fork();
            if (pid==0) {
                add_user(msg->name, pid);
                handle_user(pid);
                exit(0);
            }
        } else if (msg->type == MSG_REST_REGISTER) {
            int id = count_restaurant++;
            register_rest(msg->name, id);
        } else if (msg->type == MSG_REST_UNREGISTER) {
            unregister_rest(msg->name);
        } else if (msg->type == MSG_USER_DISCONNECT) {
            kill_user(msg->name);
        }
    
    }
  
    sem_destroy(sem_drivers);
    msgctl(permanent_queue, IPC_RMID, NULL);
    
    return 0;
}
