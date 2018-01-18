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

#include "hubert.h"

#define UPDATE_STOCK_DELAY  5   /* In seconds */

static int updating_process_pid = -1;
sem_t *sem_mutex;

void main_close(int n)
{
    if (updating_process_pid > 0) {
        kill(updating_process_pid, SIGTERM);
    }
    
    sem_close(sem_mutex);
    sem_unlink(SEM_MUTEX);
    
    exit(n);
}

static void update_restaurants(sem_t *mutex)
{
    int mem_key, queue;
    struct restaurant restaurant;

    mem_key = shmget(1500, sizeof(struct shared_memory), IPC_CREAT | 0666);
    if (mem_key < 0) {
        fprintf(stderr, "[ERROR] shmget in update_restaurants().\n");
        return;
    }
    
    struct shared_memory *mem = shmat(mem_key, 0, 0);
    if (mem < 0) {        fprintf(stderr, "[ERROR] shmat in update_restaurants().\n");
        return;
    }
    
    for (int i = 0; i < mem->rests_number; i++) {
        printf("Updater : Updating : %s\n", mem->restaurants[i].name);
        
        queue = msgget(mem->restaurants[i].id, 0);
        if (queue < 0) {            fprintf(stderr, "[ERROR] shmat in update_restaurants().\n");
            shmdt(mem);
            return;
        }
        
        if (!send_long(queue, MSG_LONG, STOCK_REQUEST)) {            fprintf(stderr, "[ERROR] send_long in update_restaurants().\n");
            shmdt(mem);
            return;
        }
        
        recv_restaurant(queue, &restaurant);
        
        sem_wait(mutex);
        update_stock(&mem->restaurants[i], &restaurant.stock, 1);
        sem_post(mutex);
    }
}

static void updating_process(sem_t *mutex)
{
    while (1) {
        sleep(UPDATE_STOCK_DELAY);
        printf("Updater : update\n");        
        
        update_restaurants(mutex);        
    }
}

int main(int argc, char **argv)
{
    sem_mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0644, 1);
    if (sem_mutex == SEM_FAILED) {
        PANIC(main_close, "[ERROR] Failed to open sem_mutex\n");
    }

    updating_process_pid = fork();
    if (updating_process_pid == 0) {
        updating_process(sem_mutex);
    } else {
        hubert_process(sem_mutex);
    }

    return 0;
}
