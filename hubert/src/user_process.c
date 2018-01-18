#include <types.h>
#include <msg.h>
#include "hubert.h"

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

struct user_process {
    int id;};

static struct user_process user;

static void user_log(const char *str, ...)
{
    va_list args;

    va_start(args, str);
    printf("User %d : ", user.id);
    vprintf(str, args);
    va_end(args);
}

static void user_close(int n)
{
    exit(0);
}

static void send_restaurants(int user_queue, struct shared_memory *mem)
{
    user_log("send_restaurant %d\n", mem->rests_number);

    struct msg_long msg_rest_count = { MSG_LONG, mem->rests_number };
    msgsnd(user_queue, &msg_rest_count, MSG_SIZE(long), 0);

    for (int i = 0; i < mem->rests_number; i++) {
        send_restaurant(user_queue, &mem->restaurants[i]);
    }
}

void user_process(int id)
{
    user.id = id;
    
    int mem_key = shmget(1500, sizeof(struct shared_memory), 0);
    struct shared_memory *mem = shmat(mem_key, 0, 0);
    sem_t *sem = sem_open(SEM_MUTEX, 0);

    user_log("user_process start\n");
    int user_queue = msgget(id, IPC_CREAT | 0666);
    if (user_queue < 0) {
        PANIC(user_close, "User : opening user queue");
    }

    while (1) {
        long request;
        recv_long(user_queue, MSG_LONG, &request);

        if (request == OFFER_REQUEST) {
            user_log("OFFER_REQUEST\n");
            sem_wait(sem);
            send_restaurants(user_queue, mem);
            sem_post(sem);
        } else if (request == COMMAND_ANNOUNCE) {
            user_log("COMMAND_ANNOUCE\n");

            sem_t *sem_drivers = sem_open(SEM_DRIVERS, 0);
            sem_wait(sem_drivers);

            struct restaurant received_rest;
            recv_restaurant(user_queue, &received_rest);

            struct restaurant *hubert_rest = hubert_find_restaurant(mem, received_rest.name);
            if (hubert_rest == NULL) {
                PANIC(user_close, "Can't find restaurant");
            }

            user_log("Commande recue : \n\n");
            print_rest(&received_rest);

            if (is_command_valid(hubert_rest, &received_rest.stock) > 0) {
                user_log("Command valid %d\n", received_rest.stock.count);

                sem_wait(sem);
                update_stock(hubert_rest, &received_rest.stock, 0);
                sem_post(sem);

                user_log("New rest : \n");
                print_rest(hubert_rest);
                user_log("\n\n");

                int rest_queue = msgget(hubert_rest->id, 0);
                struct msg_long request = { MSG_LONG, COMMAND };
                msgsnd(rest_queue, &request, MSG_SIZE(long), 0);
                user_log("Sending command : \n");
                print_rest(&received_rest);
                user_log("\n\n");
                send_restaurant(rest_queue, &received_rest);

                sleep(TIME_DELIVERING);
                sem_post(sem_drivers);

                send_long(user_queue, MSG_LONG, COMMAND_ACK);
            } else {
                user_log("Command invalid received\n");
                sem_post(sem_drivers);
                send_long(user_queue, MSG_LONG, COMMAND_NACK);
            }
        } else {
            PANIC(user_close, "Unknown message received\n");
        }
    }
}
