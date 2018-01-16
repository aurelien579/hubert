#include <types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/msg.h>

void print_rest(struct restaurant *rest)
{
    printf("Restaurant : %s, nb aliments : %d\n", rest->name, rest->stock.count);
    for (int i = 0; i < rest->stock.count; i++) {
        printf("Aliment : %s quantity %d\n",
               rest->stock.foods[i].name,
               rest->stock.foods[i].quantity);
    }
}

struct food *find_food(struct restaurant *rest, char *name)
{    
    for (int i = 0; i < rest->stock.count; i++) {
        if (strncmp(rest->stock.foods[i].name, name, NAME_MAX) == 0) {
            return &rest->stock.foods[i];
        }
    }
    
    return NULL;
}

int is_command_valid(struct restaurant *rest, struct stock *command)
{
    for (int i = 0; i < command->count; i++) {
        struct food *food = find_food(rest, command->foods[i].name);
        if (food == NULL) {
            return -1;
        }
        
        int food_stocked = food->quantity;
        int food_commanded = command->foods[i].quantity;
        if (food_stocked < food_commanded) {
            return  -1;
        }
    }
    
    return 1;
}

void update_stock(struct restaurant *rest, struct stock *command, int flag)
{
    for (int i = 0; i < command->count; i++) {
        struct food *food = find_food(rest, command->foods[i].name);
        
        if (food == NULL && flag > 0) {
            rest->stock.foods[rest->stock.count] = command->foods[i];
            food = &rest->stock.foods[rest->stock.count++];
        }
        else if (food == NULL && flag == 0) {
            printf("BREAK\n");
            break;
        }
        
        int food_commanded = command->foods[i].quantity;
        
        if (flag == 0) {
            food->quantity -= food_commanded;
        } else {
            food->quantity = food_commanded;
        }
    }
}
void send_restaurant(int user_queue, struct restaurant *rest)
{    
    int food_list_size =  rest->stock.count * sizeof(struct food);
    struct msg_rest msg_rest = { .type = MSG_REST, .size = food_list_size };
    strcpy(msg_rest.name, rest->name);                
    msgsnd(user_queue, &msg_rest, MSG_SIZE(rest), 0);
    
    struct msg_food_list *msg_list = malloc(sizeof(long) + food_list_size);
    msg_list->type = MSG_FOOD_LIST;
    memcpy(msg_list->foods, rest->stock.foods, food_list_size);
    msgsnd(user_queue, msg_list, food_list_size, 0);                
    free(msg_list);
}

void recv_stock(int queue, int size, struct stock *s)
{
    struct msg_food_list *msg_list = malloc(sizeof(long) + size);
    msgrcv(queue, msg_list, size, MSG_FOOD_LIST, 0);
    
    s->count = size / sizeof(struct food);
    memcpy(s->foods, msg_list->foods, size);
    
    free(msg_list);
}

void recv_restaurant(int queue, struct restaurant *rest)
{
    struct msg_rest msg_rest;              
    msgrcv(queue, &msg_rest, MSG_SIZE(rest), MSG_REST, 0);
    strcpy(rest->name, msg_rest.name);  
    recv_stock(queue, msg_rest.size, &rest->stock);
}
