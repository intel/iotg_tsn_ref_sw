/******************************************************************************
 *
 * Copyright (c) 2020, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <time.h>
#include <poll.h>
#include <signal.h>

/* Ethernet header */
#include <net/ethernet.h>

/* if_nametoindex() */
#include <net/if.h>

/* getpagesize() */
#include <unistd.h>

/* sched_yield() */
#include <sched.h>

/* Hwtstamp_config */
#include <linux/net_tstamp.h>

/* RLIMIT */
#include <sys/time.h>
#include <sys/resource.h>

/* XSK */
#include <linux/if_link.h>
#include "linux/if_xdp.h"
#include <bpf/libbpf.h>
#include <bpf/xsk.h>
#include <bpf/bpf.h>

#include "txrx-afxdp.h"

extern uint32_t glob_xdp_flags;
extern int glob_ifindex;
extern int verbose;
extern uint32_t glob_rx_seq;

/* User Defines */
#define DEFAULT_NUM_FLUSH_PACKETS 10 //for socket flushing

/* Signal handler to gracefully shutdown */
void afxdp_sigint_handler(int signum)
{
	fprintf(stderr, "Info: SIGINT triggered.\n"); //TODO SIGNUM?
	halt_tx_sig = signum;
}

void xdpsock_cleanup(void)
{
	struct xsk_umem *umem = glob_xskinfo_ptr->pktbuff->umem;

	xsk_socket__delete(glob_xskinfo_ptr->xskfd);
	(void)xsk_umem__delete(umem);

	exit(EXIT_SUCCESS);
}

void __afxdp_exit_with_error(int error, const char *file, const char *func, int line)
{
	fprintf(stderr, "%s:%s:%i: errno: %d/\"%s\"\n", file, func,
		line, error, strerror(error));
	xdpsock_cleanup();
	exit(EXIT_FAILURE);
}

/* Create a umem using the buffer provided? TODO what is ubuf for*/
static struct pkt_buffer *create_umem(void *ubuf, struct xsk_opt *x_opt)
{
	uint64_t single_umem_ring_size;
	struct pkt_buffer *temp_buff;
	uint32_t idx = 0;
	int ret;
	__u64 i;

	struct xsk_umem_config uconfig = {
		.fill_size = x_opt->frames_per_ring,
		.comp_size = x_opt->frames_per_ring,
		.frame_size = x_opt->frame_size,
		.frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM,
	};

	single_umem_ring_size = (uint64_t) (x_opt->frames_per_ring * x_opt->frame_size);

	ret = posix_memalign(&ubuf, getpagesize(), /* PAGE_SIZE aligned */
			     single_umem_ring_size);
	if (ret)
		exit(EXIT_FAILURE);

	temp_buff = calloc(1, sizeof(*temp_buff));
	if (!temp_buff)
		afxdp_exit_with_error(errno);

	ret = xsk_umem__create(&temp_buff->umem, ubuf, single_umem_ring_size,
				&temp_buff->rx_fill_ring, &temp_buff->tx_comp_ring,
				&uconfig);
	if (ret)
		afxdp_exit_with_error(-ret);

	temp_buff->buffer = ubuf;

	/* Populate rx fill ring with addresses */
	ret = xsk_ring_prod__reserve(&temp_buff->rx_fill_ring,
				     x_opt->frames_per_ring,
				     &idx);
	if (ret != x_opt->frames_per_ring)
		return (struct pkt_buffer *) - 1;

	for (i = 0; i < x_opt->frames_per_ring; i++)
		*xsk_ring_prod__fill_addr(&temp_buff->rx_fill_ring, idx++) =
							i * x_opt->frame_size;

	xsk_ring_prod__submit(&temp_buff->rx_fill_ring,
			      x_opt->frames_per_ring);

	return temp_buff;
}

static struct xsk_info *create_xsk_info(struct user_opt *opt, struct pkt_buffer *pktbuff)
{
	struct xsk_socket_config cfg;
	struct xsk_info *temp_xsk;
	int ret;

	temp_xsk = calloc(1, sizeof(*temp_xsk));
	if (!temp_xsk)
		afxdp_exit_with_error(errno);

	temp_xsk->pktbuff = pktbuff;

	cfg.rx_size = opt->x_opt.frames_per_ring;	//Use same size for TX RX
	cfg.tx_size = opt->x_opt.frames_per_ring;

	cfg.libbpf_flags = 0;
	cfg.xdp_flags = opt->x_opt.xdp_flags;
	cfg.bind_flags = opt->x_opt.xdp_bind_flags;

	ret = xsk_socket__create(&temp_xsk->xskfd, opt->ifname,
				 opt->x_opt.queue, temp_xsk->pktbuff->umem,
				 &temp_xsk->rx_ring, &temp_xsk->tx_ring, &cfg);
	if (ret)
		afxdp_exit_with_error(-ret);

	ret = bpf_xdp_query_id(opt->ifindex, opt->x_opt.xdp_flags, &temp_xsk->prog_id);
	if (ret)
		afxdp_exit_with_error(-ret);

	return temp_xsk;
}

static void prefill_tx_umem_rings(void *buff_addr, tsn_packet *example_pkt,
				  int count, int frame_size)
{
	__u64 i;

	for (i = 0; i < count; i++) {
		memcpy(xsk_umem__get_data(buff_addr, i * frame_size),
			example_pkt, sizeof(*example_pkt) - 1);
		// Note: frame size != packet size
	}
}

void init_xdp_socket(struct user_opt *opt)
{
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	void *ubuf = NULL;

	opt->x_opt.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
	opt->x_opt.xdp_bind_flags = 0;

	switch(opt->xdp_mode) {
	case XDP_MODE_SKB_COPY:
		opt->x_opt.xdp_flags |= XDP_FLAGS_SKB_MODE;
		opt->x_opt.xdp_bind_flags |= XDP_COPY;
		break;
	case XDP_MODE_NATIVE_COPY:
		opt->x_opt.xdp_flags |= XDP_FLAGS_DRV_MODE;
		opt->x_opt.xdp_bind_flags |= XDP_COPY;
		break;
	case XDP_MODE_ZERO_COPY:
		// opt->x_opt.xdp_flags |= XDP_FLAGS_DRV_MODE;
		opt->x_opt.xdp_bind_flags |= XDP_ZEROCOPY;
		break;
	default:
		exit_with_error("ERROR: XDP Mode s,c or z must be specified\n");
		break;
	}

	if (opt->need_wakeup)
		opt->x_opt.xdp_bind_flags |= XDP_USE_NEED_WAKEUP;

	glob_xdp_flags = opt->x_opt.xdp_flags;
	glob_ifindex = opt->ifindex;

	/* Let this app have all resource. Need root */
	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Create the umem and store the pointers */
	struct pkt_buffer *pktbuffer;
	pktbuffer = create_umem(ubuf, &opt->x_opt);

	/* Assign the umem to a socket */
	opt->xsk = create_xsk_info(opt, pktbuffer);
	glob_xskinfo_ptr = opt->xsk;

}

static void update_txstats(struct xsk_info *xsk)
{
	uint32_t rcvd;
	uint32_t idx;

	if (!xsk->outstanding_tx)
		return;

	//TODO: keep track of stats
	rcvd = xsk_ring_cons__peek(&xsk->pktbuff->tx_comp_ring, 1, &idx);
	if (rcvd > 0) {
		xsk_ring_cons__release(&xsk->pktbuff->tx_comp_ring, rcvd);
		xsk->outstanding_tx -= rcvd;
		xsk->tx_npkts += rcvd;
	}
}

static void afxdp_send_pkt(struct xsk_info *xsk, struct user_opt *opt,
			 uint32_t header_size, uint32_t packet_size,
			 void *payload, uint64_t tx_timestamp)
{
	uint64_t cur_tx = xsk->cur_tx;	//packet_count  * frame_size
	uint32_t pkt_per_send = 1;	//Dont do bactching for now.
	uint32_t idx = 0;
	uint8_t *umem_data;
	int ret;

	if (opt->enable_poll) {
		int nfds = 1;
		struct pollfd fds[nfds + 1];
		int timeout = 1000;	/* in ms, so 1 second */

		memset(fds, 0, sizeof(fds));
		fds[0].fd = xsk_socket__fd(xsk->xskfd);
		fds[0].events = POLLOUT;

		ret = poll(fds, nfds, timeout);
		if (ret <= 0)
			return; //TODO: Return a EBUSY or EAGAIN

		if (!(fds[0].revents & POLLOUT))
			return; //TODO: Return a EBUSY or EAGAIN
	}

	/* Actual filling of payload into umem. Start a loop here if batching. */
	umem_data = xsk_umem__get_data(xsk->pktbuff->buffer, cur_tx << XSK_UMEM__DEFAULT_FRAME_SHIFT);

	memcpy(umem_data + header_size, payload, packet_size - header_size);

	if (xsk_ring_prod__reserve(&xsk->tx_ring, pkt_per_send, &idx) != pkt_per_send)
		return; //TODO return EGAGIN or ENOBUFF

	if (!opt->enable_txtime)
		tx_timestamp = 0; //Just in case

	//We need to update addr every time, for cases where the umem/tx_ring is shared.
	xsk_ring_prod__tx_desc(&xsk->tx_ring, idx)->addr = cur_tx << XSK_UMEM__DEFAULT_FRAME_SHIFT;
	xsk_ring_prod__tx_desc(&xsk->tx_ring, idx)->len = packet_size;
#ifdef WITH_XDPTBS
	xsk_ring_prod__tx_desc(&xsk->tx_ring, idx)->txtime = tx_timestamp;
#endif

	/* Update counters */
	xsk_ring_prod__submit(&xsk->tx_ring, pkt_per_send);
	xsk->outstanding_tx += pkt_per_send;

	xsk->cur_tx += pkt_per_send;
	xsk->cur_tx %= opt->x_opt.frames_per_ring;

	// Complete the TX sequence.
	ret = sendto(xsk_socket__fd(xsk->xskfd), NULL, 0, MSG_DONTWAIT, NULL, 0);
	if (ret >= 0 || errno == ENOBUFS || errno == EAGAIN || errno == EBUSY) {
		update_txstats(xsk);
		return; //TODO: Return ESUCCESS?
	}

	afxdp_exit_with_error(errno);
}

void *afxdp_send_thread(void *arg)
{
	struct user_opt *opt = (struct user_opt *)arg;

	struct custom_payload *payload;
	char buff[opt->packet_size];
	uint64_t sleep_timestamp;
	uint64_t tx_timestamp;
	tsn_packet *tsn_pkt;
	struct timespec ts;

	struct xsk_info *xsk = opt->xsk;
	uint64_t seq_num = 0;
	uint64_t i = 0;

	/* Create packet template */
	tsn_pkt = alloca(opt->packet_size);
	setup_tsn_vlan_packet(opt, tsn_pkt);

	prefill_tx_umem_rings(xsk->pktbuff->buffer, tsn_pkt,
				opt->x_opt.frames_per_ring,
				opt->x_opt.frame_size);

	payload = (struct custom_payload *) buff;

	tx_timestamp = get_time_sec(CLOCK_REALTIME);    //0.5s ahead (stmmac limitation)
	tx_timestamp += opt->offset_ns;
	tx_timestamp += 2 * NSEC_PER_SEC;

	while(!halt_tx_sig && (i < opt->frames_to_send + DEFAULT_NUM_FLUSH_PACKETS) ) {

		sleep_timestamp = tx_timestamp - opt->early_offset_ns;
		ts.tv_sec = sleep_timestamp / NSEC_PER_SEC;
		ts.tv_nsec = sleep_timestamp % NSEC_PER_SEC;
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

		payload->tx_queue = opt->x_opt.queue;
		payload->seq = seq_num;
		payload->tx_timestampA = get_time_nanosec(CLOCK_REALTIME);

		//Send one packet without caring about descriptors, make it look normal.
		if (opt->enable_txtime)
			afxdp_send_pkt(xsk, opt, 18, opt->packet_size, &buff, tx_timestamp);
		else
			afxdp_send_pkt(xsk, opt, 18, opt->packet_size, &buff, 0);

		/* Result format:
		 *   seq, user txtime, hw txtime is via trace for now
		 */
		if (verbose)
			fprintf(stdout, "%d\t%ld\n", payload->seq, payload->tx_timestampA);
		seq_num++;
		tx_timestamp += opt->interval_ns;
		fflush(stdout);

		i++;
	}

	update_txstats(xsk);

	return NULL;
	/* Calling thread is responsible of removing xdp program */
}

// Receive 1 packet at a time and print it.
void afxdp_recv_pkt(struct xsk_info *xsk, void *rbuff)
{
	struct custom_payload *payload;
	uint64_t rx_timestampD;
	tsn_packet *tsn_pkt;
	void *payload_ptr;
	(void) rbuff;
	int rcvd, i;
	int ret;

	uint32_t idx_rx = 0, idx_fq = 0;

	rcvd = xsk_ring_cons__peek(&xsk->rx_ring, 1, &idx_rx);
	if (!rcvd)
		return;

	ret = xsk_ring_prod__reserve(&xsk->pktbuff->rx_fill_ring, rcvd, &idx_fq);
	while (ret != rcvd) {
		if (ret < 0)
			afxdp_exit_with_error(-ret);
		ret = xsk_ring_prod__reserve(&xsk->pktbuff->rx_fill_ring, rcvd, &idx_fq);
	}

	for (i = 0; i < rcvd; i++) {
		uint64_t addr = xsk_ring_cons__rx_desc(&xsk->rx_ring, idx_rx)->addr;
		uint32_t len = xsk_ring_cons__rx_desc(&xsk->rx_ring, idx_rx++)->len;

		char *pkt = xsk_umem__get_data(xsk->pktbuff->buffer, addr);

		if (!len) {
			fprintf(stderr, "Warning: packet received with zero-length\n");
			continue;
		}

		rx_timestampD = get_time_nanosec(CLOCK_REALTIME);

		tsn_pkt = (tsn_packet *) pkt;
		payload_ptr = (void *) (&tsn_pkt->payload);
		payload = (struct custom_payload *) payload_ptr;

		if ((tsn_pkt->vlan_hdr == 0x81 || tsn_pkt->vlan_hdr == 0x08) &&
		    (tsn_pkt->eth_hdr == htons(0xb62c)) &&
		    (payload->seq > 0 && payload->seq < (50 * 1000 * 1000)) &&
		    (tsn_pkt->vlan_prio / 32) < 8) {

			fprintf(stdout, "%lu\t%u\t%u\t%lu\t%lu\t%lu\n",
					rx_timestampD - payload->tx_timestampA,
					payload->seq,
					payload->tx_queue,
					payload->tx_timestampA,
					*(uint64_t *)(pkt - sizeof(uint64_t)),
					rx_timestampD);
			glob_rx_seq = payload->seq;
		} else if (verbose) {
			fprintf(stderr, "Info: packet received type: 0x%x\n",
				tsn_pkt->eth_hdr);
		}
	}

	xsk_ring_prod__submit(&xsk->pktbuff->rx_fill_ring, rcvd);
	xsk_ring_cons__release(&xsk->rx_ring, rcvd);
	xsk->rx_npkts += rcvd;
	fflush(stdout);

	/* FOR SCHED_FIFO/DEADLINE */
	//TODO:implement for all threads incl afpkt?
	sched_yield();
}
