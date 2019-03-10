#include "netcpu-monitor.h"
#include "stat.h"

#ifdef CONFIG_NCURSES
#include "ui_ncurses.h"
#endif

#ifndef __NCM_IF__
#define __NCM_IF__

enum ui_type {
	NCM_UI_CLI,
	NCM_UI_NCURSES,

	/* Last entry */
	NCM_N_UIS,
};

typedef void (*ncm_stat_cb_t) (struct ncm_ui *ui, void *stat);

struct ncm_ui {
	void *priv;
	ncm_stat_cb_t stat_cbs[NCM_N_STATS];
	struct ncm_ui_ops *ops;
	struct ncm_client *client;
};

struct ncm_ui_ops {
	int (*init) (struct ncm_ui *ui);
	void (*destroy) (struct ncm_ui *ui);
	void (*on_params_update) (struct ncm_ui *ui,
				  struct ncm_parameters *params);
	void (*on_connect) (struct ncm_ui *ui);
	void (*on_disconnect) (struct ncm_ui *ui);
};

struct ncm_ui *ui_create(enum ui_type);
int ui_init(struct ncm_ui *ui, struct ncm_client *c);
void ui_destroy(struct ncm_ui *ui);
void ui_notify_params_update(struct ncm_ui *ui, struct ncm_parameters *params);
void ui_notify_stats_update(struct ncm_ui *ui, struct ncm_stat *stat);
void ui_notify_connect(struct ncm_ui *ui);
void ui_notify_disconnect(struct ncm_ui *ui);

int ui_register_stat_callback(struct ncm_ui *ui, enum stat_type type,
			      ncm_stat_cb_t cb);

#endif
