#include<stdbool.h>
#include<unistd.h>
#include<sys/types.h>

#include "netcpu-monitor.h"

#ifndef __SERVER__
#define __SERVER__

int run_server(bool local, bool fork_to_background);

#endif
