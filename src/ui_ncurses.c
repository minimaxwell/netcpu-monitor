#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<time.h>
#include<ncurses.h>
#include<pthread.h>

#include "ui.h"
#include "ui_ncurses.h"
#include "client.h"

enum ui_rxtx_window_type {
	UI_RX,
	UI_TX,
	UI_RXTX,
	UI_N_RXTX,
};

struct ui_coord {
	int w;
	int h;
	int x;
	int y;
};

struct ui_bargraph {
	struct ui_coord coord;
	int percent;
	uint32_t value;
	char label[9];
};

struct ui_rxtx_window {
	enum ui_rxtx_window_type type;
	WINDOW *win;
	struct ui_coord coord;
	char label[8]; /* "ingress", "egress", "total" */
	uint64_t n_per_sec;

	/* a rxtx window has one graph per cpu */
	int n_cpus;
	struct ui_bargraph *graphs;
	uint32_t *pcpu_count;
};

struct ui_rxtx_graphs {
	struct ui_rxtx_window ui_rxtx_graphs[UI_N_RXTX];
	struct ui_coord coord;
};

struct ui_ncurses {
	struct ui_rxtx_graphs graphs;
	struct ui_coord coord;
};

int ui_bargraph_draw(WINDOW *w, struct ui_bargraph *bg)
{
	int n_bars, i;
	int barsize = bg->coord.w - 35;
	if (barsize > 50)
		barsize = 50;

	n_bars = (bg->percent * barsize) / 100;

	mvwprintw(w, bg->coord.y, bg->coord.x, " %s ", bg->label);
	mvwprintw(w, bg->coord.y, bg->coord.x + 10, "[");

	for (i = 0; i < barsize; i++)
		if (i < n_bars)
			mvwprintw(w, bg->coord.y, bg->coord.x + 11 + i, "|");

	mvwprintw(w, bg->coord.y, bg->coord.x + barsize + 11, "]");
	mvwprintw(w, bg->coord.y, bg->coord.x + barsize + 13, "%d %%", bg->percent);
	mvwprintw(w, bg->coord.y, bg->coord.x + barsize + 19, "%u", bg->value);
	mvwprintw(w, bg->coord.y, bg->coord.x + barsize + 29, "pkts/s");

	return 0;
}

int ui_windows_init(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	struct ui_rxtx_window *rtw;
	struct ui_bargraph *bg;
	int i, j;

	for (i = 0; i < UI_N_RXTX; i++) {
		rtw = &unc->graphs.ui_rxtx_graphs[i];

		rtw->coord.w = unc->coord.w;
		rtw->coord.h = rtw->n_cpus + 4;
		rtw->coord.x = 0;
		rtw->coord.y = rtw->coord.h * i;

		rtw->win = newwin(rtw->coord.h, rtw->coord.w,
				  rtw->coord.y, rtw->coord.x);

		box(rtw->win, 0, 0);
		mvwprintw(rtw->win, 0, 2, "%s traffic", rtw->label);

		for (j = 0; j < rtw->n_cpus; j++) {
			bg = &rtw->graphs[j];

			bg->coord.w = rtw->coord.w - 30;
			bg->coord.h = 1;
			bg->coord.x = 1;
			bg->coord.y = j + 2;
			ui_bargraph_draw(rtw->win, bg);
		}
		wrefresh(rtw->win);
	}

	return 0;
}

int ui_windows_cleanup(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	struct ui_rxtx_window *rtw;
	int i;

	for (i = 0; i < UI_N_RXTX; i++) {
		rtw = &unc->graphs.ui_rxtx_graphs[i];
		delwin(rtw->win);
	}

	return 0;
}

int ui_graphs_alloc(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	struct ui_rxtx_window *rtw;
	struct ui_bargraph *graphs;
	uint32_t *pcpu_counters;
	int i, j;

	for (i = 0; i < UI_N_RXTX; i++) {
		rtw = &unc->graphs.ui_rxtx_graphs[i];
		rtw->n_cpus = ui->client->params.n_cpus;

		graphs = malloc(rtw->n_cpus * sizeof(struct ui_bargraph));
		if (!graphs)
			return -1;

		pcpu_counters = malloc(rtw->n_cpus * sizeof(uint32_t));
		if (!pcpu_counters)
			return -1;

		rtw->graphs = graphs;
		rtw->pcpu_count = pcpu_counters;

		switch(i) {
		case 0:
			strncpy(rtw->label, "ingress", 8);
			rtw->type = UI_RX;
			break;
		case 1:
			strncpy(rtw->label, "egress", 7);
			rtw->type = UI_TX;
			break;
		case 2:
			strncpy(rtw->label, "total", 6);
			rtw->type = UI_RXTX;
			break;
		}

		for (j = 0; j < rtw->n_cpus; j++)
			sprintf(graphs[j].label, "CPU %d", j);

	}

	return 0;
}

int ui_ncurses_init(struct ncm_ui *ui)
{
	struct ui_ncurses *unc;

	unc = malloc(sizeof(*unc));
	if (!unc)
		return -1;

	memset(unc, 0, sizeof(*unc));

	ui->priv = unc;

	return 0;
}

void ui_ncurses_destroy(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	int i;

	for (i = 0; i < UI_N_RXTX; i++) {
		free(unc->graphs.ui_rxtx_graphs[i].graphs);
		free(unc->graphs.ui_rxtx_graphs[i].pcpu_count);
	}

	free(unc);
}

int ui_ncurses_update_graphs(struct ncm_ui *ui, struct ncm_stat_pcpu_rxtx *rxtx)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	struct ui_rxtx_window *rtw_rx, *rtw_tx, *rtw_rxtx;
	int j;

	uint64_t total_rx = 0, total_tx = 0;

	rtw_rx = &unc->graphs.ui_rxtx_graphs[UI_RX];
	rtw_tx = &unc->graphs.ui_rxtx_graphs[UI_TX];
	rtw_rxtx = &unc->graphs.ui_rxtx_graphs[UI_RXTX];

	rtw_rx->n_per_sec = 0;
	rtw_tx->n_per_sec = 0;
	rtw_rxtx->n_per_sec = 0;

	/* Accumulate everything */
	for (j = 0; j < rtw_rx->n_cpus; j++) {
		total_rx += rxtx->pcpu_pkts[j].rx;
		total_tx += rxtx->pcpu_pkts[j].rx;

	}

	for (j = 0; j < rtw_rx->n_cpus; j++) {
	}

	return 0;
}

static bool ui_ncurses_stop = false;

static void * ui_ncurses_main_loop(void *priv)
{
	struct ncm_ui *ui = (struct ncm_ui *)priv;
	struct ncm_stat_pcpu_rxtx *rxtx = NULL;
	int ret = 0;

	ret = client_start_srv_cap(ui->client);
	if (ret) {
		fprintf(stderr, "Can't start acquisition\n");
		return NULL;
	}

	while (client_is_connected(ui->client) && !ui_ncurses_stop) {

		/* We're not interested in the timestamp */
		rxtx = client_get_pcpu_stat(ui->client, NULL);
		if (!rxtx)
			continue;

		if (ui_ncurses_stop)
			goto clean;

		/* do the thing */

		usleep(100000);
clean:
		if (rxtx)
			free(rxtx);

	}

	ret = client_stop_srv_cap(ui->client);
	if (ret)
		fprintf(stderr, "Can't stop acquisition\n");

	return NULL;
}

int ui_ncurses_main(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	int width, height, ret;
	pthread_t t;
	pthread_attr_t attr;

	if (ui_graphs_alloc(ui))
		return -1;

	/* Initial ncurses configuration */
	initscr();
	raw();
	keypad(stdscr, TRUE);
	noecho();

	/* Make the cursor invisible */
	curs_set(0);
	getmaxyx(stdscr, height, width);

	unc->coord.w = width;
	unc->coord.h = height;

	if (width < 30)
		goto cleanup;

	refresh();
	ui_windows_init(ui);

	pthread_attr_init(&attr);
	ret = pthread_create(&t, &attr, ui_ncurses_main_loop, ui);
	if (ret)
		goto cleanup;

	/* Main loop */
	getch();

	ui_ncurses_stop = true;

	pthread_join(t, NULL);

cleanup:

	ui_windows_cleanup(ui);

	endwin();
	return 0;
}

