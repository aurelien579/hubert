#include <types.h>
#include "hubert_process.h"

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


static void on_close(int n)
{
    exit(0);
}

static void send_restaurant(int user_queue, struct restaurant *rest)
{    
    int food_list_size =  rest->stock.count * sizeof(struct food);
    struct msg_rest msg_rest = { .type = MSG_REST, .size = food_list_size };
    strcpy(msg_rest.name, rest->name);                
    msgsnd(user_queue, &msg_rest, MSG_SIZE(rest), 0);
    
    struct msg_food_list *msg_list = malloc(sizeof(long) + food_list_size);
    msg_list->type = MSG_FOOD_LIST;
    memcpy(msg_list->foods, rest->stock.foods, food_list_size);
    msgsnd(user_queue, msg_list, food_list_size, 0);                
    free(msg_list);
}

static void send_restaurants(int user_queue, struct shared_memory *mem)
{                
    printf("send_restaurant %d\n", mem->rests_number);
    
    struct msg_long msg_rest_count = { MSG_LONG, mem->rests_number };
    msgsnd(user_queue, &msg_rest_count, MSG_SIZE(long), 0);
    
    for (int i = 0; i < mem->rests_number; i++) {
        send_restaurant(user_queue, &mem->restaurants[i]);
    }
}

static int remove_foods_in_stock(struct restaurant *rest, struct msg_food_list *command, int nb_foods)
{
    int count_command = 0;
    for (count_command = 0; count_command < nb_foods; count_command++) {
        int count = 0;
        while (strncmp(rest->stock.foods[count].name, command->foods[count_command].name, NAME_MAX) != 0) {
            count++;
        }
        int food_stocked = rest->stock.foods[count].quantity;
        int food_commanded = command->foods[count_command].quantity;
        if (food_stocked < food_commanded) {
            return 0;
        }
        else {
            rest->stock.foods[count].quantity -= food_commanded;
        }
    }
    return 1;
}

void user_process(int permanent_queue, int id)
{
    int mem_key = shmget(1500, sizeof(struct shared_memory), 0);
    struct shared_memory *mem = shmat(mem_key, 0, 0);
    
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
        msgrcv(user_queue, &msg, MSG_SIZE(long), MSG_LONG, 0);
        
        if (msg.value == MSG_OFFER_REQUEST) {
            printf("MSG_OFFER_REQUEST\n");
            
            send_restaurants(user_queue, mem);            
        
        } else if(msg.value == MSG_COMMAND_ANNOUNCE) {
            printf("MSG_COMMAND_ANNOUCE\n");
            
            sem_t *sem_drivers = sem_open(SEM_DRIVERS, 0);
            sem_wait(sem_drivers);
            
            struct msg_command_announce command_announce;
            msgrcv(user_queue, &command_announce, sizeof(int) + NAME_MAX, MSG_COMMAND_ANNOUNCE, 0);
            
            struct restaurant *rest = hubert_find_restaurant(mem, command_announce.restaurant_name);
            if (rest == NULL) {
                PANIC("Can't find restaurant");
            }
            
            struct msg_long transfered_command = { MSG_LONG, command_announce.count };
            //msgsnd(rest->id, &transfered_command, MSG_SIZE(long), 0);
            
            int message_size = command_announce.count * sizeof(struct food);
            struct msg_food_list *command = malloc(sizeof(long) + message_size);
            msgrcv(user_queue, command, message_size, MSG_FOOD_LIST, 0);
            
            //msgsnd(rest->id , command, message_size, 0);

            if (remove_foods_in_stock(rest, command, command_announce.count)) {
                msg.value = COMMAND_ACK;
                
                sleep(TIME_DELIVERING);
                sem_post(sem_drivers);
            
                msgsnd(user_queue, &msg, MSG_SIZE(long), 0);
            }
            else {
                msg.value = COMMAND_NACK;
                
                sem_post(sem_drivers);
            
                msgsnd(user_queue, &msg, MSG_SIZE(long), 0);
            }

            free(command);
            
        } else {
            PANIC("Unknown message received\n");
        }
    }
}
