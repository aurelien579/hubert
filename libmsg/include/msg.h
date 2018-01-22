#ifndef MSG_H
#define MSG_H

#include <types.h>

int msg_send_command_status(int q, long dest, int status, int time, const char *name);
int msg_recv_command_status(int q, long dest, struct msg_command_status *msg);

int msg_send_command(int q, long dest, const struct command *cmd);
int msg_recv_command(int q, long dest, struct command *cmd);

int msg_send_request(int queue, long dest, int request);
int msg_recv_request(int queue, long dest, int *request);

#endif
