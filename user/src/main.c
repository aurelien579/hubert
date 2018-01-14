#include <types.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

static void on_close(int n)
{
	exit(0);
}

int main(int argc, char **argv)
{
	int permanent_queue = msgget(UBERT_KEY, 0666);
	if (permanent_queue < 0) {
		PANIC("msgget");
	}
	
	struct msg_state msg;
	msg.type = MSG_USER_CONNECT;
	strncpy(msg.name, argv[1], NAME_MAX);
	msgsnd(permanent_queue, &msg, sizeof(msg.name), 0);
	printf("connecté\n");
	
	struct msg_status status;
	msgrcv(permanent_queue, &status, sizeof(int), MSG_USER_STATUS, 0);
	printf("queue : %d\n", status.id);
	int queue = msgget(status.id, 0666);
	
	struct msg_type msg_type;
	msg_type.type = MSG_TYPE;
	msg_type.next_type = MSG_OFFER_REQUEST;
	msgsnd(queue, &msg_type, sizeof(msg_type.next_type), 0);

	struct msg_stock_announce announce;
	msgrcv(queue, &announce, MSG_SIZE(stock_announce), MSG_STOCK_ANNOUNCE, 0);
	
        struct msg_restaurant_list *menu = malloc(sizeof(long) + announce.rests_size);
	msgrcv(queue, menu, announce.rests_size, MSG_REST_LIST, 0);

	int i;
	for (i = 0; i < announce.count; i++) {
		printf("restaurant : %s\n", menu->rests[i].name);
	}

	sleep(10);
	
	msg.type = MSG_USER_DISCONNECT;
	msgsnd(permanent_queue, &msg, sizeof(msg.name), 0);
	
	return 0;	
}
