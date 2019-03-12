#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>

#include "ui_cli.h"
#include "client.h"
#include "ui.h"

volatile sig_atomic_t ui_cli_stop = 0;

void ui_cli_sigint_handler(int signal) {
	ui_cli_stop = 1;
}

int ui_cli_init(struct ncm_ui *ui)
{
	struct sigaction sa;

	sa.sa_handler = &ui_cli_sigint_handler;
	sa.sa_flags = SA_RESTART;
	sigfillset(&sa.sa_mask);

	return sigaction(SIGINT, &sa, NULL);
}

void ui_cli_destroy(struct ncm_ui *ui)
{
	return;
}

static void ui_cli_display_rxtx(struct ncm_ui *ui,
				struct ncm_stat_pcpu_rxtx *rxtx, bool rx)
{
	int i;
	for (i = 0; i < rxtx->size; i++)
		fprintf(stdout, "%s CPU %d : %u\n", rx ? "RX" : "TX", i,
			rxtx->pcpu_pkts[i]);
}

int ui_cli_main(struct ncm_ui *ui)
{
	struct ncm_stat_pcpu_rxtx *rx = NULL, *tx = NULL;
	int ret = 0;

	while (client_is_connected(ui->client) && !ui_cli_stop) {

		rx = client_get_pcpu_stat(ui->client, true);
		if (!rx)
			continue;

		if (ui_cli_stop)
			goto clean;

		tx = client_get_pcpu_stat(ui->client, false);
		if (!tx)
			goto clean;

		if (ui_cli_stop)
			goto clean;

		ui_cli_display_rxtx(ui, rx, true);
		ui_cli_display_rxtx(ui, tx, false);

		usleep(1000000);
clean:
		if (rx)
			free(rx);
		if(tx)
			free(tx);

	}

	return ret;
}
