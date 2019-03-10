#include "ui.h"

#ifndef __IF_CLI__
#define __IF_CLI__

int ui_cli_init(struct ncm_ui *ui);
void ui_cli_destroy(struct ncm_ui *ui);
void ui_cli_on_params_update(struct ncm_ui *ui, struct ncm_parameters *params);
void ui_cli_on_connect(struct ncm_ui *ui);
void ui_cli_on_disconnect(struct ncm_ui *ui);

#endif
