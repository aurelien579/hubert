#include <types.h>
#include <msg.h>

#include <ncurses.h>
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

static void client_close(int n)
{
    printf("\rClosing User\n");
    
    if (connected) {
        int queue = msgget(UBERT_KEY, 0666);
        if (queue >= 0) {
            struct msg_state disconnect_msg;
            disconnect_msg.type = MSG_USER_DISCONNECT;
            strncpy(disconnect_msg.name, username, NAME_MAX);
            
            msgsnd(queue, &disconnect_msg, MSG_STATE_SIZE, 0);            
        }
    }
    
    exit(0);
}

int main(int argc, char **argv)
{
    signal(SIGINT, client_close);
    
    int permanent_queue = msgget(UBERT_KEY, 0666);
    if (permanent_queue < 0) {
        PANIC(client_close, "Can't connect to Hubert.");
    }
    
    if (argc < 2) {
        PANIC(client_close, "Please provide a username.");
    }
    
    strncpy(username, argv[1], NAME_MAX);
    
    struct msg_state msg;
    msg.type = MSG_USER_CONNECT;
    strncpy(msg.name, username, NAME_MAX);
    msgsnd(permanent_queue, &msg, MSG_STATE_SIZE, 0);
    

    struct msg_long status;
    msgrcv(permanent_queue, &status, MSG_LONG_SIZE, MSG_USER_STATUS, 0);
    
    connected = 1;
    printf("connecté %d\n", status.value);
    
    int queue = msgget(status.value, IPC_CREAT | 0666);
    if (queue < 0) {
        PANIC(client_close, "can't open queue");
    }
    printf("%d\n", status.value);

    send_long(queue, MSG_LONG, OFFER_REQUEST);
    
    struct msg_long rest_count;
    msgrcv(queue, &rest_count, MSG_LONG_SIZE, MSG_LONG, 0);

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
    
    send_long(queue, MSG_LONG, COMMAND_ANNOUNCE);

    struct restaurant cmd_rest;
    strcpy(cmd_rest.name, "Pizza2!");
    cmd_rest.stock.count = 1;
    strcpy(cmd_rest.stock.foods[0].name, "patate");
    cmd_rest.stock.foods[0].quantity = 7;
    send_restaurant(queue, &cmd_rest);

    long ack;
    recv_long(queue, MSG_LONG, &ack);
    if (ack == COMMAND_ACK) {
        printf("commande reçue !\n");
    } else if (ack == COMMAND_NACK) {
        printf("commande non_expédiée : manque d'aliments dans le stock.\n");
    }

    client_close(0);
}
