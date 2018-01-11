#ifndef TYPES_H
#define TYPES_H

#define PANIC(msg) do {                                                \
    printf("[ERROR] %s at %s:%d\n", msg, __FILE__, __LINE__);          \
} while (0)                                                            \

#define NAME_MAX        42
#define UBERT_KEY       1
#define DRIVER_COUNT    5

#define MSG_USER_CONNECT        1
#define MSG_USER_DISCONNECT     2
#define MSG_REST_REGISTER       3
#define MSG_REST_UNREGISTER     4
#define MSG_USER_STATUS         5
#define MSG_REST_STATUS         6
#define MSG_OFFER_REQUEST       7
#define MSG_DELIVERY_STATUS     8
#define MSG_STOCK_REQUEST       9
#define MSG_COMMAND_ANNOUNCE    10
#define MSG_STOCK_ANNOUNCE      11
#define MSG_FOOD_LIST           12

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
