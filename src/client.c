#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<stdint.h>
#include<unistd.h>

#include "ui.h"
#include "client.h"
#include "connector.h"
#include "stat.h"

struct ncm_client *client_create(char *server_addr, uint64_t cpumap,
				 enum ncm_direction dir, char *interface)
{
	struct ncm_client *c = NULL;
	struct ncm_connector *con = NULL;

	c = malloc(sizeof(*c));
	if (!c)
		return NULL;

	memset(c, 0, sizeof(*c));

	/* Create connector */
	if (server_addr)
		con = connector_create(NCM_NETWORK_CLIENT, server_addr,
				       NCM_DEFAULT_PORT);
	else
		con = connector_create(NCM_LOCAL, NULL, 0);

	if (!con) {
		free(c);
		c = NULL;
	}

	c->con = con;

	/* We don't know yet how many CPUs there are on the server side */
	c->params.n_cpus = -1;
	c->params.cpu_map = cpumap;
	c->params.dir = dir;
	if (interface)
		strncpy(c->params.iface, interface, 15);

	return c;
}

int client_attach_ui(struct ncm_client *c, struct ncm_ui *ui)
{
	c->ui = ui;

	return ui_init(ui, c);
}

static int client_get_srv_parameters(struct ncm_client *c,
				      struct ncm_parameters *p)
{
	struct ncm_message msg, *response;
	int ret = 0;

	msg.type = NCM_MSG_GET_PARAMS;
	msg.len = 0;
	ret = connector_send(c->con, &msg);

	response = connector_receive(c->con);
	if (!response)
		return -1;

	if (response->type != NCM_MSG_RESP_GET_PARAMS) {
		fprintf(stdout, "Unexpected response when getting server parameters\n");
		free(response);
		return -2;
	}

	memcpy(p, response->buf, sizeof(*p));

	free(response);

	return 0;
}

int client_sync_params(struct ncm_client *c)
{
	int ret;
	struct ncm_parameters params;

	/* Recover the parameters, to get the real server config */
	ret = client_get_srv_parameters(c, &params);
	if (ret)
		return -1;

	c->params.n_cpus = params.n_cpus;
	c->params.cpu_map &= params.cpu_map;

	return ret;
}

int client_start_srv_cap(struct ncm_client *c)
{
	struct ncm_message *msg;
	int ret;

	msg = malloc(sizeof(*msg) + sizeof(c->params));
	msg->type = NCM_MSG_START_CAP;
	msg->len = sizeof(c->params);
	memcpy(msg->buf, &c->params, sizeof(c->params));

	ret = connector_send(c->con, msg);

	free(msg);

	return ret;
}

int client_stop_srv_cap(struct ncm_client *c)
{
	struct ncm_message *msg;
	int ret;

	msg = malloc(sizeof(*msg) + sizeof(c->params));
	msg->type = NCM_MSG_STOP_CAP;
	msg->len = sizeof(c->params);
	memcpy(msg->buf, &c->params, sizeof(c->params));

	ret = connector_send(c->con, msg);

	free(msg);

	return ret;
}

static struct ncm_stat *client_get_srv_stat(struct ncm_client *c,
					    enum stat_type type)
{
	struct ncm_message *msg, *response;
	struct ncm_stat_req *req;
	struct ncm_stat *stat = NULL;
	size_t stat_size;
	int ret = 0;

	msg = malloc(sizeof(*msg) + sizeof(*req));
	if (!msg) {
		fprintf(stderr, "Can't allocate msg\n");
		return NULL;
	}

	msg->type = NCM_MSG_GET_STAT;
	msg->len = sizeof(struct ncm_stat_req);
	req = (struct ncm_stat_req *) msg->buf;
	req->type = type;

	ret = connector_send(c->con, msg);
	if (ret) {
		fprintf(stderr, "Error sending stat request\n");
		goto err_req;
	}

	response = connector_receive(c->con);
	if (!response)
		goto err_req;

	if (response->type != NCM_MSG_RESP_GET_STATS) {
		fprintf(stdout, "Unexpected response when getting server stats\n");
		goto err;
	}

	/* Stats have a really variable layout, use the len param */
	stat = malloc(response->len);
	if (!stat)
		goto err;

	memcpy(stat, response->buf, response->len);

err:
	free(response);
err_req:
	free(msg);

	return stat;
}

struct ncm_stat_pcpu_rxtx *client_get_pcpu_stat(struct ncm_client *c, bool rx)
{
	struct ncm_stat_pcpu_rxtx *rxtx_stat = NULL;
	struct ncm_stat *stat = NULL;

	if (rx)
		stat = client_get_srv_stat(c, NCM_STAT_PCPU_RX);
	else
		stat = client_get_srv_stat(c, NCM_STAT_PCPU_TX);

	if (!stat)
		return NULL;

	rxtx_stat = malloc(sizeof(*rxtx_stat) + stat->size);
	if (!rxtx_stat)
		goto err;

	memcpy(rxtx_stat, stat->buf, sizeof(*rxtx_stat) + stat->size);

err:
	free(stat);
	return rxtx_stat;
}

int client_run(struct ncm_client *c)
{
	/* Main loop for client. Basically, wait refresh delay, get stats,
	 * update ui, rinse and repeat.
	 */
	bool stop = false;
	int ret;

	/* First, establish connection */
	ret = connector_connect(c->con);
	if (ret) {
		fprintf(stderr, "Unable to connect to server\n");
		return ret;
	}

	if (client_sync_params(c)) {
		fprintf(stderr, "Error synchronizing parameters with server\n");
		return -1;
	}

	/* Main loop is under UI's control. When this returns, we are in the
	 * exit path.*/
	ret = ui_run(c->ui);

	ui_destroy(c->ui);

	connector_destroy(c->con);

	free(c);

	return ret;
}

bool client_is_connected(struct ncm_client *c)
{
	return connector_is_link_ok(c->con);
}
