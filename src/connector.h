#define NCM_DEFAULT_PORT 1991

#ifndef __CONNECTOR__
#define __CONNECTOR__

enum ncm_connector_type {
	NCM_LOCAL,
	NCM_NETWORK,
};

struct ncm_message {
	int type;
	void *buf;
};

struct ncm_connector {
	enum ncm_connector_type type;
	int fd;
	char *addr;
	int port;
	void *user_priv;
	void (* on_message)(void *priv, struct ncm_message *m);
	void (* on_connect)(void *priv);
	void (* on_disconnect)(void *priv);
};


struct ncm_connector *connector_create(enum ncm_connector_type type, char *addr,
				       int port, void *priv);

int connector_connect(struct ncm_connector *con);

int connector_send(struct ncm_connector *con, struct ncm_message *msg);

#endif
