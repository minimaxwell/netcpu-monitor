#ifndef __UI_ONESHOT__
#define __UI_ONESHOT__

#include "ui.h"

int ui_oneshot_init(struct ncm_ui *ui);
void ui_oneshot_destroy(struct ncm_ui *ui);
int ui_oneshot_main(struct ncm_ui *ui);
int ui_oneshot_set_param(struct ncm_ui *ui, void *param);

#endif
