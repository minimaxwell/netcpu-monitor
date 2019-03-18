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
	NCM_UI_ONESHOT,

	/* Last entry */
	NCM_N_UIS,
};

struct ui_ops {
	int (*init)(struct ncm_ui *ui);
	void (*destroy)(struct ncm_ui *ui);
	int (*main)(struct ncm_ui *ui);
	int (*set_param)(struct ncm_ui *ui, void *param);
};

struct ncm_ui {
	void *priv;
	enum ui_type type;
	struct ncm_client *client;
	struct ui_ops *ops;
};

struct ncm_ui *ui_create(enum ui_type);
int ui_init(struct ncm_ui *ui);
void ui_destroy(struct ncm_ui *ui);
int ui_run(struct ncm_ui *ui);
int ui_set_param(struct ncm_ui *ui, void *param);

/* For u32, we need to scale up to uin64_t temporarily */
#define U32_PER_SEC(u32count, us) ( (uint64_t)(u32count) * 1000000 / (us))
#define U64_PER_SEC(u64count, us) ( (u64count) * 1000000 / (us))

#endif
