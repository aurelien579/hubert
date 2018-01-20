#ifndef UI_H
#define UI_H

#include <types.h>
#include <semaphore.h>

extern struct menu menus[RESTS_MAX];
extern int menus_count;
extern sem_t *mutex;
extern int command_progress;

enum user_state {
    CONNECTED,
    DISCONNECTED
};

struct ui;

typedef void (*on_command_t)    (struct command *, int);
typedef void (*on_close_t)      (void);
typedef void (*on_refresh_t)    (void);
typedef void (*on_connect_t)    (void);

struct ui *ui_new();
void ui_start(struct ui *self);
void ui_stop(struct ui *self);
void ui_free(struct ui *self);
void ui_set_state(struct ui *self, enum user_state state);
void ui_update_menus(struct ui *self);

void ui_set_command_status(struct ui *self, char *name, int status, int time);
void ui_set_on_refresh(struct ui *self, on_refresh_t f);
void ui_set_on_command(struct ui *self, on_command_t f);
void ui_set_on_close(struct ui *self, on_close_t f);
void ui_set_on_connect(struct ui *self, on_connect_t f);

#endif
