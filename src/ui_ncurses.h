#ifndef __IF_NCURSES__
#define __IF_NCURSES__

#include "ui.h"

int ui_ncurses_init(struct ncm_ui *ui);
void ui_ncurses_destroy(struct ncm_ui *ui);
int ui_ncurses_main(struct ncm_ui *ui);

#endif
