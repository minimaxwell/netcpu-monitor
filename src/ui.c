#include<string.h>
#include<stdlib.h>
#include<stdio.h>

#include "netcpu-monitor.h"
#include "ui.h"
#include "ui_cli.h"
#include "ui_oneshot.h"
#include "client.h"

struct ui_ops uis[NCM_N_UIS] = {
	{
		.init = ui_cli_init,
		.destroy = ui_cli_destroy,
		.main = ui_cli_main,
	},
	{}, /* ncurses */
	{
		.init = ui_oneshot_init,
		.destroy = ui_oneshot_destroy,
		.main = ui_oneshot_main,
		.set_param = ui_oneshot_set_param,
	}, /* oneshot */
};

struct ncm_ui *ui_create(enum ui_type type)
{
	struct ncm_ui *ui;

	ui = malloc(sizeof(*ui));
	if (!ui)
		return NULL;

	memset(ui, 0, sizeof(*ui));
	ui->type = type;
	ui->ops = &uis[type];

	if (ui_init(ui)) {
		free(ui);
		return NULL;
	}

	return ui;
}

int ui_init(struct ncm_ui *ui)
{
if (ui->ops->init)
		return ui->ops->init(ui);

	return -1;
}

void ui_destroy(struct ncm_ui *ui) {

	if (ui->ops->destroy)
		ui->ops->destroy(ui);

	free(ui);
}

int ui_run(struct ncm_ui *ui)
{
	if (ui->ops->main)
		return ui->ops->main(ui);

	return -1;
}

int ui_set_param(struct ncm_ui *ui, void *param)
{
	if (ui->ops->set_param)
		return ui->ops->set_param(ui, param);

	return 0;
}
