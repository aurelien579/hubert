#include <types.h>
#include <msg.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>

static int connected = 0;
static char username[NAME_MAX];

static void on_close(int n)
{
    printf("\rClosing User\n");
    
    if (connected) {
        int queue = msgget(UBERT_KEY, 0666);
        if (queue >= 0) {        
            struct msg_state disconnect_msg;
            disconnect_msg.type = MSG_USER_DISCONNECT;
            strncpy(disconnect_msg.name, username, NAME_MAX);
            
            msgsnd(queue, &disconnect_msg, MSG_SIZE(state), 0);            
        }
    }
    
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, on_close);
    
    int permanent_queue = msgget(UBERT_KEY, 0666);
    if (permanent_queue < 0) {
        PANIC("Can't connect to Hubert.");
    }
    
    if (argc < 2) {
        PANIC("Please provide a username.");
    }
    
    strncpy(username, argv[1], NAME_MAX);
    
    struct msg_state msg;
    msg.type = MSG_USER_CONNECT;
    strncpy(msg.name, username, NAME_MAX);
    msgsnd(permanent_queue, &msg, MSG_SIZE(state), 0);
    connected = 1;
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
        struct restaurant rest;
        recv_restaurant(queue, &rest);

        printf("REST : %s\n", rest.name);

        for (int i=0; i < rest.stock.count; i++){
            printf("food : %s   quantity : %d\n", rest.stock.foods[i].name,
                                                  rest.stock.foods[i].quantity);
        }
    }

    rest_count.value = MSG_COMMAND_ANNOUNCE;
    msgsnd(queue, &rest_count, MSG_SIZE(long), 0);
    printf("long send\n");

    struct restaurant cmd_rest;
    strcpy(cmd_rest.name, "Pizza!");
    cmd_rest.stock.count = 1;
    strcpy(cmd_rest.stock.foods[0].name, "patate");
    cmd_rest.stock.foods[0].quantity = 7;
    send_restaurant(queue, &cmd_rest);

    struct msg_long msg_ack;
    msgrcv(queue, &msg_ack, MSG_SIZE(long), MSG_LONG, 0);
    if (msg_ack.value == COMMAND_ACK) {
        printf("commande reçue !\n");
    }
    else if (msg_ack.value == COMMAND_NACK) {
        printf("commande non_expédiée : manque d'aliments dans le stock.\n");
    }

    on_close(0);
}
