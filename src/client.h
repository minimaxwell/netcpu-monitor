#include<stdint.h>

#include "netcpu-monitor.h"
#include "connector.h"

#ifndef __CLIENT__
#define __CLIENT__

struct ncm_client {
	char *server_addr;
	uint32_t cpumap;
	enum ncm_direction dir;
	struct ncm_ui *ui;
	struct ncm_connector *con;
};

struct ncm_client *client_create(char *server_addr, uint32_t cpumap,
				 enum ncm_direction dir, char *interface);

int client_attach_ui(struct ncm_client *c, struct ncm_ui *ui);

int client_run(struct ncm_client *c);

#endif
