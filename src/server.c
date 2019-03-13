#define _GNU_SOURCE
#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<sched.h>
#include<signal.h>
#include<time.h>

#include "stat.h"
#include "server.h"
#include "monitor.h"
#include "connector.h"

struct ncm_server {
	bool local;
	struct ncm_connector *con;
	struct ncm_parameters params;
	struct ncm_monitor *mon;
};

volatile sig_atomic_t server_stop = 0;

static void server_set_params(struct ncm_server *s, struct ncm_parameters *p)
{
	s->params.cpu_map &= p->cpu_map;

	/* Only one interface watched at a time for now */
	strncpy(s->params.iface, p->iface, 16);

	/* Reconfigure monitors if necessary */
}

static void server_get_params(struct ncm_server *s)
{
	struct ncm_message *msg;

	msg = malloc(sizeof(*msg) + sizeof(s->params));
	msg->type = NCM_MSG_RESP_GET_PARAMS;
	msg->len = sizeof(s->params);
	memcpy(msg->buf, &s->params, sizeof(s->params));

	if (connector_send(s->con, msg)) {
		fprintf(stderr, "Error responding to get_params request\n");
	}

	free(msg);
}

static void server_start_cap(struct ncm_server *s, struct ncm_parameters *p)
{
	if (ncm_monitor_start_cap(s->mon, p))
		fprintf(stderr, "Error starting capture\n");
}

static void server_stop_cap(struct ncm_server *s, struct ncm_parameters *p)
{
	if (ncm_monitor_stop_cap(s->mon))
		fprintf(stderr, "Error stop capture\n");
}

static struct ncm_stat *server_get_stat_pcpu_rxtx(struct ncm_server *s)
{
	struct ncm_stat_pcpu_rxtx *rxtx;
	struct ncm_stat *stat;
	struct timespec ts;

	rxtx = ncm_monitor_snapshot(s->mon, &ts);
	if (!rxtx) {
		fprintf(stderr, "Error taking stats snapshot\n");
		return NULL;
	}

	stat = malloc(sizeof(*stat) + sizeof(*rxtx) +
		      rxtx->size * sizeof(struct ncm_stats_pcpu_rxtx_entry));
	if (!stat)
		goto clean_stat;

	stat->type = NCM_STAT_PCPU_RXTX;
	stat->size = sizeof(*rxtx) + rxtx->size *
		     sizeof(struct ncm_stats_pcpu_rxtx_entry);
	stat->ts = ts;
	memcpy(stat->buf, rxtx, stat->size);

clean_stat:
	free(rxtx);

	return stat;
}

static void server_get_stat(struct ncm_server *s, struct ncm_stat_req *req)
{
	struct ncm_message *msg = NULL;
	struct ncm_stat *stat;

	switch (req->type) {
	case NCM_STAT_PCPU_RXTX:
		stat = server_get_stat_pcpu_rxtx(s);
		break;
	default:
		return;
	}

	if (!stat)
		return;

	msg = malloc(sizeof(*msg) + sizeof(*stat) + stat->size);
	if (!msg)
		goto stat_err;
	msg->type = NCM_MSG_RESP_GET_STATS;
	msg->len = sizeof(*stat) + stat->size;
	memcpy(msg->buf, stat, msg->len);

	if (connector_send(s->con, msg))
		fprintf(stderr, "Error responding to get_stats request\n");

	free(msg);
stat_err:
	free(stat);
}

static int server_main_loop(struct ncm_server *s)
{
	struct ncm_message *msg = NULL;

	while (!server_stop) {

		if (connector_connect(s->con)) {
			fprintf(stderr, "Connection error\n");
			break;
		}

		fprintf(stdout, "Incoming connection\n");

		while (connector_is_link_ok(s->con) && !server_stop) {
			msg = connector_receive(s->con);
			if (!msg)
				continue;

			/* Process message */
			switch (msg->type) {
			case NCM_MSG_SET_PARAMS :
				server_set_params(s, (struct ncm_parameters *) msg->buf);
				break;
			case NCM_MSG_GET_PARAMS :
				server_get_params(s);
				break;
			case NCM_MSG_START_CAP :
				server_start_cap(s, (struct ncm_parameters *) msg->buf);
				break;
			case NCM_MSG_STOP_CAP :
				server_stop_cap(s, (struct ncm_parameters *) msg->buf);
				break;
			case NCM_MSG_GET_STAT :
				server_get_stat(s, (struct ncm_stat_req *) msg->buf);
				break;
			case NCM_MSG_QUIT :
				server_stop = true;
			default:
				fprintf(stderr, "Unsupported message type %d\n", msg->type);
			}

			free(msg);
		}

		/* If we got disconnected in local mode, quit */
		if (s->local)
			break;

	}

	return 0;
}

static int server_get_cpu_params(struct ncm_server *s)
{
	cpu_set_t cpu_map;
	int ret = 0, i;

	CPU_ZERO(&cpu_map);

	/* Get intial parameters such as the number of CPUs */
	s->params.n_cpus = sysconf(_SC_NPROCESSORS_CONF);

	/* We're only concerned about the CPUs the server has access to, we
	 * won't be able to monitor the rest anyways
	 */
	ret = sched_getaffinity(0, sizeof(s->params.cpu_map), &cpu_map);
	if (ret) {
		fprintf(stderr, "Can't get CPU map\n");
		return ret;
	}

	for (i = 0; i < s->params.n_cpus; i++) {
		if (CPU_ISSET(i, &cpu_map))
			s->params.cpu_map |= (1 << i);
	}

	return ret;
}

void server_sigint_handler(int signal) {
	server_stop = 1;
}

int run_server(bool local, bool fork_to_background)
{
	/* Init stuff, such as forking in BG */
	struct ncm_server *s = NULL;
	struct ncm_connector *con = NULL;
	struct sigaction sa;
	int ret = 0;
	pid_t pid;

	s = malloc(sizeof(*s));
	if (!s)
		return 0;

	s->local = local;

	/* Create connector */
	if (local)
		con = connector_create(NCM_LOCAL_SERVER, NULL, 0);
	else
		con = connector_create(NCM_NETWORK_SERVER, "0.0.0.0",
				       NCM_DEFAULT_PORT);

	if (!con) {
		ret = -1;
		goto server_free;
	}

	s->con = con;

	if (fork_to_background) {
		pid = fork();
		if (pid < 0) {
			ret = -1;
			goto server_free;
		}

		if (pid > 0) {
			return 1;
		}

		/* Detach from controlling tty */
		setsid();
	}

	sa.sa_handler = &server_sigint_handler;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);

	if (sigaction(SIGINT, &sa, NULL))
		goto server_free;

	if (sigaction(SIGTERM, &sa, NULL))
		goto server_free;

	if (server_get_cpu_params(s))
		goto server_free;

	s->mon = monitor_create(s->params.n_cpus);
	if (!s->mon)
		goto server_free;

	ret = server_main_loop(s);

	connector_destroy(s->con);
	monitor_destroy(s->mon);

server_free:
	free(s);
	return ret;
}
