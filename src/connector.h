#define NCM_DEFAULT_PORT 1991

#include<stdbool.h>
#include<stdint.h>

#ifndef __CONNECTOR__
#define __CONNECTOR__

enum ncm_connector_type {
	NCM_LOCAL,
	NCM_NETWORK_SERVER,
	NCM_NETWORK_CLIENT,
};

enum ncm_message_type {
	NCM_MSG_SET_PARAMS,
	NCM_MSG_GET_PARAMS,
	NCM_MSG_RESP_GET_PARAMS,
	NCM_MSG_START_CAP,
	NCM_MSG_STOP_CAP,
	NCM_MSG_GET_STAT,
	NCM_MSG_RESP_GET_STATS,
	NCM_MSG_QUIT,

	/* Last msg */
	NCM_N_MSG,
};

struct ncm_message {
	enum ncm_message_type type;
	int len;
	uint8_t buf[0];
};

struct ncm_connector {
	enum ncm_connector_type type;
	int sockfd;
	int confd;
	char *addr;
	int port;
	bool status;
};

struct ncm_connector *connector_create(enum ncm_connector_type type, char *addr,
				       int port);

void connector_destroy(struct ncm_connector *con);

bool connector_is_link_ok(struct ncm_connector *con);

int connector_connect(struct ncm_connector *con);

int connector_send(struct ncm_connector *con, struct ncm_message *msg);

struct ncm_message *connector_receive(struct ncm_connector *con);

#endif
