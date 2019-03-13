#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/un.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<string.h>

#include "connector.h"

#define LOCAL_SOCK_PATH "/tmp/netcpu.default"

static int connector_create_network_server(struct ncm_connector *con)
{
	struct sockaddr_in serv_addr;
	int sockfd = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Can't open socket\n");
		return sockfd;
	}

	serv_addr.sin_family = AF_INET;

	/* We might want to change that. */
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(con->port);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		return -1;
	}

	/* Backlog allows only one connection at a time. */
	listen(sockfd, 1);

	return sockfd;
}

static int connector_create_network_client(struct ncm_connector *con)
{
	int sockfd = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		fprintf(stderr, "Can't open socket\n");

	return sockfd;
}

static int connector_create_local_server(struct ncm_connector *con)
{
	struct sockaddr_un serv_addr;
	int sockfd = 0;

	memset(&serv_addr, 0, sizeof(serv_addr));

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "Can't open socket\n");
		return sockfd;
	}

	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, LOCAL_SOCK_PATH,
		sizeof(serv_addr.sun_path) - 1);

	/* Just in case */
	unlink(LOCAL_SOCK_PATH);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		close(sockfd);
		return -1;
	}

	listen(sockfd, 1);

	return sockfd;
}

static int connector_create_local_client(struct ncm_connector *con)
{
	int sockfd = 0;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		fprintf(stderr, "Can't open socket\n");

	return sockfd;
}

struct ncm_connector *connector_create(enum ncm_connector_type type, char *addr,
				       int port)
{
	struct ncm_connector *con = NULL;

	con = malloc(sizeof(*con));
	if (!con)
		return NULL;

	con->type = type;
	con->addr = addr;
	con->port = port;
	con->confd = -1;
	con->status = false;

	switch (type) {
	case NCM_LOCAL_SERVER:
		con->sockfd = connector_create_local_server(con);
		break;
	case NCM_LOCAL_CLIENT:
		con->sockfd = connector_create_local_client(con);
		break;
	case NCM_NETWORK_SERVER:
		con->sockfd = connector_create_network_server(con);
		break;
	case NCM_NETWORK_CLIENT:
		con->sockfd = connector_create_network_client(con);
		break;
	}

	if (con->sockfd < 0) {
		fprintf(stderr, "Error creating connector\n");
		goto con_err;
	}

	return con;

con_err:
	free(con);
	return NULL;
}

void connector_destroy(struct ncm_connector *con)
{
	if (con->type == NCM_NETWORK_SERVER)
		unlink(LOCAL_SOCK_PATH);

	if (con->confd >= 0)
		close(con->confd);

	if (con->type == NCM_NETWORK_SERVER && con->sockfd >= 0)
		close(con->sockfd);

	free(con);
}

/* Blocking until connection is established */
int connector_connect(struct ncm_connector *con)
{
	int ret;

	if (con->type == NCM_NETWORK_SERVER || con->type == NCM_LOCAL_SERVER) {

		con->confd = accept(con->sockfd, NULL, NULL);

	} else if (con->type == NCM_NETWORK_CLIENT) {
		struct sockaddr_in serv_addr;

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(con->addr);
		serv_addr.sin_port = htons(con->port);

		ret = connect(con->sockfd, (struct sockaddr *)&serv_addr,
				     sizeof(serv_addr));
		if (ret)
			con->confd = ret;
		else
			con->confd = con->sockfd;
	} else if (con->type == NCM_LOCAL_CLIENT) {
		struct sockaddr_un serv_addr;

		memset(&serv_addr, 0, sizeof(serv_addr));

		serv_addr.sun_family = AF_UNIX;
		strncpy(serv_addr.sun_path, LOCAL_SOCK_PATH,
			sizeof(serv_addr.sun_path) - 1);

		ret = connect(con->sockfd, (struct sockaddr *)&serv_addr,
				     sizeof(serv_addr));
		if (ret)
			con->confd = ret;
		else
			con->confd = con->sockfd;
	} else {
		fprintf(stderr, "Not implemented\n");
		/* not imp */
	}

	if (con->confd < 0)
		return con->confd;

	con->status = true;

	return 0;
}

bool connector_is_link_ok(struct ncm_connector *con)
{
	return con->status;
}

int connector_send(struct ncm_connector *con, struct ncm_message *msg)
{
	int n;

	if (!con->status)
		return -1;

	if (con->confd < 0)
		return -1;

	n = send(con->confd, msg, sizeof(*msg) + msg->len, 0);
	if (n < 0) {
		fprintf(stderr, "Error sending message : %s\n", strerror(errno));
		con->status = false;
		return -1;
	} else if (n < sizeof(*msg) + msg->len) {
		fprintf(stderr, "Packet not fully written, we have a problem\n");
		return -1;
	}

	return 0;
}

/* Blocking until a message is received */
struct ncm_message *connector_receive(struct ncm_connector *con)
{
	struct ncm_message *msg_head = NULL, *msg = NULL;
	int n;

	if (!con->status)
		return NULL;

	/* Allocate only the header */
	msg_head = malloc(sizeof(*msg_head));
	if (!msg_head)
		return NULL;

	n = recv(con->confd, msg_head, sizeof(*msg_head), 0);
	if (n == 0)
		goto disconnect;

	if (n != sizeof(*msg_head))
		goto err;

	/* If there's no message body, we're done */
	if (msg_head->len == 0)
		return msg_head;

	/* Probably has security issues */
	msg = realloc(msg_head, sizeof(*msg_head) + msg_head->len);

	/* If realloc fails, the original block is left untouched and should be
	 * freed*/
	if (!msg)
		goto err;

	n = recv(con->confd, msg->buf, msg->len, 0);
	if (n == 0)
		goto disconnect;

	if (n != msg->len)
		goto err;

	return msg;

disconnect:

	con->status = false;

	if (con->type == NCM_NETWORK_SERVER || con->type == NCM_LOCAL_SERVER)
		close(con->confd);

err:
	free(msg_head);
	return NULL;
}
