#include <types.h>
#include <msg.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#define SHMEM_KEY 50

static int running = 1;
static int shmemid = -1;


static int queue = -1;


static void on_close(int n)
{
    printf("\rClosing Restaurant\n");
    running = 0;
    

    if (queue > 0) {
    
        struct restaurant *rest = shmat(shmemid, 0, 0);
        int permanent_queue = msgget(UBERT_KEY, 0);
        
        struct msg_state disconnect_msg;
        disconnect_msg.type = MSG_REST_UNREGISTER;
        strncpy(disconnect_msg.name, rest->name, NAME_MAX);
        msgsnd(permanent_queue, &disconnect_msg, MSG_SIZE(state), 0);
        
        printf("queue closed\n");
        msgctl(queue, IPC_RMID, 0);
    }

    if (shmemid > 0) {
        shmctl(shmemid, IPC_RMID, 0);
    }

    exit(0);
}

void restaurant_process(int shmemid, int cook_pid)
{   
    sem_t *mutex = sem_open("restaurant_mutex", O_CREAT, 0666, 1);
    struct restaurant *rest = shmat(shmemid, 0, 0);
    signal(SIGINT, on_close);

    int permanent_queue = msgget(UBERT_KEY, 0);

    struct msg_state msg_state;
    msg_state.type = MSG_REST_REGISTER;
    strcpy(msg_state.name, rest->name);
    msgsnd(permanent_queue, &msg_state, MSG_SIZE(state), 0);

    struct msg_long msg_status;
    msgrcv(permanent_queue, &msg_status, MSG_SIZE(long), MSG_REST_STATUS, 0);
    if (msg_status.value < 0) {
        fprintf(stderr, "Can't connect to hubert\n");
        return;
    }

    queue = msgget(msg_status.value, IPC_CREAT | 0666);
    printf("id : %d\n", msg_status.value);
    while (running) {
        struct msg_long request;
        msgrcv(queue, &request, MSG_SIZE(long), MSG_LONG, 0);

        if (request.value == STOCK_REQUEST) {
            sem_wait(mutex);
            send_restaurant(queue, rest);
            sem_post(mutex);
        } else if (request.value == COMMAND) {
            struct restaurant cmd_rest;
            recv_restaurant(queue, &cmd_rest);
            sem_wait(mutex);
            printf("Command received : \n");
            print_rest(&cmd_rest);
            printf("\n\n");
            update_stock(rest, &cmd_rest.stock, 0);

            printf("After update :\n");
            print_rest(rest);
            printf("\n\n");
            sem_post(mutex);
            kill(cook_pid, SIGUSR1);
        }
        else {
            printf("error : message non compris\n");
        }
    }

    kill(cook_pid, SIGKILL);
    shmdt(rest);
    sem_destroy(mutex);
}

void cook_on_update(int n)
{
    sem_t *mutex = sem_open("restaurant_mutex", O_CREAT, 0666, 1);
    struct restaurant *rest = shmat(shmemid, 0, 0);

    sem_wait(mutex);

    for (int i = 0; i < rest->stock.count; i++) {
        if (rest->stock.foods[i].quantity <= 10) {
            printf("Je cuisine du %s\n", rest->stock.foods[i].name);
            sleep(2);
            rest->stock.foods[i].quantity += 5;
        }
    }

    sem_post(mutex);

    shmdt(rest);
    sem_destroy(mutex);
}

int read_config(const char *filename, int shmemid)
{
    struct restaurant *rest = shmat(shmemid, 0, 0);

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    if (fgets(rest->name, NAME_MAX, file) == NULL) {
        return -1;
    }

    rest->name[strlen(rest->name) - 1] = '\0';

    printf("Restaurant name : %s\n", rest->name);

    char buffer[512];

    while (fgets(buffer, 512, file) != NULL) {
        sscanf(buffer, "%s %d",  rest->stock.foods[rest->stock.count].name,
                    &rest->stock.foods[rest->stock.count].quantity);

        rest->stock.count++;
    }

    printf("config read\n");
    print_rest(rest);

    shmdt(rest);

    return 1;
}

int main(int argc, char **argv)
{
    int pid;
    shmemid = shmget(SHMEM_KEY, sizeof(struct restaurant), IPC_CREAT | 0666);

    if (read_config("config.txt", shmemid) < 0) {
        fprintf(stderr, "Invalid config\n");
        return -1;
    }

    pid = fork();
    if (pid == 0) {
        signal(SIGUSR1, cook_on_update);
        while (1);
        exit(0);
    } else {
        restaurant_process(shmemid, pid);
    }

    shmctl(shmemid, IPC_RMID, NULL);
}

