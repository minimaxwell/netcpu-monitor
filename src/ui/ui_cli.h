#include "ui.h"

#ifndef __IF_CLI__
#define __IF_CLI__

int ui_cli_init(struct ncm_ui *ui);
void ui_cli_destroy(struct ncm_ui *ui);
int ui_cli_main(struct ncm_ui *ui);

#endif
