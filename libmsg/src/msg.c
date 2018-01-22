#include <msg.h>
#include <types.h>

#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdarg.h>

int send_command_status(int q, int dest, int status, int time, const char *name)
{
	struct msg_command_status msg;
	msg.dest = dest;
	msg.status = status;
	msg.time = time;
	strncpy(msg.rest_name, name, NAME_MAX);
	
	if (msgsnd(q, &msg, MSG_STATUS_COMMAND_SIZE, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}

int recv_command_status(int q, int dest, struct msg_command_status *msg)
{
	if (msgrcv(q, msg, MSG_STATUS_COMMAND_SIZE, dest, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}

int recv_command(int q, int dest, struct command *cmd)
{
    struct msg_command msg;
    if (msgrcv(q, &msg, MSG_COMMAND_SIZE, dest, 0) < 0) {
        return 0;
    }
    
    *cmd = msg.command;
    return 1;
}

int send_command(int q, int dest, const struct command *cmd)
{
    struct msg_command msg = { dest, *cmd };
    if (msgsnd(q, &msg, MSG_COMMAND_SIZE, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}
