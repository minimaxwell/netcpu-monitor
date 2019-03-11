#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "connector.h"

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
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(con->port);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		return -1;
	}

	/* Backlog allows only one connection at a time. */
	listen(sockfd, 1);

	return sockfd;
}

static int connector_create_network_client(struct ncm_connector *con)
{
	struct sockaddr_in serv_addr;
	int sockfd = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		fprintf(stderr, "Can't open socket\n");

	return sockfd;
}

static int connector_create_local(struct ncm_connector *con)
{
	return -1;
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

	if (type == NCM_LOCAL) {
		con->sockfd = connector_create_local(con);
	} else if (type == NCM_NETWORK_SERVER) {
		con->sockfd = connector_create_network_server(con);
	} else {
		con->sockfd = connector_create_network_client(con);
	}

	if (con->sockfd < 0) {
		fprintf(stderr, "Not supported\n");
		goto con_err;
	}

	return con;

con_err:
	free(con);
	return NULL;
}

/* Blocking until connection is established */
int connector_connect(struct ncm_connector *con)
{
	if (con->type == NCM_NETWORK_SERVER) {
		struct sockaddr_in cli_addr;
		socklen_t cli_len = sizeof(cli_addr);

		con->confd = accept(con->sockfd, (struct sockaddr *)&cli_addr,
				    &cli_len);

	} else if (con->type == NCM_NETWORK_CLIENT) {
		struct sockaddr_in serv_addr;

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(con->addr);
		serv_addr.sin_port = htons(con->port);

		con->confd = connect(con->sockfd, (struct sockaddr *)&serv_addr,
				     sizeof(serv_addr));
	} else {
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

	if (con->confd < 0)
		return -1;

	n = write(con->confd, msg, sizeof(*msg) + msg->len);
	if (n < sizeof(*msg) + msg->len) {
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

	/* Allocate only the header */
	msg_head = malloc(sizeof(*msg_head));
	if (!msg)
		return NULL;

	n = read(con->confd, msg, sizeof(*msg_head));
	if (n != sizeof(*msg_head))
		goto err;

	/* Probably has security issues */
	msg = realloc(msg_head, sizeof(*msg_head) + msg_head->len);

	/* If realloc fails, the original block is left untouched and should be
	 * freed*/
	if (!msg)
		goto err;

	n = read(con->confd, msg->buf, msg->len);
	if (n != msg->len) {
		fprintf(stderr, "Message body of unexpected size (expect %d, got %d)\n",
			msg->len, n);
		goto err;
	}

	return msg;

err:
	free(msg_head);
	return NULL;
}
