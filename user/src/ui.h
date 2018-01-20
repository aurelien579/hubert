#ifndef UI_H
#define UI_H

#include <types.h>

struct ui;

typedef void (*on_command_t)    (struct command *, int);
typedef void (*on_close_t)      (void);
typedef void (*on_refresh_t)    (void);

struct ui *ui_new();
void ui_start(struct ui *self);
void ui_free(struct ui *self);
void ui_update_menus(struct ui *self, const struct menu *menus, int c);

void ui_set_on_refresh(struct ui *self, on_refresh_t f);
void ui_set_on_command(struct ui *self, on_command_t f);
void ui_set_on_close(struct ui *self, on_close_t f);

#endif
