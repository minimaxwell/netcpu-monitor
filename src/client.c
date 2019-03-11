#include<stdlib.h>
#include<string.h>

#include "ui.h"
#include "client.h"
#include "connector.h"
#include "stat.h"

struct ncm_client *client_create(char *server_addr, uint32_t cpumap,
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
	c->params.iface = interface;

	return c;
}

int client_attach_ui(struct ncm_client *c, struct ncm_ui *ui)
{
	c->ui = ui;

	return ui_init(ui, c);
}

int client_run(struct ncm_client *c)
{
	/* Main loop for client. Basically, wait refresh delay, get stats,
	 * update ui, rinse and repeat.
	 */

	return 0;
}
