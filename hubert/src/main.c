#include <types.h>
#include <msg.h>

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

#include "hubert_process.h"

#define UPDATE_STOCK_DELAY  5   /* In seconds */

sem_t *sem_mutex;

static void update_restaurants()
{
    int mem_key = shmget(1500, sizeof(struct shared_memory), IPC_CREAT | 0666);
    struct shared_memory *mem = shmat(mem_key, 0, 0);

    for (int i = 0; i < mem->rests_number; i++) {
        printf("Updater : %d\n", i);
        int queue = msgget(mem->restaurants[i].id, IPC_CREAT | 0666);
        struct msg_long stock_request = { MSG_LONG, STOCK_REQUEST };
        msgsnd(queue, &stock_request, MSG_SIZE(long), 0);

        struct restaurant rest;
        recv_restaurant(queue, &rest);

        update_stock(&mem->restaurants[i], &rest.stock, 1);
    }
}

static void updating_process()
{
    while (1) {
        sleep(UPDATE_STOCK_DELAY);
        printf("Updater : update\n");
        
        sem_wait(sem_mutex);
        update_restaurants();        
        sem_post(sem_mutex);
    }
}

int main(int argc, char **argv)
{
    sem_mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0644, 1);
    if (sem_mutex == SEM_FAILED) {
        printf("failed open semaphore mutex\n");
        exit(-1);
    }
	
    int pid = fork();

    if (pid == 0) {
        updating_process();
    } else {
        hubert_process(pid);
    }

    return 0;
}
