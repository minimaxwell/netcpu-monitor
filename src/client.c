#include<stdlib.h>
#include<string.h>

#include "ui.h"
#include "client.h"

struct ncm_client *client_create(char *server_addr, uint32_t cpumap,
				 enum ncm_direction dir, char *interface)
{
	struct ncm_client *c = NULL;

	c = malloc(sizeof(*c));
	if (!c)
		return NULL;

	memset(c, 0, sizeof(*c));

	/* Create connector */

	return c;
}

int client_attach_ui(struct ncm_client *c, struct ncm_ui *ui)
{
	c->ui = ui;

	return ui_init(ui, c);
}

int client_run(struct ncm_client *c)
{
	/* Main loop for client. blocking poll/select on connector. Signal
	 * handlers must be registered by ui. */

	return 0;
}
