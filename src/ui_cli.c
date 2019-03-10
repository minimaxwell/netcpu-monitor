#include<stdio.h>

#include "ui_cli.h"
#include "ui.h"

static void ui_cli_on_pcpu_rx_update(struct ncm_ui *ui,
				     struct ncm_stat_pcpu_rxtx *rxtx)
{
	int i;

	for (i = 0; i < rxtx->size; i++)
		fprintf(stdout, "RX CPU %d : %u\n", i, rxtx->pcpu_pkts[i]);
}

static void ui_cli_on_pcpu_tx_update(struct ncm_ui *ui,
				     struct ncm_stat_pcpu_rxtx *rxtx)
{

}

int ui_cli_init(struct ncm_ui *ui)
{
	return 0;
}

void ui_cli_destroy(struct ncm_ui *ui)
{

}

void ui_cli_on_params_update(struct ncm_ui *ui, struct ncm_parameters *params)
{
	fprintf(stdout, "New params : n_cpus = %d, cpu_map = 0x%08x\n",
		params->n_cpus, params->cpu_map);
}

void ui_cli_on_connect(struct ncm_ui *ui)
{
	fprintf(stdout, "%s\n", __func__);

}

void ui_cli_on_disconnect(struct ncm_ui *ui)
{
	fprintf(stdout, "%s\n", __func__);
}

