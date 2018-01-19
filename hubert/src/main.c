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

sem_t *sem_mutex;

void main_close(int n)
{    
    sem_close(sem_mutex);
    sem_unlink(SEM_MUTEX);    
    exit(n);
}

int main(int argc, char **argv)
{
    sem_mutex = sem_open(SEM_MUTEX, O_CREAT | O_EXCL, 0644, 1);
    if (sem_mutex == SEM_FAILED) {
        PANIC(main_close, "[ERROR] Failed to open sem_mutex\n");
    }

    hubert_process(sem_mutex);

    return 0;
}
