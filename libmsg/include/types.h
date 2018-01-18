#ifndef TYPES_H
#define TYPES_H

#define PANIC(f, msg) do {                                          \
    printf("[ERROR] %s at %s:%d\n", msg, __FILE__, __LINE__);       \
    f(-1);                                                            \
} while (0)                                                         \

#define MSG_SIZE(T) (sizeof(struct msg_##T) - sizeof(long))

#define NAME_MAX        20
#define REST_MAX        20
#define FOOD_MAX        20

#define UBERT_KEY       1
#define DRIVER_COUNT    5

/**
 * User can request the offer or send a command
 */
#define OFFER_REQUEST       1
#define COMMAND_ANNOUNCE    2

#define MSG_DELIVERY_STATUS     8
#define MSG_STOCK_ANNOUNCE      10
#define MSG_REST_LIST           11
#define MSG_STOCK               12
#define MSG_FOOD_LIST           17
#define COMMAND_ACK             100
#define COMMAND_NACK            101

#define STOCK_REQUEST           18
#define COMMAND                 19

struct food {
    char    name[NAME_MAX];
    int     quantity;
};

struct stock {
    int         count;
    struct food foods[FOOD_MAX]; /* Array of size count */
};

struct restaurant {
    int          id;
    char         name[NAME_MAX];
    struct stock stock;
};

struct user {
    struct user    *next;
    int             id;
    int             pid;
    char            name[NAME_MAX];
};

/** 
 * A client (eg. a restaurant or a user) connects/distconnects to hubert by
 * sending a msg_state. The client must provides its name int the message.
 * Hubert then responds with a msg_long containing the dedicated message queue
 * id. When an error occured, the value is -1.
 */
#define MSG_USER_CONNECT        1
#define MSG_USER_DISCONNECT     2
#define MSG_REST_REGISTER       3
#define MSG_REST_UNREGISTER     4
#define MSG_STATE_SIZE         (sizeof(char) * NAME_MAX)
struct msg_state {
    long    type;
    char    name[NAME_MAX];
};

/** 
 *  msg_long is used to transfer a single long value.
 */
#define MSG_USER_STATUS         5
#define MSG_REST_STATUS         6
#define MSG_LONG                15
#define MSG_LONG_SIZE          (sizeof(long))
struct msg_long {
    long type;
    long value;
};


/** 
 *  msg_rest is used 
 */
#define MSG_REST                16
#define MSG_REST_SIZE          (sizeof(char) * NAME_MAX + sizeof(int))
struct msg_rest {
    long type;
    char name[NAME_MAX];
    int  size;
} __attribute__((packed));


struct msg_food_list {
    long        type;
    struct food foods[];
};

#endif
