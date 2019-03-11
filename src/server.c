#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>

#include "server.h"
#include "monitor.h"
#include "connector.h"

struct ncm_server {
	struct ncm_connector *con;
};

static int server_main_loop(struct ncm_server *s)
{
	bool stop = false;
	struct ncm_message *msg = NULL;

	while (!stop) {

		if (connector_connect(s->con)) {
			fprintf(stderr, "Connection error\n");
			break;
		}

		while (connector_is_link_ok(s->con)) {
			msg = connector_receive(s->con);
			if (!msg)
				continue;

			/* Process message */

			free(msg);
		}

	}
}

int run_server(bool local, bool fork_to_background)
{
	/* Init stuff, such as forking in BG */
	struct ncm_server *s = NULL;
	struct ncm_connector *con = NULL;
	int ret = 0;
	pid_t pid;

	s = malloc(sizeof(*s));
	if (!s)
		return 0;

	/* Create connector */
	if (local)
		con = connector_create(NCM_LOCAL, NULL, 0);
	else
		con = connector_create(NCM_NETWORK_SERVER, "0.0.0.0",
				       NCM_DEFAULT_PORT);

	if (!con) {
		ret = -1;
		goto server_free;
	}

	if (fork_to_background) {
		pid = fork();
		if (pid < 0) {
			ret = -1;
			goto server_free;
		}

		if (pid > 0) {
			return 0;
		}

		/* Detach from controlling tty */
		setsid();
	}

	ret = server_main_loop(s);

server_free:
	free(s);
	return ret;
}
