#include <types.h>
#include <msg.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdarg.h>
#include <error.h>
#include <time.h>

#define ALIMENTS_MAX    15
#define COMMAND_MAX     10
#define RAND_FACTOR     5
#define TIME_TO_BUY     16

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
static struct rest *rest_mem = NULL; /* mémoire partagé restaurant-cuisine */

static sem_t *rest_mutex = NULL;	 /* sémaphore partagé restaurant-cuisine */
static sem_t *hubert_mutex = NULL;	 /* sémaphore partagé hubert-restaurant */
static int shmemid = -1;			 /* id mémoire partagé restaurant-cuisine */
static int shmemid_hubert = -1;		 /* id mémoire partagé hubert-restaurant */
static int q = -1;					 /* id file de message avec hubert */

static void on_close(int n);

LOG_FUNCTIONS(rest)
LOG_FUNCTIONS(cook)
PANIC_FUNCTION(rest, on_close)

static void disconnect()
{/* fonction de déconnection du restaurant à hubert */
    struct msg_state msg = { HUBERT_DEST, REST_DISCONNECT, getpid() };
    msgsnd(q, &msg, MSG_STATE_SIZE, 0);
}

static void on_close(int n)
{/* fonction de fermeture du processus restaurant */
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

void print_rest()
{/* fonction d'affichage de la fiche restaurant */
	sem_wait(rest_mutex);
    printf("\nRestaurant : %s, nb aliments : %d\n", rest_mem->name, rest_mem->plates_count);
    for (int i = 0; i < rest_mem->plates_count; i++) {
        printf("\nPlat : %s           temps de préparation : %d\nAliments necessaires :\n",
               rest_mem->recettes[i].name, rest_mem->recettes[i].temps_prep);
        for (int j = 0; j < rest_mem->recettes[i].ingredients_count; j++) {
            printf("     %d %s", rest_mem->recettes[i].quantities[j],
                                 rest_mem->recettes[i].ingredients[j]);
        }
    }
    sem_post(rest_mutex);
    printf("\n");
}

void print_stock( struct cuisine_stock *s)
{/* fonction d'affichage du stock restaurant */
	for (int i=0; i < s->aliments_count; i++) {
		printf("%s : %d\n", s->aliments[i], s->quantities[i]);
	}
}

int ingredient_in_stock(struct cuisine_stock s, char *ingredient)
{/* fonction de vérification de l'existence de l'ingrédient dans le stock */
	for (int i = 0; i < s.aliments_count; i++) {
		if (strcmp(ingredient, s.aliments[i]) == 0) {
			return i;
		}
	}
	return -1;
}

int read_config(int nb_args, char *arg, struct cuisine_stock *s)
{/* fonction de récupération des informations associées au restaurant */
	char f[20] = "config1.txt";
    if (nb_args > 2) {
		rest_panic("too much arguments");
	}
	else if (nb_args == 2) {
		strcpy(f, arg);
	}
	/* par défaut ouvre config1.txt, mais possible de rentrer une autre */
	/* fiche restaurant si elle respecte les conventions de forme */
    
    FILE *file = fopen(f, "r");
    if (file == NULL) {
        return -1;
    }
	
	struct rest *r = rest_mem;
	
    if (fgets(r->name, NAME_MAX, file) == NULL) {
        shmdt(r);
        return -1;
    }

    r->name[strlen(r->name) - 1] = '\0';

    log_rest("Restaurant name : %s", r->name);

    char buffer[512];
	log_rest("buffer ok");
	
    while (fgets(buffer, 512, file) != NULL) {
        if (buffer[0] == '{') {
		log_rest("test");
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
    if (s->aliments_count > ALIMENTS_MAX) {
		log_rest_panic("Too much aliments in restaurant");
		on_close(-1);
	}
	
    printf("Stock :\n\n");
	print_stock(s);
	log_rest("End reading config"); 
    
    return 1;
}

int update_hubert_mem()
{/* fonction de préparation du menu */
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
{/* fonction de connection du restaurant à hubert */
    int key = getpid();
    struct msg_state msg = { HUBERT_DEST, REST_CONNECT, key };
    msgsnd(q, &msg, MSG_STATE_SIZE, 0);
    
    struct msg_status status;
    msgrcv(q, &status, MSG_STATUS_SIZE, key, 0);
    
    return status.status;
}

int time_command(struct command cmd)
{/* fonction de calcul du temps necesaire à la préparation d'une commande */
    int time = 0;
    
    for (int i = 0; i < cmd.count; i++) {
		sem_wait(rest_mutex);
        for (int j = 0; j < rest_mem->plates_count; j++) {
            if (strcmp(cmd.foods[i], rest_mem->recettes[j].name) == 0 && time < rest_mem->recettes[j].temps_prep) {
                time = rest_mem->recettes[j].temps_prep;
            }
        }
        sem_post(rest_mutex);
    }
    
    return time;	// renvoie le temps maximum de préparation vis à vis de chacun des plats
}

void add_command(struct command c)
{/* fonction d'ajout d'une comande recu à la liste des commandes à traiter */
    sem_wait(rest_mutex);
    if (rest_mem->cmd_count == COMMAND_MAX) {
		log_rest_panic("can't add new command : too much command");
	} else {
		rest_mem->cmd_waiting[rest_mem->cmd_count++] = c;
		sem_post(rest_mutex);
		log_rest("add command %d", c.user_pid);
	}
}

int rand_time(int a, int b)
{/* fonction de calcul d'un temps aléatoire */
    return rand()%(b-a) +a;
}

int find_repeice(char *name)
{/* fonction de recherche de la recette associé au nom donné en parametre */
	sem_wait(rest_mutex);
	for (int n=0; n < rest_mem->plates_count; n++) {
		if (strcmp(name, rest_mem->recettes[n].name) == 0) {
		sem_post(rest_mutex);
		return n;
		}
	}
	sem_post(rest_mutex);
	return -1;
}

void add_aliments_to_cmd(struct cuisine_stock *command_tot, struct receipe recette , int commanded)
{/* fonction permettant de connaitre l'ensemble des ingrédients necessaires à une commande */
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

void buy_aliments_missing(struct cuisine_stock alims, struct cuisine_stock *s)
{/* fonction d'achat des aliments manquant à une commande */
	int missing_count = 0;
	for (int n=0; n<alims.aliments_count; n++) {
		for (int i=0; i < s->aliments_count; i++) {
			if (strcmp(alims.aliments[n], s->aliments[i]) == 0) {
				while (alims.quantities[n] > s->quantities[i]) {
					log_cook("buying %s...", s->aliments[i]);
					s->quantities[i] += 50;
					missing_count++;
				}
			}
		}
	}
	if (missing_count > 0) {
		sleep(rand_time(TIME_TO_BUY, TIME_TO_BUY + RAND_FACTOR));
	}
}

void collect_aliments(struct cuisine_stock aliments_needed, struct cuisine_stock *s)
{/* fonction de collecte des ingrédients dans le stock */
	for (int n=0; n<aliments_needed.aliments_count; n++) {
		for (int i=0; i < s->aliments_count; i++) {
			if (strcmp(aliments_needed.aliments[n], s->aliments[i]) == 0) {
			
				s->quantities[i] -= aliments_needed.quantities[n];
			}
		}
	}	
}

void prepare_ingredients(struct command cmd, struct cuisine_stock *s)
{/* fonction de preparation des ingrédients necessaires à la commande */
    struct cuisine_stock aliments_needed;
    aliments_needed.aliments_count = 0;
    
	/* remplissage de aliments_needed */
	for (int i=0; i<cmd.count; i++) {
		
		int n = find_repeice(cmd.foods[i]);
		if ( n < 0) {
			log_cook_error("can't find receipe");
		}
		struct receipe recette = rest_mem->recettes[n];
		add_aliments_to_cmd(&aliments_needed, recette, cmd.quantities[i]);
	
	}
	//printf("Aliments needed for command %d :\n", cmd.user_pid);
	//print_stock(&aliments_needed);
	
	buy_aliments_missing(aliments_needed, s);
	
	collect_aliments(aliments_needed, s);
}

struct command pop_command()
{/* fonction de récupération de la plus vieille commande client dans la liste des commandes */        
	sem_wait(rest_mutex);
    struct command cmd = rest_mem->cmd_waiting[0];
    memcpy(rest_mem->cmd_waiting, &rest_mem->cmd_waiting[1], (--rest_mem->cmd_count) * sizeof(struct command));
    sem_post(rest_mutex);
    return cmd;
}

int cook_aliments(struct cuisine_stock *s)
{/* fonction de preparation des commandes clients */
    
    struct command cmd = pop_command();
    log_cook("starting command %d", cmd.user_pid);
    int time = time_command(cmd) + rand_time(0, RAND_FACTOR);
    log_cook("time to prepare : %d", time);
    if (time == 0) {
        log_cook_error("can't calcul command time");
        return 0;
    }
    
    if (!msg_send_command_status(q, cmd.user_pid, COMMAND_START, time, cmd.name))
		log_cook_error("Sending COMMAND_START to %d", cmd.user_pid);

    log_cook("Command %d : Collecting ingredients...", cmd.user_pid);
    prepare_ingredients(cmd, s);
    log_cook("Command %d : Ingredients collected",cmd.user_pid);
    log_cook("New stock :");
    print_stock(s);
    log_cook("Command %d : Preparing command...", cmd.user_pid);
    sleep(time);
    log_cook("Command %d : Command ready !", cmd.user_pid);
    return 1;
}

void kitchen_process(struct cuisine_stock *s)
{/* fonction gérant la cuisine */
    while(1) {
        sem_wait(rest_mutex);
        if (rest_mem->cmd_count > 0) {
            int user_pid = rest_mem->cmd_waiting[0].user_pid;
            char name_rest[NAME_MAX];
            strcpy(name_rest, rest_mem->cmd_waiting[0].name);
			sem_post(rest_mutex);
            if (cook_aliments(s)) {
            	if (!msg_send_command_status(q, user_pid, COMMAND_COOKED, 0, name_rest)) {
				log_cook_error("send command status to hubert");
				}
				log_cook("Command sent");
			}
        } else {
			sem_post(rest_mutex);
		}
    }
}

void create_shmem_and_sem() 
{/* fonction d'initialisation des mémoires partagées (cuisine-rest & rest-hubert) avec les sémaphores associés */
	shmemid = shmget(getpid() + 1, sizeof(struct rest), IPC_CREAT | IPC_EXCL | 0666);
    if (shmemid < 0) {        
        shmemid = shmget(getpid() + 1, sizeof(struct rest), IPC_CREAT | 0666);
        shmctl(shmemid, IPC_RMID, NULL);
        rest_panic("Can't open shmemid");
    }
    
    shmemid_hubert = shmget(getpid(), sizeof(struct menu), IPC_CREAT | IPC_EXCL | 0666);
    if (shmemid_hubert < 0) {
        rest_panic("Can't open shmemid_hubert");
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
        rest_panic("Can't open rest_mutex");
    }
}

int main(int argc, char *argv[])
{/* fonction principale du processus */
    signal(SIGINT, on_close);
    
    create_shmem_and_sem();
    
    rest_mem = shmat(shmemid, 0, 0);
    if (rest_mem == (void *) -1) {
        rest_panic("Can't attach shared memory");
    }
    memset(rest_mem, 0, sizeof(struct rest));
    
    struct cuisine_stock *s = malloc(sizeof(struct cuisine_stock));
    
    if (!read_config(argc, argv[1], s)) {
        rest_panic("Can't read config");
        return -1;
    }
    
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
            struct command cmd;
			log_rest("Reading command...", getpid());
            if (!msg_recv_command(q, getpid(), &cmd)) {
				log_rest("hubert diconnected");
				on_close(-1);
			}
            
            add_command(cmd);
			
            if (!msg_send_command_status(q, cmd.user_pid, COMMAND_RECEIVED, 0, cmd.name)) {
				log_rest_panic("hubert disconnected");
				exit(-1);
			}
        }
    }
    free(s);
    on_close(0);
    return 0;
}
