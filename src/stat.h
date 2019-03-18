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
	uint32_t drops;
	uint32_t total;
};

struct ncm_stat_pcpu_rxtx {
	int size;
	struct ncm_stats_pcpu_rxtx_entry pcpu_pkts[0];
};

/* Link request */
struct ncm_stat_link_req {
	char iface[16];
};

struct ncm_stat_link_info {
	uint32_t link_state;
	uint32_t link_speed; /* in mbps, if available */
	uint32_t n_rxqs;
	uint32_t n_txqs;
	uint32_t can_rss:1;
	uint32_t can_ntuple:1;
};

/* Per-link stats reply */
struct ncm_stat_link_global_stats {
	uint64_t pkts_tx;
	uint64_t pkts_rx;
	uint64_t bits_tx;
	uint64_t bits_rx;
	uint64_t drops;
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
