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

#define C_BG COLOR_BLACK

#define C_RED 1
#define C_GREEN 2
#define C_YELLOW 3
#define C_BLUE 4
#define C_MAGENTA 5
#define C_CYAN 6
#define C_WHITE 7

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
	int true_percent;
	uint32_t value;
	uint32_t drops;
	uint32_t true_value;
	char label[9];
};

struct ui_rxtx_window {
	enum ui_rxtx_window_type type;
	WINDOW *win;
	struct ui_coord coord;
	char label[8]; /* "ingress", "egress", "total" */
	uint64_t n_per_sec;
	uint64_t drop_per_sec;
	uint64_t total_per_sec;

	/* a rxtx window has one graph per cpu */
	int n_cpus;
	struct ui_bargraph *graphs;
	uint64_t *pcpu_count;
	uint64_t *true_count;
	uint64_t *drop_count;
};

struct ui_rxtx_graphs {
	struct ui_rxtx_window ui_rxtx_graphs[UI_N_RXTX];
	struct ui_coord coord;
};

struct ui_ncurses {
	struct ui_rxtx_graphs graphs;
	struct ui_coord coord;
};

static void ui_init_colors(void) {
	init_pair(C_RED, COLOR_RED, C_BG);
	init_pair(C_GREEN, COLOR_GREEN, C_BG);
	init_pair(C_YELLOW, COLOR_YELLOW, C_BG);
	init_pair(C_BLUE, COLOR_BLUE, C_BG);
	init_pair(C_MAGENTA, COLOR_MAGENTA, C_BG);
	init_pair(C_CYAN, COLOR_CYAN, C_BG);
	init_pair(C_WHITE, COLOR_WHITE, C_BG);
}

static int ui_barsize_get(int window_width)
{
	int barsize = window_width - 23;

	if (barsize > 20)
		barsize = 20;

	return barsize;
}

int ui_bargraph_draw(WINDOW *w, struct ui_bargraph *bg)
{
	int n_bars, n_red_bars, i;
	int barsize = ui_barsize_get(bg->coord.w);

	n_bars = (bg->percent * barsize) / 100;
	n_red_bars = (bg->true_percent * barsize) / 100;

	wmove(w, bg->coord.y, bg->coord.x);

	wattron(w, COLOR_PAIR(C_CYAN));
	wprintw(w, " %-2s ", bg->label);
	wattroff(w, COLOR_PAIR(C_CYAN));

	if (bg->drops)
		wattron(w, COLOR_PAIR(C_YELLOW));

	waddch(w, '[' | A_BOLD);
	waddch(w, ' ');

	for (i = 0; i < barsize; i++)
		if (i < n_bars) {
			waddch(w, '|');
		} else if (i < n_red_bars) {
			waddch(w, '|' | COLOR_PAIR(C_RED));
		} else {
			waddch(w, ' ');
		}

	waddch(w, ' ');
	waddch(w, ']' | A_BOLD);
	waddch(w, ' ');

	wprintw(w, "%-3d", bg->percent);
	waddch(w, ' ');
	wprintw(w, "%-10u", bg->value);

	if (bg->drops)
		wattroff(w, COLOR_PAIR(C_YELLOW));

	return 0;
}

static void ui_display_header(struct ncm_ui *ui)
{
	struct ncm_client *c = ui->client;

	wmove(stdscr, 1, 1);
	wprintw(stdscr, "Monitoring ");

	wattron(stdscr, COLOR_PAIR(C_YELLOW));
	wprintw(stdscr, "%s", c->params.iface);
	wattroff(stdscr, COLOR_PAIR(C_YELLOW));

	wprintw(stdscr, " on ");
	wattron(stdscr, COLOR_PAIR(C_GREEN));
	wprintw(stdscr, "%s", c->server_addr ? c->server_addr : "localhost");
	wattroff(stdscr, COLOR_PAIR(C_GREEN));

	wmove(stdscr, 2, 0);
	wclrtoeol(stdscr);
	wmove(stdscr, 0, 0);
}

static int ui_windows_draw(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	struct ui_rxtx_window *rtw;
	struct ui_bargraph *bg;
	struct ui_coord new_coord;
	int i, j, barsize;
	int n_cols = 1;
	int header_height = 3;

	int width = unc->coord.w;
	if (width > 53)
		width = 53;

	if (unc->coord.w > width * 2)
		n_cols = 2;

	ui_display_header(ui);

	for (i = 0; i < UI_N_RXTX; i++) {
		rtw = &unc->graphs.ui_rxtx_graphs[i];

		new_coord.w = width;
		new_coord.h = rtw->n_cpus + 3;

		/* Hack. I'm not good with responsive design */
		new_coord.x = width * ( i % n_cols ) + (i % n_cols);
		new_coord.y = new_coord.h * (i / n_cols) + header_height;

		barsize = ui_barsize_get(new_coord.w - 2);

		if (rtw->coord.x != new_coord.x ||
		    rtw->coord.y != new_coord.y ||
		    rtw->coord.w != new_coord.w ||
		    rtw->coord.h != new_coord.h) {

			rtw->coord = new_coord;

			if (rtw->win) {
				wclear(rtw->win);
				wborder(rtw->win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
				wrefresh(rtw->win);
				delwin(rtw->win);
			}

			rtw->win = newwin(rtw->coord.h, rtw->coord.w,
					  rtw->coord.y, rtw->coord.x);
		}

		/* For some reason, there are glitches when resizing that forces
		 * us to redraw menus and stuff at each iteration
		 */
		box(rtw->win, 0, 0);
		mvwprintw(rtw->win, 0, 8, "%s traffic", rtw->label);

		wattron(rtw->win, COLOR_PAIR(C_CYAN) | A_BOLD);

		mvwprintw(rtw->win, 1, 1, "CPU");
		mvwprintw(rtw->win, 1, barsize + 10, "%%");
		mvwprintw(rtw->win, 1, barsize + 14, "pps");

		wattroff(rtw->win, COLOR_PAIR(C_CYAN) | A_BOLD);

		for (j = 0; j < rtw->n_cpus; j++) {
			bg = &rtw->graphs[j];

			bg->coord.w = rtw->coord.w - 2;
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
	uint64_t *pcpu_counters;
	uint64_t *true_counters;
	uint64_t *drop_counters;
	int i, j;

	for (i = 0; i < UI_N_RXTX; i++) {
		rtw = &unc->graphs.ui_rxtx_graphs[i];
		rtw->n_cpus = ui->client->params.n_cpus;

		graphs = malloc(rtw->n_cpus * sizeof(struct ui_bargraph));
		if (!graphs)
			return -1;
		memset(graphs, 0, rtw->n_cpus * sizeof(struct ui_bargraph));

		pcpu_counters = malloc(rtw->n_cpus * sizeof(uint64_t));
		if (!pcpu_counters)
			return -1;

		true_counters = malloc(rtw->n_cpus * sizeof(uint64_t));
		if (!true_counters)
			return -1;

		drop_counters = malloc(rtw->n_cpus * sizeof(uint64_t));
		if (!drop_counters)
			return -1;

		rtw->graphs = graphs;
		rtw->pcpu_count = pcpu_counters;
		rtw->true_count = true_counters;
		rtw->drop_count = drop_counters;

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
			sprintf(graphs[j].label, "%d", j);

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
		free(unc->graphs.ui_rxtx_graphs[i].true_count);
		free(unc->graphs.ui_rxtx_graphs[i].drop_count);
	}

	free(unc);
}

static int ui_ncurses_update_graph(struct ui_rxtx_window * rtw)
{
	struct ui_bargraph *bg;
	int j;

	for (j = 0; j < rtw->n_cpus; j++) {
		bg = &rtw->graphs[j];

		bg->drops = 0;
		bg->true_value = 0;
		bg->true_percent = 0;

		if (rtw->type == UI_RXTX && rtw->drop_count[j]) {

			bg->percent = ( rtw->pcpu_count[j] * 100 ) / rtw->total_per_sec;
			bg->true_percent = ( rtw->true_count[j] * 100 ) / rtw->total_per_sec;

		} else if (rtw->n_per_sec) {

			bg->percent = ( rtw->pcpu_count[j] * 100 ) / rtw->n_per_sec;

		} else {

			bg->percent = 0;

		}

		bg->value = rtw->pcpu_count[j];

		bg->drops = rtw->drop_count[j];

		if (rtw->type == UI_RXTX)
			bg->true_value = rtw->true_count[j];
	}

	return 0;
}

static int ui_ncurses_update_graphs(struct ncm_ui *ui,
				    struct ncm_stat_pcpu_rxtx *rxtx,
				    struct timespec *ts)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	struct ui_rxtx_window *rtw_rx, *rtw_tx, *rtw_rxtx;
	uint64_t us = ( ts->tv_sec * 1000000 ) + ( ts->tv_nsec / 1000 );
	int j;

	uint64_t total_rx = 0, total_tx = 0;
	uint64_t total_drop = 0, total_total = 0;

	rtw_rx = &unc->graphs.ui_rxtx_graphs[UI_RX];
	rtw_tx = &unc->graphs.ui_rxtx_graphs[UI_TX];
	rtw_rxtx = &unc->graphs.ui_rxtx_graphs[UI_RXTX];

	/* Accumulate everything */
	for (j = 0; j < rtw_rx->n_cpus; j++) {
		total_rx += rxtx->pcpu_pkts[j].rx;
		total_tx += rxtx->pcpu_pkts[j].tx;
		total_drop += rxtx->pcpu_pkts[j].drops;
		total_total += rxtx->pcpu_pkts[j].total;

		rtw_rx->pcpu_count[j] = U32_PER_SEC(rxtx->pcpu_pkts[j].rx, us);
		rtw_tx->pcpu_count[j] = U32_PER_SEC(rxtx->pcpu_pkts[j].tx, us);
		rtw_rxtx->pcpu_count[j] = rtw_rx->pcpu_count[j] + rtw_tx->pcpu_count[j];
		rtw_rxtx->true_count[j] = U32_PER_SEC(rxtx->pcpu_pkts[j].total, us);

		/* drop and total are only meaningful for rxtx, but we still need
		 * to know if there's drop occuring on a socket*/
		rtw_rx->drop_count[j] = U32_PER_SEC(rxtx->pcpu_pkts[j].drops, us);
		rtw_tx->drop_count[j] = rtw_rx->drop_count[j];
		rtw_rxtx->drop_count[j] = rtw_rx->drop_count[j];
	}

	rtw_rx->n_per_sec = U64_PER_SEC(total_rx, us);
	rtw_tx->n_per_sec = U64_PER_SEC(total_tx, us);

	rtw_rxtx->n_per_sec = rtw_rx->n_per_sec + rtw_tx->n_per_sec;
	rtw_rxtx->drop_per_sec = U64_PER_SEC(total_drop, us);
	rtw_rxtx->total_per_sec = U64_PER_SEC(total_total, us);

	ui_ncurses_update_graph(rtw_rx);
	ui_ncurses_update_graph(rtw_tx);
	ui_ncurses_update_graph(rtw_rxtx);

	return 0;
}

static bool ui_ncurses_stop = false;

static void * ui_ncurses_main_loop(void *priv)
{
	struct ncm_ui *ui = (struct ncm_ui *)priv;
	struct ncm_stat_pcpu_rxtx *rxtx = NULL;
	struct timespec ts;
	int ret = 0;

	ret = client_start_srv_cap(ui->client);
	if (ret) {
		fprintf(stderr, "Can't start acquisition\n");
		return NULL;
	}

	while (client_is_connected(ui->client) && !ui_ncurses_stop) {

		/* We're not interested in the timestamp */
		rxtx = client_get_pcpu_stat(ui->client, &ts);
		if (!rxtx)
			continue;

		if (ui_ncurses_stop)
			goto clean;

		ui_ncurses_update_graphs(ui, rxtx, &ts);

		usleep(100000);
clean:
		if (rxtx)
			free(rxtx);

	}

	if (client_is_connected(ui->client)) {
		ret = client_stop_srv_cap(ui->client);
		if (ret)
			fprintf(stderr, "Can't stop acquisition\n");
	}

	return NULL;
}

int ui_ncurses_main(struct ncm_ui *ui)
{
	struct ui_ncurses *unc = (struct ui_ncurses *) ui->priv;
	int width, height, ret, key;
	pthread_t t;
	pthread_attr_t attr;

	if (ui_graphs_alloc(ui))
		return -1;

	/* Initial ncurses configuration */
	initscr();
	clear();
	raw();
	keypad(stdscr, TRUE);
	noecho();
	/* getch timeouts in 10ms */
	timeout(10);

	if (has_colors()) {
		start_color();
		ui_init_colors();
	}

	/* Make the cursor invisible */
	curs_set(0);
	getmaxyx(stdscr, height, width);

	unc->coord.w = width;
	unc->coord.h = height;

	if (width < 30)
		goto cleanup;

	refresh();
	ui_windows_draw(ui);

	/* Create the worker thread that will update the values used by the
	 * UI.
	 */
	pthread_attr_init(&attr);
	ret = pthread_create(&t, &attr, ui_ncurses_main_loop, ui);
	if (ret)
		goto cleanup;

	/* Main loop */
	while(true) {
		key = getch();

		if (key == KEY_RESIZE) {
			getmaxyx(stdscr, height, width);
			if (height != unc->coord.h || width != unc->coord.w) {
				clear();
				unc->coord.w = width;
				unc->coord.h = height;
			}
		}

		ui_windows_draw(ui);
		refresh();
		if (key == 'Q' || key == 'q')
			break;
	}

	ui_ncurses_stop = true;

	pthread_join(t, NULL);

cleanup:
	ui_windows_cleanup(ui);

	curs_set(1);
	endwin();
	return 0;
}

