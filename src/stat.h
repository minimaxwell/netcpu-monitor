#include<stdint.h>
#include "netcpu-monitor.h"

#ifndef __STAT__
#define __STAT__

enum stat_type {
	NCM_STAT_PCPU_RX,
	NCM_STAT_PCPU_TX,

	/* Last stat */
	NCM_N_STATS,
};

struct ncm_stat_pcpu_rxtx {
	int size;
	uint32_t pcpu_pkts[0];
};

struct ncm_stat {
	enum stat_type type;
	int size;
	uint8_t buf[0];
};

struct ncm_stat_req {
	enum stat_type type;
};

#endif
