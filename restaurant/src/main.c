#include <types.h>

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

void restaurant_on_int(int n)
{
	running = 0;
}

void restaurant_send_stock(int queue)
{
}

void restaurant_process(int shmemid, int cook_pid)
{
	sem_t *mutex = sem_open("restaurant_mutex", O_CREAT, 0666, 1);
	struct restaurant *rest = shmat(shmemid, 0, 0);
	
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

	int queue = msgget(msg_status.value, 0);
	
	signal(SIGINT, restaurant_on_int);
	while (running) {
		struct msg_long request;
		msgrcv(queue, &request, MSG_SIZE(long), MSG_LONG, 0);
		
		int stock_count = rest->stock.count;
		
		struct msg_long announce = { MSG_LONG, stock_count };
		msgsnd(queue, &announce, MSG_SIZE(long), 0);

		struct msg_food_list *list = malloc(sizeof(long) + sizeof(struct food) * stock_count);
		memcpy(list->foods, rest->stock.foods, sizeof(struct food) * stock_count);
		list->type = MSG_FOOD_LIST;
		msgsnd(queue, list, sizeof(struct food) * stock_count, 0);
	}
	
	shmdt(rest);
	sem_destroy(mutex);
}

void cook_on_update(int n)
{
}

void cook_process(int shmemid)
{
	sem_t *mutex = sem_open("restaurant_mutex", O_CREAT, 0666, 1);
	struct restaurant *rest = shmat(shmemid, 0, 0);
	
	signal(SIGUSR1, cook_on_update);	
	
	sem_destroy(mutex);
	shmdt(rest);
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
	
	printf("Restaurant name : %s\n", rest->name);
	
	char buffer[512];
	
	while (fgets(buffer, 512, file) != NULL) {
		sscanf(buffer, "%s:%d",  rest->stock.foods[rest->stock.count].name,
					&rest->stock.foods[rest->stock.count].quantity);
		rest->stock.count++;
	}

	shmdt(rest);
	
	return 1;
}

int main(int argc, char **argv)
{
	int pid;
	int shmemid = shmget(SHMEM_KEY, sizeof(struct restaurant), IPC_CREAT | 0666);
	
	if (read_config("config.txt", shmemid) < 0) {
		fprintf(stderr, "Invalid config\n");
		return -1;
	}

	pid = fork();
	if (pid == 0) {
		cook_process(shmemid);
		exit(0);
	} else {
		restaurant_process(shmemid, pid);	
	}

	shmctl(shmemid, IPC_RMID, NULL);
}

