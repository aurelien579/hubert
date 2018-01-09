#ifndef TYPES_H
#define TYPES_H

#define NAME_MAX 42

struct food {
    char    name[NAME_MAX];
    int     quantity;
};

struct stock {
    int          count;
    struct food  *foods; /* Array of size count */
};

struct restaurant {
    int                  id;
    char                 name[NAME_MAX];
    struct stock         stock;
    struct restaurant   *next;
};

struct user {
    int             id;
    int             pid;
    char            name[NAME_MAX];
    struct user    *next;
};

struct msg_status {
    long    type;
    int     id;
};

struct msg_state {
    long    type;
    char    name[NAME_MAX];
};

struct msg_offer_request {
    long type;
};

struct msg_delivery_status {
    long type;
    int done;
};

struct stock_request {
    long type;
};

struct msg_command_announce {
    long type;
    int count;
    char restaurant_name[NAME_MAX];
};

struct msg_stock_annouce {
    long type;
    int size;
};    

struct msg_food_list {
    long type;
    struct food foods[];
};

#endif
