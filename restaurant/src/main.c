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
#include <time.h>

#define ALIMENTS_MAX    5
#define COMMAND_MAX     10

struct cuisine_stock {
    char aliments[ALIMENTS_MAX][NAME_MAX];
    int quantities[ALIMENTS_MAX];
    int  aliments_count;
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
    struct command cmd_waiting[COMMAND_MAX];
    int cmd_count;

};

static int cook_pid = -1;
static struct rest *rest_mem = NULL;

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

void print_stock( struct cuisine_stock *s) 
{
	printf("Stock : \n\n");
	for (int i=0; i < s->aliments_count; i++) {
		printf("%s : %d\n", s->aliments[i], s->quantities[i]);
	}
}

int ingredient_in_stock(struct cuisine_stock s, char *ingredient)
{
	for (int i = 0; i < s.aliments_count; i++) {
		if (strcmp(ingredient, s.aliments[i]) == 0) {
			return i;
		}
	}
	return -1;
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

            fgets(buffer, 512, file);
            while (buffer[0] != '}') {
                int n = r->plates_count;
                sscanf(buffer, "%s %d", r->recettes[n].ingredients[r->recettes[n].ingredients_count],
                       &r->recettes[n].quantities[r->recettes[n].ingredients_count]);
                
                char *ingredient=malloc(sizeof(char));
                strcpy(ingredient, r->recettes[n].ingredients[r->recettes[n].ingredients_count]);
                if (ingredient_in_stock(*s, ingredient) < 0) {
                    s->quantities[s->aliments_count] = 5;
                    strcpy(s->aliments[s->aliments_count++], ingredient);
                }
                free(ingredient);
                r->recettes[r->plates_count].ingredients_count++;
                fgets(buffer, 512, file);
            }
            r->plates_count++;
        } 
        else {
            sscanf(buffer, "%s %d" , r->recettes[r->plates_count].name, &r->recettes[r->plates_count].temps_prep);
        }
    }

    log_rest("End reading config");
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
    
    for (int i = 0; i < cmd.count; i++) {
        for (int j = 0; j < r->plates_count; j++) {
            if (strcmp(cmd.foods[i], r->recettes[j].name) == 0 && time < r->recettes[j].temps_prep) {
                time = r->recettes[j].temps_prep;
            }
        }
    }
    
    return time;
}

void add_command(struct command c, struct rest *r)
{    
    sem_wait(rest_mutex);
    rest_mem->cmd_waiting[rest_mem->cmd_count++] = c;
    sem_post(rest_mutex);
}

int rand_time(int a, int b)
{

    return rand()%(b-a) +a;
}

int number_repeice(char *name, struct rest *r)
{
	for (int n=0; n < r->plates_count; n++) {
		if (strcmp(name, r->recettes[n].name) == 0) {
		return n;
		}
	}
	return -1;
}

void add_aliments_to_cmd (struct cuisine_stock *command_tot, struct receipe recette , int commanded)
{
	for (int j=0; j < recette.ingredients_count; j++) {
		int pos = ingredient_in_stock(*command_tot, recette.name);
		if (pos < 0) {
			strcpy(command_tot->aliments[command_tot->aliments_count], recette.ingredients[j]);
			command_tot->quantities[command_tot->aliments_count++] = commanded * recette.quantities[j];
			//log_cook("ingredient ajouté : %d %s", ali_tot.quantities[ali_tot.aliments_count - 1], ali_tot.aliments[ali_tot.aliments_count - 1]);
		}
		else {
			command_tot->quantities[pos] += commanded * recette.quantities[j];
			//log_cook("ingredient maj : %d %s", ali_tot.quantities[pos], ali_tot.aliments[pos]);	
		}
	}
}

void buy_aliments_if_needed(int cmd_need, struct cuisine_stock *s, int pos)
{	
	while (cmd_need > s->quantities[pos]) {
		log_cook("need to buy %s", s->aliments[pos]);
		sleep(rand_time(10, 20));
		s->quantities[pos] += 50;					
	}
}

void pick_aliments(struct cuisine_stock command, struct cuisine_stock *s)
{
	for (int n=0; n<command.aliments_count; n++) {
		for (int i=0; i < s->aliments_count; i++) {
			if (strcmp(command.aliments[n], s->aliments[i]) == 0) {
				
				buy_aliments_if_needed(command.quantities[n], s, i);
				
				s->quantities[i] -= command.quantities[n];
			}
		}
	}	
}

void collect_ingredients(struct command cmd, struct rest *r, struct cuisine_stock *s)
{	
    struct cuisine_stock ali_tot;
    ali_tot.aliments_count = 0;
    
	/* remplissage de ali_tot */
	for (int i=0; i<cmd.count; i++) {
		int n = number_repeice(cmd.foods[i], r);
		if (n < 0) {
			log_cook_error("can't find receipe");
		}
		add_aliments_to_cmd(&ali_tot, r->recettes[n] , cmd.quantities[i]);
	
	}

	print_stock(&ali_tot);
	
	pick_aliments(ali_tot, s);
}

struct command pop_command(struct rest *r)
{
    struct command cmd = r->cmd_waiting[0];
    memcpy(r->cmd_waiting, &r->cmd_waiting[1], (--r->cmd_count) * sizeof(struct command));
    return cmd;
}

int cook_aliments(struct rest *r, struct cuisine_stock *s)
{
    sem_wait(rest_mutex);
    struct command cmd = pop_command(r);
    sem_post(rest_mutex);
    
    //log_rest("cook_aliments %d %s for %d", cmd.quantities[0], cmd.foods[0], cmd.user_pid);
    int time = time_command(r, cmd);
    log_rest("time to prepare : %d", time);
    if (time == 0) {
        log_cook_error("can't calcul command time");
        return 0;
    }
    
    struct msg_command_status rcv_command = { .dest=cmd.user_pid, .status= COMMAND_START, .time = time};
    strcpy(rcv_command.rest_name, r->name);
    msgsnd(q, &rcv_command, MSG_STATUS_COMMAND_SIZE, 0);
    log_cook("Collecting ingredients");
    collect_ingredients(cmd, r, s);
    log_cook("ingredients collectés");
    log_cook("New stock :");
    print_stock(s);
    log_cook("preparing command");
    sleep(time);
    log_cook("commande prete !");
    return 1;
}

void kitchen_process(struct cuisine_stock *s)
{    
    while(1) {
        sleep(3);
        if (rest_mem->cmd_count > 0) {
            int user_pid = rest_mem->cmd_waiting[0].user_pid;
            
            if (cook_aliments(rest_mem, s)) {
                struct msg_command_status msg = { .dest = user_pid, .status = COMMAND_COOKED, .time = 0 };
                strcpy(msg.rest_name, rest_mem->name);
                if (msgsnd(q, &msg, MSG_STATUS_COMMAND_SIZE, 0) < 0) {
                    log_cook_error("While sending COMMAND_COOKED");
                }
            }	log_cook("command sent");
        }
    }
}
int main(int argc, char **argv)
{
    signal(SIGINT, on_close);
    
    shmemid = shmget(getpid() + 1, sizeof(struct rest), IPC_CREAT | IPC_EXCL | 0666);
    if (shmemid < 0) {        
        shmemid = shmget(getpid() + 1, sizeof(struct rest), IPC_CREAT | 0666);
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
    
    rest_mem = shmat(shmemid, 0, 0);
    if (rest_mem == (void *) -1) {
        rest_panic("Can't attach shared memory");
    }
    memset(rest_mem, 0, sizeof(struct rest));
    
    struct cuisine_stock *s = malloc(sizeof(struct cuisine_stock));
    
    if (!read_config("config.txt", rest_mem, s)) {
        rest_panic("Can't read config");
        return -1;
    }
    print_rest(rest_mem);
	print_stock(s);
    
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
            
            add_command(command.command, rest_mem);
        }
    }
    free(s);
    on_close(0);
    return 0;
}
