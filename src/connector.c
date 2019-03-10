#include<stdlib.h>

#include "connector.h"

struct ncm_connector *connector_create(enum ncm_connector_type type, char *addr,
				       int port, void *priv)
{
	return NULL;
}

int connector_connect(struct ncm_connector *con)
{
	return 0;
}

int connector_send(struct ncm_connector *con, struct ncm_message *msg)
{
	return 0;
}


