#ifndef MSG_H
#define MSG_H

#include <types.h>

int send_command_status(int q, int dest, int status, int time, const char *name);
int recv_command_status(int q, int dest, struct msg_command_status *msg);

int send_command(int q, int dest, const struct command *cmd);
int recv_command(int q, int dest, struct command *cmd);

#endif
