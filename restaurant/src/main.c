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

struct cuisine_stock {
<<<<<<< HEAD
	char foods[ALIMENTS_MAX][NAME_MAX];
	int quantities[ALIMENTS_MAX];
	int food_count;
=======
    char aliments[ALIMENTS_MAX][NAME_MAX];
    int quantities[ALIMENTS_MAX];
    int aliments_count;
>>>>>>> d2330f7ddd4fdc60ae931ee0f2dc94d4294d2e6f
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
<<<<<<< HEAD
    int     		plates_count;
=======
    int             foods_count;
>>>>>>> d2330f7ddd4fdc60ae931ee0f2dc94d4294d2e6f

};

static sem_t *hubert_mutex = NULL;
static int shmemid = -1;
static int shmemid_hubert = -1;
static int q = -1;

static void on_close(int n);

LOG_FUNCTIONS(rest)
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
    
    if (q >= 0) {
        disconnect();
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
<<<<<<< HEAD
			//printf("checking aliment\n");
			fgets(buffer, 512, file);
			while (buffer[0] != '}') {
				int n = r->plates_count;
				sscanf(buffer, "%s %d", r->recettes[n].ingredients[r->recettes[n].ingredients_count],
					   &r->recettes[n].quantities[r->recettes[n].ingredients_count]);
				
				/*char ingredient[NAME_MAX] = r->recettes[n].ingredients[r->recettes[n].ingredients_count];
				if (!ingredient_in_stock(s, ingredient) {
					s->quantities[food_count] = 5;
					strcpy(s->foods[food_count++], ingrediant);
				}*/
				
				r->recettes[r->plates_count].ingredients_count++;
				fgets(buffer, 512, file);
			}
			r->plates_count++;
        }
        else {
			sscanf(buffer, "%s %d" , r->recettes[r->plates_count].name, &r->recettes[r->plates_count].temps_prep);
		}
=======
            //printf("checking aliment\n");
            fgets(buffer, 512, file);
            while (buffer[0] != '}') {
                
                sscanf(buffer, "%s %d", r->recettes[r->foods_count].ingredients[r->recettes[r->foods_count].ingredients_count],
                       &r->recettes[r->foods_count].quantities[r->recettes[r->foods_count].ingredients_count]);

                r->recettes[r->foods_count].ingredients_count++;
                fgets(buffer, 512, file);
            }
            r->foods_count++;
        }
        else {
            sscanf(buffer, "%s %d" , r->recettes[r->foods_count].name, &r->recettes[r->foods_count].temps_prep);
        }
>>>>>>> d2330f7ddd4fdc60ae931ee0f2dc94d4294d2e6f
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

int time_max_command(struct msg_command *cmd, struct rest *r) {
	int time = 0;
	for (int i = 0; i < cmd->foods_count; cmd->foods_count++) {
		while (strcmp(cmd->)
	
	
}

int main(int argc, char **argv)
{
    signal(SIGINT, on_close);
    
    shmemid = shmget(HUBERT_KEY, sizeof(struct rest), IPC_CREAT | IPC_EXCL | 0666);
    if (shmemid < 0) {
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
    
    struct rest *r = shmat(shmemid, 0, 0);
    struct cuisine_stock s;
    
    if (!read_config("config.txt", r, &s)) {
        rest_panic("Can't read config");
        return -1;
    }
    shmdt(r);
    //(unsigned long)time(NULL)
    if (fork() == 0) {
		cichen_process(s);
	}
	else {
		while(1) {
		struct msg_command command;
		msgrcv(HUBERT_DEST, &command, MSG_COMMAND_SIZE, getpid(), 0);
		struct rest *r = shmat(shmemid, 0, 0);
		int time = time_max_command(&command, r);
		
		}
	}	

    if (!update_hubert_mem()) {
        rest_panic("Can't initialy update the menu");        
    }
    
    q = msgget(HUBERT_KEY, 0);
    if (q <= 0) {
        rest_panic("Can't msgget");
    }
    
    int status = connect();
    if (status != STATUS_OK) {
        rest_panic("Can't connect to hubert. Status : %d", status);
    }
    
<<<<<<< HEAD
    
    
=======
    while (1);
>>>>>>> d2330f7ddd4fdc60ae931ee0f2dc94d4294d2e6f
    
    on_close(0);
    return 0;
}
