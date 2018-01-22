#include "driver.h"

#include <types.h>
#include <msg.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdarg.h>
#include <error.h>

LOG_FUNCTIONS(driver)

#define BASE_DELIVERY_TIME  10
#define RAND_FACTOR         4

static int calculate_delivery_time(struct command *cmd)
{
    return BASE_DELIVERY_TIME;
}

static int notify_user_delivery_done(struct command *cmd)
{
    int queue = msgget(cmd->user_pid, 0);
    if (queue < 0) {
        return 0;
    }
    
    struct msg_command_status msg;
    msg.dest = cmd->user_pid;
    strcpy(msg.rest_name, cmd->name);
    msg.status = COMMAND_ARRIVED;
    msg.time = 0;
    if (msgsnd(queue, &msg, MSG_STATUS_COMMAND_SIZE, 0) < 0) {
        return 0;
    }
    
    return 1;
}

static int notify_user_delivery_begin(struct command *cmd, int delay)
{
    int queue = msgget(cmd->user_pid, 0);
    if (queue < 0) {
        return 0;
    }
    
    struct msg_command_status msg;
    msg.dest = cmd->user_pid;
    strcpy(msg.rest_name, cmd->name);
    msg.status = COMMAND_SENT;
    msg.time = delay;
    if (msgsnd(queue, &msg, MSG_STATUS_COMMAND_SIZE, 0) < 0) {
        return 0;
    }
    
    return 1;
}

static void do_delivery(struct command *cmd)
{
    sleep(BASE_DELIVERY_TIME + (rand() % RAND_FACTOR));
}

void driver_new(struct driver *self)
{
    self->pid = 0;
    self->ready = 1;
}

int driver_is_ready(struct driver *self)
{
    return self->ready;
}

void driver_deliver(struct driver *self, struct command cmd)
{
    self->pid = fork();
    if (self->pid == 0) {
        self->ready = 0;
        int delay = calculate_delivery_time(&cmd);
        
        if (!notify_user_delivery_begin(&cmd, delay)) {
            log_driver_error("Can't notify_user_delivery_begin");
        }
        
        do_delivery(&cmd);
        
        if (!notify_user_delivery_done(&cmd)) {
            log_driver_error("Can't notify_user_delivery_done");            
        }
        
        self->ready = 1;        
        exit(0);
    }
}

void driver_terminate(struct driver *self)
{
    if (!driver_is_ready(self)) {
        kill(self->pid, SIGTERM);
    }
}
