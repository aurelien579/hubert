#include <types.h>

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

#define UPDATE_STOCK_DELAY  1   /* In seconds */

/*static void list_print(struct list *l)
{
    struct list *cur = l;

    while (cur) {
        printf("%s\n", cur->name);
        cur = cur->next;
    }
}

void print_rests(struct restaurant *rests)
{
    struct restaurant *cur = rests;

    while (cur) {
        printf("%s\n", cur->info.name);
        cur = cur->next;
    }
}*/

static void update_stock()
{
    //printf("updating stocks...\n");
}

static void updating_process()
{
    while (1) {
        update_stock();
        sleep(UPDATE_STOCK_DELAY);
    }
}

int main(int argc, char **argv)
{
    //test();
    
    sem_t *sem_mutex = sem_open(SEM_MUTEX, O_CREAT, 0644, 1);
    if ( sem_mutex == SEM_FAILED) {
        printf("failed open semaphore mutex\n");
        exit(-1);
    }
    int pid = fork();

    if (pid == 0) {
        updating_process();
    } else {
        hubert_process(pid);
    }
    sem_destroy(sem_mutex);
    return 0;
}
