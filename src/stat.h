#ifndef __STAT__
#define __STAT__

#include<stdint.h>
#include<time.h>
#include "netcpu-monitor.h"

enum stat_type {
	NCM_STAT_NONE = 0,
	NCM_STAT_PCPU_RXTX,

	/* Last stat */
	NCM_N_STATS,
};

struct ncm_stats_pcpu_rxtx_entry {
	uint32_t rx;
	uint32_t tx;
};

struct ncm_stat_pcpu_rxtx {
	int size;
	struct ncm_stats_pcpu_rxtx_entry pcpu_pkts[0];
};

struct ncm_stat {
	enum stat_type type;
	struct timespec ts;
	int size;
	uint8_t buf[0];
};

struct ncm_stat_req {
	enum stat_type type;
};

#endif
