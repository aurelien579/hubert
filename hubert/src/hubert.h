#ifndef HUBERT_PROCESS_H
#define HUBERT_PROCESS_H

#include <types.h>
#include <semaphore.h>

#define REST_FIRST_ID   500
#define USER_FIRST_ID   REST_FIRST_ID + REST_MAX


#define SEM_MUTEX            "hubert_mutex"
#define SEM_DRIVERS          "livreurs"
#define NB_DRIVERS           10
#define TIME_DELIVERING      2

struct shared_memory {
    struct menu   restaurants[REST_MAX];
    int           rests_number;
};

//extern sem_t *sem_mutex;
void main_close(int n);

void hubert_process(sem_t *sem_mutex);

void hubert_add_restaurant(struct shared_memory *ptr, const char *name, int id);
void hubert_del_restaurant(struct shared_memory *ptr, const char *name);
struct restaurant *hubert_find_restaurant(struct shared_memory *mem, char *name);

#endif
