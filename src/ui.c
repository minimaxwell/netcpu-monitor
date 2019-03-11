#include<string.h>
#include<stdlib.h>
#include<stdio.h>

#include "netcpu-monitor.h"
#include "ui.h"
#include "ui_cli.h"
#include "client.h"

struct ncm_ui *ui_create(enum ui_type type)
{
	struct ncm_ui *ui;

	ui = malloc(sizeof(*ui));
	if (!ui)
		return NULL;

	memset(ui, 0, sizeof(*ui));
	ui->type = type;

	return ui;
}

int ui_init(struct ncm_ui *ui, struct ncm_client *c)
{
	ui->client = c;
	switch(ui->type) {
	case NCM_UI_CLI:
		return ui_cli_init(ui);
	case NCM_UI_NCURSES:
	case NCM_UI_ONESHOT_START:
	case NCM_UI_ONESHOT_STOP:
	default:
		fprintf(stderr, "Unsupported UI\n");
	}
	return -1;
}

void ui_destroy(struct ncm_ui *ui) {
	switch(ui->type) {
	case NCM_UI_CLI:
		return ui_cli_destroy(ui);
	case NCM_UI_NCURSES:
	case NCM_UI_ONESHOT_START:
	case NCM_UI_ONESHOT_STOP:
	default:
		fprintf(stderr, "Unsupported UI\n");
	}

	free(ui);
}

int ui_run(struct ncm_ui *ui)
{
	switch(ui->type) {
	case NCM_UI_CLI:
		return ui_cli_main(ui);
	case NCM_UI_NCURSES:
	case NCM_UI_ONESHOT_START:
	case NCM_UI_ONESHOT_STOP:
	default:
		fprintf(stderr, "Unsupported UI\n");
	}

	return -1;
}
