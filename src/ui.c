#include<string.h>
#include<stdlib.h>

#include "netcpu-monitor.h"
#include "ui.h"
#include "ui_cli.h"

#define N_UIS 2
struct ncm_ui_ops uis[NCM_N_UIS] = {
	/* Simple console client interface */
	{
		.init = ui_cli_init,
		.destroy = ui_cli_destroy,
		.on_params_update = ui_cli_on_params_update,
		.on_connect = ui_cli_on_connect,
		.on_disconnect = ui_cli_on_disconnect,
	},
	/* Ncurses client interface */
#ifdef CONFIG_NCURSES
	{
	},
#else
	{},
#endif
};

struct ncm_ui *ui_create(enum ui_type type)
{
	struct ncm_ui *ui;

	if (!uis[type].init)
		return NULL;

	ui = malloc(sizeof(*ui));
	if (!ui)
		return NULL;

	memset(ui, 0, sizeof(*ui));

	return ui;
}

int ui_init(struct ncm_ui *ui, struct ncm_client *c)
{
	ui->client = c;
	return ui->ops->init(ui);
}

void ui_destroy(struct ncm_ui *ui)
{
	ui->ops->destroy(ui);

	free(ui);
}

void ui_notify_params_update(struct ncm_ui *ui, struct ncm_parameters *params)
{
	ui->ops->on_params_update(ui, params);
}

void ui_notify_stats_update(struct ncm_ui *ui, struct ncm_stat *stat)
{
	ncm_stat_cb_t cb = NULL;

	if (ui->stat_cbs[stat->type])
		cb = ui->stat_cbs[stat->type];

	cb(ui, &stat->buf[0]);
}

void ui_notify_connect(struct ncm_ui *ui)
{
	ui->ops->on_connect(ui);
}

void ui_notify_disconnect(struct ncm_ui *ui)
{
	ui->ops->on_disconnect(ui);
}

int ui_register_stat_callback(struct ncm_ui *ui, enum stat_type type,
			      ncm_stat_cb_t cb)
{
	if (ui->stat_cbs[type])
		return -1;

	ui->stat_cbs[type] = cb;
}

