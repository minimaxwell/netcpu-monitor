#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>

#include "ui.h"
#include "ui_oneshot.h"
#include "client.h"

enum ncm_ui_oneshot_cmd {
	NCM_UI_ONESHOT_START,
	NCM_UI_ONESHOT_STOP,
	NCM_UI_ONESHOT_SNAPSHOT,
};

struct ui_oneshot {
	enum ncm_ui_oneshot_cmd cmd;
};

int ui_oneshot_init(struct ncm_ui *ui)
{
	struct ui_oneshot *ui_os;

	ui_os = malloc(sizeof(*ui_os));
	if (!ui_os)
		return -1;

	ui->priv = ui_os;
	return 0;
}

void ui_oneshot_destroy(struct ncm_ui *ui)
{
	struct ui_oneshot *ui_os = (struct ui_oneshot *) ui->priv;

	free(ui_os);
}

void ui_oneshot_display_pcpu_rxtx(struct ncm_ui *ui,
				  struct ncm_stat_pcpu_rxtx *rxtx,
				  struct timespec *ts)
{
	int i;
	fprintf(stdout,
		"Capture summary on %d cores, duration %ld.%lu seconds :\n",
		rxtx->size, ts->tv_sec, ts->tv_nsec);

	for (i = 0; i < rxtx->size; i++) {
		fprintf(stdout, "CPU%d: %d packets received, %d packets sent\n",
			i, rxtx->pcpu_pkts[i].rx, rxtx->pcpu_pkts[i].tx);
	}
}

int ui_oneshot_main(struct ncm_ui *ui)
{
	struct ui_oneshot *ui_os = (struct ui_oneshot *) ui->priv;
	struct ncm_stat_pcpu_rxtx *rxtx = NULL;
	struct timespec ts;

	switch (ui_os->cmd) {
	case NCM_UI_ONESHOT_START:
		return client_start_srv_cap(ui->client);
	case NCM_UI_ONESHOT_STOP:
		return client_stop_srv_cap(ui->client);
	case NCM_UI_ONESHOT_SNAPSHOT:
		rxtx = client_get_pcpu_stat(ui->client, &ts);
		if(rxtx)
			ui_oneshot_display_pcpu_rxtx(ui, rxtx, &ts);
		else
			fprintf(stderr, "Could not read snapshot\n");
		free(rxtx);
		break;
	}

	return 0;
}

int ui_oneshot_set_param(struct ncm_ui *ui, void *param)
{
	struct ui_oneshot *ui_os = (struct ui_oneshot *) ui->priv;
	char *cmd_str = (char *)param;
	int ret = 0;

	if (!cmd_str)
		return -1;

	if (!strcmp(cmd_str, "start"))
		ui_os->cmd = NCM_UI_ONESHOT_START;
	else if (!strcmp(cmd_str, "stop"))
		ui_os->cmd = NCM_UI_ONESHOT_STOP;
	else if (!strcmp(cmd_str, "snapshot"))
		ui_os->cmd = NCM_UI_ONESHOT_SNAPSHOT;
	else
		return -1;

	return ret;
}

