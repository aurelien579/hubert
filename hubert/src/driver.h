#ifndef DRIVER_H
#define DRIVER_H

#include <types.h>

struct driver {
    int     ready;
    int     pid;
};

/**
 * @brief Create a new driver. self MUST be in a shared memory segment.
 * @param self a pointer in a shared memory segment.
 * @return 
 */
void driver_new(struct driver *self);

int driver_is_ready(struct driver *self);

/**
 * @brief Start a new process for delivering a command and notify the user.
 * @param self
 * @param cmd
 */
void driver_deliver(struct driver *self, struct command cmd);

void driver_terminate(struct driver *self);

#endif
