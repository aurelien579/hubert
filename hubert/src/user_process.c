#include <types.h>
#include <msg.h>
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

static void send_restaurants(int user_queue, struct shared_memory *mem)
{                
    printf("send_restaurant %d\n", mem->rests_number);
    
    struct msg_long msg_rest_count = { MSG_LONG, mem->rests_number };
    msgsnd(user_queue, &msg_rest_count, MSG_SIZE(long), 0);
    
    for (int i = 0; i < mem->rests_number; i++) {
        send_restaurant(user_queue, &mem->restaurants[i]);
    }
}

void user_process(int permanent_queue, int id)
{
    int mem_key = shmget(1500, sizeof(struct shared_memory), 0);
    struct shared_memory *mem = shmat(mem_key, 0, 0);
    sem_t *sem = sem_open(SEM_MUTEX, 0);
    
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
            sem_wait(sem);
            send_restaurants(user_queue, mem);            
            sem_post(sem);
        } else if(msg.value == MSG_COMMAND_ANNOUNCE) {
            printf("MSG_COMMAND_ANNOUCE\n");
            
            sem_t *sem_drivers = sem_open(SEM_DRIVERS, 0);
            sem_wait(sem_drivers);
            
            struct restaurant received_rest;
            recv_restaurant(user_queue, &received_rest);
            
            struct restaurant *hubert_rest = hubert_find_restaurant(mem, received_rest.name);
            if (hubert_rest == NULL) {
                PANIC("Can't find restaurant");
            }
            
            printf("Commande recue : \n\n");
            print_rest(&received_rest);
            
            if (is_command_valid(hubert_rest, &received_rest.stock) > 0) {
                printf("Command valid %d\n", received_rest.stock.count);
                
                sem_wait(sem);
                update_stock(hubert_rest, &received_rest.stock, 0);
                sem_post(sem);
                
                printf("New rest : \n");
                print_rest(hubert_rest);
                printf("\n\n");
                
                int rest_queue = msgget(hubert_rest->id, 0);
                struct msg_long request = { MSG_LONG, COMMAND };
                msgsnd(rest_queue, &request, MSG_SIZE(long), 0);
                printf("Sending command : \n");
                print_rest(&received_rest);
                printf("\n\n");
                send_restaurant(rest_queue, &received_rest);                
                
                msg.value = COMMAND_ACK;
                
                sleep(TIME_DELIVERING);
                sem_post(sem_drivers);
            
                msgsnd(user_queue, &msg, MSG_SIZE(long), 0);
            } else {
                printf("Command invalid\n");

                msg.value = COMMAND_NACK;
                
                sem_post(sem_drivers);
            
                msgsnd(user_queue, &msg, MSG_SIZE(long), 0);
            }            
        } else {
            PANIC("Unknown message received\n");
        }
    }
}
