#ifndef MSG_H
#define MSG_H

void print_rest(struct restaurant *rest);
struct food *find_food(struct restaurant *rest, char *name);
int is_command_valid(struct restaurant *rest, struct stock *command);
void update_stock(struct restaurant *rest, struct stock *command, int flag);


/* All these functions return 0 on error and 1 on sucess. */
int recv_state_and_type(int queue, long *type, char *name, int len);
int send_state(int queue, long type, const char *name, int len);
int recv_long_and_type(int queue, long *type, long *value);
int recv_long(int queue, long type, long *value);
int send_long(int queue, long type, long value);

void send_restaurant(int user_queue, struct restaurant *rest);
void recv_restaurant(int queue, struct restaurant *rest);
void recv_stock(int queue, int size, struct stock *s);

#endif
