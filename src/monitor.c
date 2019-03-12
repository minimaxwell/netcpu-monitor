#define _GNU_SOURCE

#include<net/if.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<semaphore.h>
#include<linux/if_packet.h>
#include<linux/if.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<net/ethernet.h>
#include<arpa/inet.h>
#include<string.h>
#include<time.h>

#include "monitor.h"
#include "stat.h"

struct ncm_monitor_worker {
	int fd;
	int cpu;
	pthread_t thread_id;

	/* Protects the counter and status */
	pthread_mutex_t thread_mutex;

	/* Start and stops acquisition */
	sem_t thread_sem;

	uint32_t rx_counter;
	uint32_t tx_counter;
	bool alive;
	bool run;
};

struct ncm_monitor {
	char iface[16];
	int promisc_fd;
	int fanout_group;
	struct timespec ts_last_acq;
	int n_workers;
	struct ncm_monitor_worker *workers;
	bool capturing;
};

void *worker_main_loop(void *priv)
{
	unsigned char packet_buffer[65535];
	struct ncm_monitor_worker *w = priv;
	struct sockaddr_ll sll;
	unsigned int sll_length = sizeof(sll);
	int len;

	while (w->alive) {
		/* Wait to be activated. We must only be activated when we have
		 * a valid fd from which we can listen to.
		 */
		sem_wait(&w->thread_sem);
		if (!w->alive)
			goto done;

		if (!w->run)
			continue;


		len = recvfrom(w->fd, packet_buffer, sizeof(packet_buffer), 0,
			       (struct sockaddr *)&sll, &sll_length);
		if (len < 0)
			goto done;

		pthread_mutex_lock(&w->thread_mutex);

		if (sll.sll_pkttype == PACKET_OUTGOING)
			w->tx_counter++;
		else
			w->rx_counter++;

		pthread_mutex_unlock(&w->thread_mutex);

done:
		/* Do the thing */
		sem_post(&w->thread_sem);
	}

	return w;
}

static int worker_init(struct ncm_monitor_worker *w, int cpu)
{
	int ret;
	pthread_mutexattr_t mutex_attr;
	pthread_attr_t thread_attr;
	cpu_set_t cpu_set;

	memset(w, 0, sizeof(*w));

	w->cpu = cpu;
	w->alive = true;
	w->run = false;

	/* Initialize the resource mutex */
	pthread_mutexattr_init(&mutex_attr);
	pthread_mutex_init(&w->thread_mutex, &mutex_attr);

	ret = sem_init(&w->thread_sem, 0, 0);
	if (ret)
		return ret;

	CPU_ZERO(&cpu_set);
	CPU_SET(cpu, &cpu_set);

	/* Attach the thread to the given core */
	pthread_attr_init(&thread_attr);
	ret = pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set), &cpu_set);
	if (ret)
		return ret;

	ret = pthread_create(&w->thread_id, &thread_attr, worker_main_loop, w);

	pthread_attr_destroy(&thread_attr);

	return ret;
}

/* most of the magic borrowed from rxtxcpu */
static int worker_setup(struct ncm_monitor_worker *w, unsigned int if_id,
			int fanout_group)
{
	struct tpacket_req req;
	struct timeval receive_timeout;
	struct sockaddr_ll sll;
	int ret, fanout;

	/* We want all L2 packets with header included */
	w->fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (w->fd < 0)
		return w->fd;

	memset(&req, 0, sizeof(req));
	setsockopt(w->fd, SOL_PACKET, PACKET_RX_RING, (void *)&req, sizeof(req));
	setsockopt(w->fd, SOL_PACKET, PACKET_TX_RING, (void *)&req, sizeof(req));

	/* Setup a timer for the worker's main loop */
	receive_timeout.tv_sec = 0;
	receive_timeout.tv_usec = 1000;
	setsockopt(w->fd, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout,
		   sizeof(receive_timeout));

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_ALL);
	sll.sll_ifindex = if_id;

	ret = bind(w->fd, (struct sockaddr *)&sll, sizeof(sll));
	if (ret < 0)
		goto err_fd;

	fanout = fanout_group | (PACKET_FANOUT_CPU << 16);
	ret = setsockopt(w->fd, SOL_PACKET, PACKET_FANOUT, &fanout, sizeof(fanout));

	return 0;

err_fd:
	close(w->fd);

	return ret;
}

static void worker_cleanup(struct ncm_monitor_worker *w)
{
	w->alive = false;
	sem_post(&w->thread_sem);

	pthread_join(w->thread_id, NULL);

	close(w->fd);

	sem_destroy(&w->thread_sem);
	pthread_mutex_destroy(&w->thread_mutex);
}

static int worker_start(struct ncm_monitor_worker *w)
{
	w->rx_counter = 0;
	w->tx_counter = 0;
	w->run = true;
	return sem_post(&w->thread_sem);
}

static int worker_stop(struct ncm_monitor_worker *w)
{
	w->run = false;
	return 0;
}

static void worker_snap_and_reset(struct ncm_monitor_worker *w,
				  struct ncm_stats_pcpu_rxtx_entry *entry)
{
	pthread_mutex_lock(&w->thread_mutex);
	entry->rx = w->rx_counter;
	entry->tx = w->tx_counter;

	w->rx_counter = 0;
	w->tx_counter = 0;
	pthread_mutex_unlock(&w->thread_mutex);
}

struct ncm_monitor *monitor_create(int n_cpus)
{
	struct ncm_monitor *mon;
	int i;

	mon = malloc(sizeof(*mon));
	if (!mon)
		return NULL;

	mon->capturing = false;
	mon->n_workers = n_cpus;
	mon->workers = malloc(n_cpus * sizeof(struct ncm_monitor_worker));
	if (!mon->workers)
		goto free_mon;

	for (i = 0; i < mon->n_workers; i++) {
		if (worker_init(&mon->workers[i], i)) {
			fprintf(stderr, "Error creating worker for CPU %d\n", i);
			goto free_mon;
		}
	}

	return mon;

free_mon:
	free(mon);

	return NULL;
}

void monitor_destroy(struct ncm_monitor *mon)
{
	int i;

	for (i = 0; i < mon->n_workers; i++)
		worker_cleanup(&mon->workers[i]);

	free(mon->workers);
	free(mon);
}

/* From rxtxcpu */
int ncm_monitor_set_promisc(struct ncm_monitor *mon, unsigned int if_id)
{
	struct packet_mreq mr;

	memset(&mr, 0, sizeof(mr));
	mr.mr_type = PACKET_MR_PROMISC;
	mr.mr_ifindex = if_id;

	/* Keep this socket open while we capture to stay in promiscuous mode */
	mon->promisc_fd = socket(AF_PACKET, SOCK_DGRAM, 0);
	if (mon->promisc_fd < 0)
		return mon->promisc_fd;

	return setsockopt(mon->promisc_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			 (void *)&mr, sizeof(mr));
}

int ncm_monitor_start_cap(struct ncm_monitor *mon, struct ncm_parameters *params)
{
	unsigned int if_id;
	int i, ret;

	if (mon->capturing)
		return 0;

	strncpy(mon->iface, params->iface, 16);

	if_id = if_nametoindex(params->iface);
	if (!if_id)
		return -1;

	ret = ncm_monitor_set_promisc(mon, if_id);
	if (ret)
		return ret;

	/* from rxtxcpu */
	mon->fanout_group = getpid() & 0xffff;

	for (i = 0; i < mon->n_workers; i++) {
		/* Take requested cpu_map into account */
		if (!(params->cpu_map & (1 << i)))
			continue;

		ret = worker_setup(&mon->workers[i], if_id, mon->fanout_group);
		if (ret) {
			fprintf(stderr, "Error setting up worker %d\n", i);
			return ret;
		}

		ret = worker_start(&mon->workers[i]);
		if (ret) {
			fprintf(stderr, "Error starting worker %d\n", i);
		}
	}

	mon->capturing = true;

	return clock_gettime(CLOCK_MONOTONIC_RAW, &mon->ts_last_acq);
}

struct ncm_stat_pcpu_rxtx *ncm_monitor_snapshot(struct ncm_monitor *mon,
						struct timespec *ts)
{
	struct ncm_stat_pcpu_rxtx *rxtx = NULL;
	struct timespec ts_now;
	int i;

	rxtx = malloc(sizeof(*rxtx) + mon->n_workers *
		      sizeof(struct ncm_stats_pcpu_rxtx_entry));
	if (!rxtx)
		return NULL;

	rxtx->size = mon->n_workers;

	for (i = 0; i < mon->n_workers; i++) {
		worker_snap_and_reset(&mon->workers[i], &rxtx->pcpu_pkts[i]);
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now);
	ts->tv_sec = ts_now.tv_sec - mon->ts_last_acq.tv_sec;
	ts->tv_nsec = ts_now.tv_nsec - mon->ts_last_acq.tv_nsec;

	mon->ts_last_acq = ts_now;

	return rxtx;
}

int ncm_monitor_stop_cap(struct ncm_monitor *mon)
{
	struct ncm_monitor_worker *w = NULL;
	int i;

	if(!mon->capturing)
		return 0;

	for (i = 0; i < mon->n_workers; i++) {
		w = &mon->workers[i];
		if (!w->run)
			continue;

		if (worker_stop(w))
			fprintf(stderr, "Error stopping worker %d\n", i);
		else
			close(w->fd);
	}

	close(mon->promisc_fd);

	mon->capturing = false;

	return 0;
}
