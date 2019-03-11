#ifndef __NETCPU_MONITOR__
#define __NETCPU_MONITOR__

#include<stdint.h>

enum ncm_direction {
	NCM_DIR_IN,
	NCM_DIR_OUT,
	NCM_DIR_INOUT,
};

struct ncm_parameters {
	int n_cpus;
	uint64_t cpu_map;
	enum ncm_direction dir;
	char iface[16];
};

struct ncm_client;
struct ncm_ui;

#endif
