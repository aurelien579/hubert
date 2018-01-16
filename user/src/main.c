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
	
	struct msg_long status;
	msgrcv(permanent_queue, &status, MSG_SIZE(long), MSG_USER_STATUS, 0);
    
	int queue = msgget(status.value, IPC_CREAT | 0666);	
    if (queue < 0) {
        PANIC("can't open queue");
    }
    printf("%d\n", status.value);
       
    struct msg_long request;
    request.type = MSG_LONG;
    request.value = MSG_OFFER_REQUEST;
    msgsnd(queue, &request, sizeof(int), 0);
    
    struct msg_long rest_count;
    msgrcv(queue, &rest_count, MSG_SIZE(long), MSG_LONG, 0);    
    
    printf("rest_count %ld\n", rest_count.value);
    
    for (int i = 0; i < rest_count.value; i++) {
        struct msg_rest msg_rest;              
        msgrcv(queue, &msg_rest, MSG_SIZE(rest), MSG_REST, 0);
        
        int size = msg_rest.size;
        
        struct msg_food_list *msg_list = malloc(sizeof(long) + size);
        msgrcv(queue, msg_list, size, MSG_FOOD_LIST, 0);  
        
        int nb_aliments = msg_rest.size / sizeof(struct food);
        printf("REST : %s\n", msg_rest.name);
        
        for (int i=0; i < nb_aliments; i++){
            printf("food : %s   quantity : %d\n", msg_list->foods[i].name, 
                                                  msg_list->foods[i].quantity);
        }
                      
        free(msg_list);
    }    
    rest_count.value = MSG_COMMAND_ANNOUNCE;
    msgsnd(queue, &rest_count, MSG_SIZE(long), 0);
    printf("long send\n");
    
    struct msg_command_announce command_announce = { MSG_COMMAND_ANNOUNCE, 1 };
    strcpy(command_announce.restaurant_name, "Name test");
    msgsnd(queue, &command_announce, MSG_SIZE(command_announce), 0);
    printf("command announce send\n");
    
    struct msg_food_list command = { MSG_FOOD_LIST };
    strcpy( command.foods[0].name, "patate");
    command.foods[0].quantity = 7;
    int message_size = command_announce.count * sizeof(struct food);
    msgsnd(queue, &command, message_size, 0);
    printf("command send\n");
    
    struct msg_long msg_ack;
    msgrcv(queue, &msg_ack, MSG_SIZE(long), MSG_LONG, 0);
    if (msg_ack.value == COMMAND_ACK) {
        printf("commande reçue !\n");
    }
    else if (msg_ack.value == COMMAND_NACK) {
        printf("commande non_expédiée : manque d'aliments dans le stock.\n");
    }
        
    return 0;
	sleep(1);
	
	msg.type = MSG_USER_DISCONNECT;
	msgsnd(permanent_queue, &msg, sizeof(msg.name), 0);
    
	
	return 0;	
}
