#include <msg.h>
#include <types.h>

#include <string.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdarg.h>

int msg_send_command_status(int q, long dest, int status, int time, const char *name)
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

int msg_recv_command_status(int q, long dest, struct msg_command_status *msg)
{
	if (msgrcv(q, msg, MSG_STATUS_COMMAND_SIZE, dest, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}

int msg_recv_command(int q, long dest, struct command *cmd)
{
    struct msg_command msg;
    if (msgrcv(q, &msg, MSG_COMMAND_SIZE, dest, 0) < 0) {
        return 0;
    }
    
    *cmd = msg.command;
    return 1;
}

int msg_send_command(int q, long dest, const struct command *cmd)
{
    struct msg_command msg = { dest, *cmd };
    if (msgsnd(q, &msg, MSG_COMMAND_SIZE, 0) < 0) {
        return 0;
    } else {
        return 1;
    }
}

int msg_send_request(int queue, long dest, int request)
{
	struct msg_request msg = { dest, request };
    if (msgsnd(queue, &msg, MSG_REQUEST_SIZE, 0) < 0) {
        return 0;
    } else {
		return 1;
	}
}

int msg_recv_request(int queue, long dest, int *request)
{
	struct msg_request msg;
    if (msgrcv(queue, &msg, MSG_REQUEST_SIZE, dest, 0) < 0) {
        return 0;
    } else {
		*request = msg.type;
		return 1;
	}
}
