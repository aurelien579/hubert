#ifndef TYPES_H
#define TYPES_H

#define PANIC(msg) do {                                                \
    printf("[ERROR] %s at %s:%d\n", msg, __FILE__, __LINE__);          \
    on_close(-1);                                                      \
} while (0)                                                            \

#define MSG_SIZE(T) (sizeof(struct msg_##T) - sizeof(long))

#define NAME_MAX        20
#define REST_MAX        20
#define FOOD_MAX        20

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
#define MSG_REST_LIST           12
#define MSG_STOCK 		        14
#define MSG_LONG                15
#define MSG_REST                16
#define MSG_FOOD_LIST           17
#define COMMAND_ACK             100

struct food {
    char    name[NAME_MAX];
    int     quantity;
};

struct stock {
    int 	     count;
    struct food  foods[FOOD_MAX]; /* Array of size count */
};

struct restaurant {
    int          id;
    char         name[NAME_MAX];
    struct stock stock;
};

struct user {
    struct user    *next;
    int             id;
    char            name[NAME_MAX];
};

struct msg_state {
    long    type;
    char    name[NAME_MAX];
} __attribute__((packed));

struct msg_stock_request {
    long type;
} __attribute__((packed));

struct msg_command_announce {
    long type;
    int count;
    char restaurant_name[NAME_MAX];
} __attribute__((packed));

struct msg_long {
    long type;
    int value;
} __attribute__((packed));

struct msg_rest {
    long type;
    char name[NAME_MAX];
    int  size;
} __attribute__((packed));

struct msg_food_list {
	long type;
	struct food foods[];
} __attribute__((packed));

struct msg_restaurant_list {
    long              type;
    struct restaurant rest[];
} __attribute__((packed));

#endif
