/* License license blabla */

#include<stdio.h>
#include<unistd.h>
#include<stdint.h>
#include<stdlib.h>
#include<stdbool.h>
#include<errno.h>
#include<getopt.h>
#include<string.h>

#include "netcpu-monitor.h"
#include "client.h"
#include "server.h"
#include "ui.h"

#define NCM_SHORTOPTS "Dc:C:d:i:ns"
static const struct option long_options[] = {
	{"background",	no_argument,		NULL,	'D'},
	{"client",	required_argument,	NULL,	'c'},
	{"cpu-map",	required_argument,	NULL,	'C'},
	{"direction",	required_argument,	NULL,	'd'},
	{"interface",	required_argument,	NULL,	'i'},
	{"ncurses",	no_argument,		NULL,	'n'},
	{"server",	no_argument,		NULL,	's'},
};

void print_help(void)
{
	fprintf(stdout, "netcpu-monitor : Monitor CPU activity due to networking\n");
	fprintf(stdout, "\tUsage : netcpu [options] \n");
	fprintf(stdout, "\t\t -D : Daemon mode (only available in server mode)\n");
	fprintf(stdout, "\t\t -c <server address> : Run in client mode, connect to server at the given address\n");
	fprintf(stdout, "\t\t -C <cpu_map> : Specify the cpu map to listen to (one bit per cpu)\n");
	fprintf(stdout, "\t\t -d <direction> : Specify the direction to listen : 'in', 'out' or 'inout'\n");
	fprintf(stdout, "\t\t -n : Use ncurses interface (not available in server mode)");
	fprintf(stdout, "\t\t -s : Run in server mode, listens on port 1991\n");
}

int main(int argc, char **argv)
{
	bool opt_background = false;
	char *opt_client_serveraddr = NULL;
	uint64_t opt_cpumap = 0xffffffff;
	enum ncm_direction opt_dir = NCM_DIR_INOUT;
	char *opt_interface = NULL;
	enum ui_type opt_ui_type = NCM_UI_CLI;
	bool opt_server = false;
	struct ncm_client *client = NULL;
	struct ncm_ui *ui = NULL;
	bool local = false;

	int opt = 0;
	int longopt_index = 0;

	while(1) {
		opt = getopt_long(argc, argv, NCM_SHORTOPTS, long_options,
				  &longopt_index);

		if (opt == -1)
			break;

		switch (opt) {
		case 'D' :
			opt_background = true;
			break;
		case 'c' :
			opt_client_serveraddr = optarg;
			break;
		case 'C' :
			errno = 0;
			opt_cpumap = (uint32_t) strtoul(optarg, NULL, 0);

			if (errno) {
				fprintf(stderr, "Invalid cpu-map\n");
				goto print_help_and_quit;
			}

			break;
		case 'd' :
			if (!strcmp(optarg, "in")) {
				opt_dir = NCM_DIR_IN;
			} else if (!strcmp(optarg, "out")) {
				opt_dir = NCM_DIR_OUT;
			} else if (!strcmp(optarg, "inout")) {
				opt_dir = NCM_DIR_INOUT;
			} else {
				fprintf(stderr, "Invalid direction '%s'\n",
					optarg);
				goto print_help_and_quit;
			}

			break;
		case 'i' :
			opt_interface = optarg;
			break;
		case 'n' :
			opt_ui_type = NCM_UI_NCURSES;
			break;
		case 's' :
			opt_server = true;
			break;
		default :
			print_help();
			return -1;
		}
	}

	/* Validate options */
	if (opt_server && opt_client_serveraddr)
		goto print_help_and_quit;

	/* Todo : local mode where client and server run on the same machine */
	if (!opt_server && !opt_client_serveraddr)
		local = true;

	if (opt_server || local)
		return run_server(local, opt_background);

	client = client_create(opt_client_serveraddr, opt_cpumap, opt_dir,
			       opt_interface);
	if (!client)
		return 1;

	ui = ui_create(opt_ui_type);
	if (!ui)
		return 1;

	if (client_attach_ui(client, ui))
		return 1;

	return client_run(client);

print_help_and_quit:

	print_help();
	return 1;
}
