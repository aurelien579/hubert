#ifndef TYPES_H
#define TYPES_H

#define NAME_MAX        20
#define USER_MAX        20
#define RESTS_MAX       10
#define FOODS_MAX       10

#define HUBERT_KEY      50
#define HUBERT_DEST     1

struct menu {
    char name[NAME_MAX];
    char foods[FOODS_MAX][NAME_MAX];
    int foods_count;
};

struct command {
    char name[NAME_MAX];
    char foods[FOODS_MAX][NAME_MAX];
    char quantities[FOODS_MAX];
    int count;
};

enum state_type {
    USER_CONNECT,
    USER_DISCONNECT,
    REST_CONNECT,
    REST_DISCONNECT
};

#define MSG_STATE_SIZE  (sizeof(struct msg_state) - sizeof(long))
struct msg_state {
    long dest;
    enum state_type type;
    int key;
};

#define STATUS_OK       1
#define STATUS_EXISTS   2
#define STATUS_FULL     3
#define STATUS_ERROR    4
#define MSG_STATUS_SIZE (sizeof(int))
struct msg_status {
    long dest;
    int status;
};

struct msg_menus {
    long dest;
    struct menu menus[RESTS_MAX];
    long menus_count;
};
#define MSG_MENUS_SIZE  (sizeof(struct msg_menus) - sizeof(long))

enum menu_request_type {
    MENU_REQUEST,
    COMMAND_REQUEST,
};

struct msg_request {
    long dest;
    long type;
};
#define MSG_REQUEST_SIZE  (sizeof(long))

struct msg_command {
    long dest;
    struct command command;
};

#endif
