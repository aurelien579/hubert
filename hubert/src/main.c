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
    rest->info.stock.foods = NULL;
    rest->info.stock.count = 0;
    rest->next = restaurants;
    strncpy(rest->info.name, name, NAME_MAX);
    
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

int get_rest_count()
{
    int count;
    struct restaurant *restaurants_temp = restaurants;
    
    while (restaurants_temp != NULL) {
        count ++;
        restaurants_temp = restaurants_temp->next;
    }
    
    return count;
}

int get_size_rests_info()
{
    int size;
    struct restaurant *restaurants_temp = restaurants;
    
    while (restaurants_temp != NULL) {
        size += sizeof(restaurants_temp->info.name) + restaurants_temp->info.stock.count * sizeof(struct food);
        restaurants_temp = restaurants_temp->next;
    }
    
    return size;
}

void create_rests_info(struct restaurant_info rests[], int rest_count)
{
    struct restaurant *cur = restaurants;
    
    for (int i = 0; i < rest_count; i++) {
        //int rest_size = sizeof(cur->info.name) + sizeof(struct food) * cur->info.stock.count;
        rests[i] = cur->info;
        //memcpy(&rests[i], &cur->info, rest_size);
        
        cur = cur->next;
    }
}

static void handle_user(int id)
{    
    int user_queue = msgget(id, IPC_CREAT | 0666);
    if (user_queue < 0) {
        PANIC("opening user queue");
    }
    
    while (1) {
        struct msg_type msg;
        msgrcv(user_queue, &msg, sizeof(long), MSG_TYPE, 0);
        
        if (msg.next_type == MSG_OFFER_REQUEST) {
            printf("MSG_OFFER_REQUEST\n");
            
            struct msg_stock_announce header;
            header.type = MSG_STOCK_ANNOUNCE;
            header.rest_count = get_rest_count();
            msgsnd(user_queue, &header, sizeof(header.rest_count), 0);
            
            int rests_info_size = get_size_rests_info();
            struct msg_restaurant_list *menu = malloc(sizeof(long) + rests_info_size);
            menu->type = MSG_REST_LIST;
            create_rests_info(menu->rests, header.rest_count);
            msgsnd(user_queue, &menu, rests_info_size, 0); 
        }
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
        printf("%s\n", cur->info.name);
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
    
    struct msg_state msg;
    
    while (1) {
        msgrcv(permanent_queue, &msg, NAME_MAX * sizeof(char), 0, 0); // a tester option choisi
        int type = msg.type;
        msg.type = -1;
        
        if (type == MSG_USER_CONNECT) {
            int pid = fork();      
            if (pid == 0) {
                handle_user(getpid());
                exit(0);
            } else {
                add_user(msg.name, pid);                
            }
        } else if (type == MSG_REST_REGISTER) {
            int id = count_restaurant++;
            register_rest(msg.name, id);
        } else if (type == MSG_REST_UNREGISTER) {
            unregister((struct list **) &restaurants, msg.name);
        } else if (type == MSG_USER_DISCONNECT) {
            kill_user(msg.name);
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
