#ifndef HUBERT_PROCESS_H
#define HUBERT_PROCESS_H

#include <types.h>

#define SEM_MUTEX            "shared_mem_mutex"
#define SEM_DRIVERS          "livreurs"
#define NB_DRIVERS           10
#define TIME_DELIVERING      2

struct shared_memory {
    struct restaurant   restaurants[REST_MAX];
    int                 rests_number;
};

void hubert_process(int updating_process_pid);

void hubert_add_restaurant(struct shared_memory *ptr, const char *name, int id);
void hubert_del_restaurant(struct shared_memory *ptr, const char *name);
struct restaurant *hubert_find_restaurant(struct shared_memory *mem, char *name);

#endif
