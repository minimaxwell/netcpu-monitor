#include<stdint.h>

#include "netcpu-monitor.h"
#include "connector.h"
#include "stat.h"

#ifndef __CLIENT__
#define __CLIENT__

struct ncm_client {
	char *server_addr;
	struct ncm_ui *ui;
	struct ncm_connector *con;
	struct ncm_parameters params;
};

struct ncm_client *client_create(char *server_addr, uint64_t cpumap,
				 enum ncm_direction dir, char *interface);

int client_attach_ui(struct ncm_client *c, struct ncm_ui *ui);

int client_run(struct ncm_client *c);

int client_sync_params(struct ncm_client *c);
int client_start_srv_cap(struct ncm_client *c);
int client_stop_srv_cap(struct ncm_client *c);
bool client_is_connected(struct ncm_client *c);

/* Stats accessors */
struct ncm_stat_pcpu_rxtx *client_get_pcpu_stat(struct ncm_client *c, bool rx);

#endif
