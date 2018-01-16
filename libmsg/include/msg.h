#ifndef MSG_H
#define MSG_H

void print_rest(struct restaurant *rest);
struct food *find_food(struct restaurant *rest, char *name);
int is_command_valid(struct restaurant *rest, struct stock *command);
void update_stock(struct restaurant *rest, struct stock *command, int flag);

void send_restaurant(int user_queue, struct restaurant *rest);
void recv_restaurant(int queue, struct restaurant *rest);
void recv_stock(int queue, int size, struct stock *s);

#endif
