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

struct restaurant *restaurants  = NULL;
unsigned long      rests_number = 0;

struct user       *users        = NULL;
unsigned long      users_number = 0;

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

    struct user *cur = users;
    while (cur) {
        printf("Disconnecting %s\n", cur->name);
        msgctl(cur->id, IPC_RMID, NULL);
        cur = cur->next;
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
            rests_number--;
            
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
    
    rests_number++;
    
    restaurants = rest;
}

static void add_user(const char *name, int id)
{
    struct user *user = malloc(sizeof(struct user));
    user->id = id;
    strncpy(user->name, name, NAME_MAX);
    user->next = users;

    users = user;
    
    users_number++;
}

static void kill_user(const char *name)
{
    int pid = unregister((struct list **) &users, name);

    if (pid < 0) {
        PANIC("unregister");
    }

    kill(pid, SIGKILL);
    
    users_number--;
}

int get_size_rests_info()
{
    int size = 0;
    struct restaurant *restaurants_temp = restaurants;

    while (restaurants_temp != NULL) {
        size += sizeof(restaurants_temp->info.name)
              + sizeof(int)
              + restaurants_temp->info.stock.count * sizeof(struct food);
            
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

struct restaurant *find_restaurant(char *restaurant) {
    struct restaurant *rest = restaurants;
    while (strncmp(restaurant, restaurants->info.name, NAME_MAX) != 0) {
            rest = rest->next;
    }
    return rest;
}

void remove_foods_in_stock(char *rest_name, struct msg_food_list *command, int nb_foods) {
    struct restaurant *rest = find_restaurant(rest_name);

    int count_command = 0;
    for (count_command = 0; count_command < nb_foods; count_command++) {
        int count = 0;
        while (strncmp(rest->info.stock.foods[count].name, command->foods[count_command].name, NAME_MAX) != 0) {
            count++;
        }
        rest->info.stock.foods[count].quantity -= command->foods[count_command].quantity;
    }
}

static void handle_user(int id)
{    
    printf("handle_user %d\n", id);
    int user_queue = msgget(id, IPC_CREAT | 0666);
    if (user_queue < 0) {
        PANIC("opening user queue");
    }    

    struct msg_long status;
    status.type = MSG_USER_STATUS;
    status.value = id;
    msgsnd(permanent_queue, &status, MSG_SIZE(long), 0);
    
    while (1) {        
        struct msg_long msg;
        msgrcv(user_queue, &msg, sizeof(int), MSG_LONG, 0);
        printf("RECV\n");
        
        if (msg.value == MSG_OFFER_REQUEST) {
            printf("MSG_OFFER_REQUEST\n");
            
            struct msg_long msg_rest_count = { MSG_LONG, rests_number };
            msgsnd(user_queue, &msg_rest_count, MSG_SIZE(long), 0);
            
            struct restaurant *cur = restaurants;
            for (int i = 0; i < rests_number; i++) {
                int food_list_size =  cur->info.stock.count * sizeof(struct food);
                struct msg_rest msg_rest = { .type = MSG_REST, .size = food_list_size };
                strcpy(msg_rest.name, cur->info.name);                
                msgsnd(user_queue, &msg_rest, MSG_SIZE(rest), 0);
                
                struct msg_food_list *msg_list = malloc(sizeof(long) + food_list_size);
                msg_list->type = MSG_FOOD_LIST;
                memcpy(msg_list->foods, cur->info.stock.foods, food_list_size);
                msgsnd(user_queue, msg_list, food_list_size, 0);                
                free(msg_list);
                
                cur = cur->next;
            }
            
            
        } else if(msg.value == MSG_COMMAND_ANNOUNCE) {
            printf("MSG_COMMAND_ANNOUCE\n");

            /*struct msg_command_announce command_announce;
            msgrcv(user_queue, &command_announce, sizeof(int) + NAME_MAX, MSG_COMMAND_ANNOUNCE, 0);

            struct restaurant *rest = find_restaurant(command_announce.restaurant_name);
            int id = rest->id;

            struct msg_stock_announce stock_announce;
            stock_announce.type = MSG_STOCK_ANNOUNCE;
            stock_announce.count = command_announce.count;

            msgsnd(id, &stock_announce, sizeof(int), 0);

            struct msg_food_list *command = malloc(sizeof(long) + command_announce.count * sizeof(struct food));
            msgrcv(user_queue, command, command_announce.count * sizeof(struct food), MSG_STOCK, 0);

            msgsnd(id , command, command_announce.count * sizeof(struct food), 0);

            remove_foods_in_stock(command_announce.restaurant_name, command, command_announce.count);
            free(command);*/
        } else {
            PANIC("Unknown message received\n");
        }
    }
}

static void update_stock()
{
    //printf("updating stocks...\n");
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
        printf("Reading...\n");
        msgrcv(permanent_queue, &msg, NAME_MAX * sizeof(char), 0, 0); // a tester option choisi
        int type = msg.type;
        msg.type = -1;

        if (type == MSG_USER_CONNECT) {
            printf("RECV MSG_USER_CONNECT\n");
                        
            int pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_DFL);
                handle_user(getpid());
                exit(0);
            } else {                
                printf("pid : %d\n", pid);
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
    register_rest("Name test", 45);
    struct restaurant *rest= find_restaurant("Name test");
    rest->info.stock.foods = malloc(sizeof(struct food));
    rest->info.stock.count = 2;
    strcpy(rest->info.stock.foods[0].name, "patate");
    rest->info.stock.foods[0].quantity = 10;
    strcpy(rest->info.stock.foods[1].name, "chips");
    rest->info.stock.foods[1].quantity = 250000;
    int pid = fork();

    if (pid == 0) {
        updating_process();
    } else {
        updating_process_pid = pid;
        hubert_process();
    }

    return 0;
}
