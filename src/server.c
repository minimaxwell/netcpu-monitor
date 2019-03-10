#include "server.h"
#include "monitor.h"
#include "connector.h"

struct ncm_server {
	struct ncm_connector *con;
};

int run_server(bool local, bool fork_to_background)
{
	/* Init stuff, such as forking in BG */

	/* Create connector */

	/* Create monitors */

	/* Wait for clients */
	return 0;
}
