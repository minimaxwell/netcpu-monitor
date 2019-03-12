#ifndef __MONITOR__
#define __MONITOR__

#include<stdbool.h>

#include "netcpu-monitor.h"
#include "monitor.h"
#include "stat.h"

struct ncm_monitor;

struct ncm_monitor *monitor_create(int n_cpus);
void monitor_destroy(struct ncm_monitor *mon);
int ncm_monitor_start_cap(struct ncm_monitor *mon, struct ncm_parameters *params);
struct ncm_stat_pcpu_rxtx *ncm_monitor_snapshot(struct ncm_monitor *mon);
int ncm_monitor_stop_cap(struct ncm_monitor *mon);

#endif
