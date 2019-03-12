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

static void ui_cli_display_header(struct ncm_ui *ui, int n_cpu)
{
	int i;
	fprintf(stdout, "CPU");

	for (i = 0; i < n_cpu; i++)
		fprintf(stdout, "\t%d", i);

	fprintf(stdout, "\n");
}

static void ui_cli_display_rxtx(struct ncm_ui *ui,
				struct ncm_stat_pcpu_rxtx *rxtx)
{
	int i;

	fprintf(stdout, "RX/TX");
	for (i = 0; i < rxtx->size; i++)
		fprintf(stdout, "\t%u/%u", rxtx->pcpu_pkts[i].rx, rxtx->pcpu_pkts[i].tx);
	fprintf(stdout, "\n");
}

int ui_cli_main(struct ncm_ui *ui)
{
	struct ncm_stat_pcpu_rxtx *rxtx = NULL;
	int ret = 0;

	ret = client_start_srv_cap(ui->client);
	if (ret) {
		fprintf(stderr, "Can't start acquisition\n");
		return ret;
	}

	ui_cli_display_header(ui, ui->client->params.n_cpus);

	while (client_is_connected(ui->client) && !ui_cli_stop) {

		rxtx = client_get_pcpu_stat(ui->client);
		if (!rxtx)
			continue;

		if (ui_cli_stop)
			goto clean;

		ui_cli_display_rxtx(ui, rxtx);

		usleep(1000000);
clean:
		if (rxtx)
			free(rxtx);

	}

	ret = client_stop_srv_cap(ui->client);
	if (ret)
		fprintf(stderr, "Can't stop acquisition\n");

	return ret;
}
