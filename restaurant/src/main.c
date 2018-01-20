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
#include <stdarg.h>
#include <error.h>

#define ALIMENTS_MAX    5
#define COMMAND_MAX		10

struct command_list {
    struct command cmd;
    struct command_list  *next;
};

struct cuisine_stock {
    char aliments[ALIMENTS_MAX][NAME_MAX];
    int quantities[ALIMENTS_MAX];
    int aliments_count;
};

struct receipe {
    char name[NAME_MAX];
    int  temps_prep;
    char ingredients[ALIMENTS_MAX][NAME_MAX];
    int  quantities[ALIMENTS_MAX];
    int  ingredients_count;
};

struct rest {
    char            name[NAME_MAX];
    struct receipe  recettes[FOODS_MAX];
    int             plates_count;
    struct command_list *cmd_waiting;

};

static int cook_pid = -1;
static sem_t *rest_mutex = NULL;
static sem_t *hubert_mutex = NULL;
static int shmemid = -1;
static int shmemid_hubert = -1;
static int q = -1;

static void on_close(int n);

LOG_FUNCTIONS(rest)
LOG_FUNCTIONS(cook)
PANIC_FUNCTION(rest, on_close)

static void disconnect()
{    
    struct msg_state msg = { HUBERT_DEST, REST_DISCONNECT, getpid() };
    msgsnd(q, &msg, MSG_STATE_SIZE, 0);
}

static void on_close(int n)
{
    log_rest("Closing Restaurant");

    if (shmemid > 0) {
        shmctl(shmemid, IPC_RMID, 0);
    }
    
    if (shmemid_hubert > 0) {
        shmctl(shmemid_hubert, IPC_RMID, 0);
    }

    if (hubert_mutex != NULL) {
        sem_destroy(hubert_mutex);
        char pid[NAME_MAX];
        sprintf(pid, "%d", getpid());
        sem_unlink(pid);
    }
    
    if (rest_mutex != NULL) {
        sem_destroy(rest_mutex);
        char sem_name[NAME_MAX];
        sprintf(sem_name, "%d", getpid() + 1);
        sem_unlink(sem_name);
    }
    
    if (q >= 0) {
        disconnect();
    }
    
    if (cook_pid > 0) {
        kill(cook_pid, SIGTERM);
    }

    exit(0);
}

void print_rest(struct rest *r)
{
    printf("\nRestaurant : %s, nb aliments : %d\n", r->name, r->plates_count);
    for (int i = 0; i < r->plates_count; i++) {
        printf("\nPlat : %s           temps de préparation : %d\nAliments necessaires :\n",
               r->recettes[i].name, r->recettes[i].temps_prep);
        for (int j = 0; j < r->recettes[i].ingredients_count; j++) {
            printf("     %d %s", r->recettes[i].quantities[j],
                                 r->recettes[i].ingredients[j]);
        }
        printf("\n");
    }
}

int read_config(const char *filename, struct rest *r, struct cuisine_stock *s)
{
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    if (fgets(r->name, NAME_MAX, file) == NULL) {
        shmdt(r);
        return -1;
    }

    r->name[strlen(r->name) - 1] = '\0';

    log_rest("Restaurant name : %s", r->name);

    char buffer[512];

    while (fgets(buffer, 512, file) != NULL) {
        if (buffer[0] == '{') {
            //printf("checking aliment\n");
            fgets(buffer, 512, file);
            while (buffer[0] != '}') {
                int n = r->plates_count;
                sscanf(buffer, "%s %d", r->recettes[n].ingredients[r->recettes[n].ingredients_count],
                       &r->recettes[n].quantities[r->recettes[n].ingredients_count]);
                
                /*char ingredient[NAME_MAX];
                strcpy(ingredient, r->recettes[n].ingredients[r->recettes[n].ingredients_count]);
                if (!ingredient_in_stock(s, ingredient)) { //TODO : creer la fonction
                    s->quantities[r->recettes[n].ingredients_count]] = 5;
                    strcpy(s->foods[r->recettes[n].ingredients_count]++], ingrediant);
                }*/
                
                r->recettes[r->plates_count].ingredients_count++;
                fgets(buffer, 512, file);
            }
            r->plates_count++;
        } else {
            sscanf(buffer, "%s %d" , r->recettes[r->plates_count].name, &r->recettes[r->plates_count].temps_prep);
        }
    }

    log_rest("End reading config");
    print_rest(r);

    return 1;
}

int update_hubert_mem()
{
    struct menu *m = shmat(shmemid_hubert, 0, 0);
    struct rest *r = shmat(shmemid, 0, 0);
    
    if (m == (void *) -1) {
        return 0;
    }
    
    if (r == (void *) -1) {
        return 0;
    }
    
    sem_wait(hubert_mutex);
    
    strncpy(m->name, r->name, NAME_MAX);
    for (int i = 0; i < r->plates_count; i++) {
        strncpy(m->foods[i], r->recettes[i].name, NAME_MAX);        
    }
    m->foods_count = r->plates_count;
        
    sem_post(hubert_mutex);
    
    return 1;
}

static int connect()
{
    int key = getpid();
    struct msg_state msg = { HUBERT_DEST, REST_CONNECT, key };
    msgsnd(q, &msg, MSG_STATE_SIZE, 0);
    
    struct msg_status status;
    msgrcv(q, &status, MSG_STATUS_SIZE, key, 0);
    
    return status.status;
}

int time_command(struct rest *r, struct command cmd)
{
    int time = 0;
    log_cook("Command count : %d", cmd.count);
    
    for (int i = 0; i < cmd.count; i++) {
        for (int j = 0; j < r->plates_count; j++) {
            if (strcmp(cmd.foods[i], r->recettes[j].name) == 0 && time < r->recettes[j].temps_prep) {
                time = r->recettes[j].temps_prep;
            }
        }
    }
    
    return time;
}

void add_command(struct command *c)
{
    struct rest *r = shmat(shmemid, 0, 0);
    
    struct command_list *cmd = malloc(sizeof(struct command_list));
    cmd->cmd = *c;
    cmd->next = NULL;
    
    sem_wait(rest_mutex);
    
    struct command_list **temp = &r->cmd_waiting;
    while (*temp != NULL) {
        temp = &(*temp)->next;
    }
    
    *temp = cmd;
    sem_post(rest_mutex);
    
    log_rest("add_command %p %d %s", r, r->cmd_waiting->cmd.count, r->cmd_waiting->cmd.name);
    shmdt(r);
}

void buy_ingredients(struct command cmd)
{
    struct cuisine_stock ali_tot;
        
}

int cook_aliments(struct rest *r)
{
    sem_wait(rest_mutex);
    struct command cmd = r->cmd_waiting->cmd;    
    sem_post(rest_mutex);
    
    log_rest("cook_aliments %p %s", r, cmd.name);
    int time = time_command(r, cmd);
    if (time == 0) {
        log_cook_error("can't calcul command time");
        return 0;
    }
    
    struct msg_command_status rcv_command = { .dest=cmd.user_pid, .status= COMMAND_START, .time = time};
    strcpy(rcv_command.rest_name, r->name);
    msgsnd(HUBERT_KEY, &rcv_command, MSG_COMMAND_SIZE, 0);
    
    buy_ingredients(cmd);
    sleep(time);
    
    return 1;
}

void kitchen_process(struct cuisine_stock s)
{
    struct rest *r = shmat(shmemid, 0, 0);
    
    while(1) {
        sleep(3);
        if (r->cmd_waiting != NULL) {
            if (cook_aliments(r)) {
                log_rest("Cooked");
                struct msg_command_status finish_cmd;
            }
            
        }
    }
}
int main(int argc, char **argv)
{
    signal(SIGINT, on_close);
    
    shmemid = shmget(HUBERT_KEY, sizeof(struct rest), IPC_CREAT | IPC_EXCL | 0666);
    if (shmemid < 0) {        
        shmemid = shmget(HUBERT_KEY, sizeof(struct rest), IPC_CREAT | 0666);
        shmctl(shmemid, IPC_RMID, NULL);
        rest_panic("Can't open shmemid");
        return -1;
    }
    
    shmemid_hubert = shmget(getpid(), sizeof(struct menu), IPC_CREAT | IPC_EXCL | 0666);
    if (shmemid_hubert < 0) {
        rest_panic("Can't open shmemid_hubert");
        return -1;
    }
    
    char name[NAME_MAX];
    sprintf(name, "%d", getpid());
    hubert_mutex = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
    if (hubert_mutex == SEM_FAILED) {
        rest_panic("Can't open hubert_mutex");
    }
    
    sprintf(name, "%d", getpid() + 1);
    rest_mutex = sem_open(name, O_CREAT | O_EXCL, 0666, 1);
    if (rest_mutex == SEM_FAILED) {
        rest_panic("Can't open hubert_mutex");
    }
    
    struct rest *r = shmat(shmemid, 0, 0);
    struct cuisine_stock s;
    
    if (!read_config("config.txt", r, &s)) {
        rest_panic("Can't read config");
        return -1;
    }
    shmdt(r);
    
    if (!update_hubert_mem()) {
        rest_panic("Can't initialy update the menu");        
    }
    
    q = msgget(HUBERT_KEY, 0);
    if (q < 0) {
        rest_panic("Can't open queue");
    }
        
    int status = connect();
    if (status != STATUS_OK) {
        rest_panic("Can't connect to hubert. Status : %d", status);
    }
    
    log_rest("Connected");
    
    cook_pid = fork();
    if (cook_pid == 0) {
        signal(SIGINT, SIG_DFL);
        kitchen_process(s);
    } else {
        while(1) {
            struct msg_command command;
            if (msgrcv(q, &command, MSG_COMMAND_SIZE, getpid(), 0) < 0) {
                rest_panic("While receiving");
            }
            
            add_command(&command.command);
        }
    }
    
    on_close(0);
    return 0;
}
