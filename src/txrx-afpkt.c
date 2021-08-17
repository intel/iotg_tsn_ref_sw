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
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/errqueue.h>
#include <linux/if_ether.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <linux/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "txrx-afpkt.h"

#define MAX_PACKETS 10000
#define MSG_BUFLEN  1500
#define RCVBUF_SIZE (MSG_BUFLEN * MAX_PACKETS)

extern uint32_t glob_rx_seq;

/* Signal handler */
void afpkt_sigint_handler(int signum)
{
	fprintf(stderr, "Info: SIGINT triggered.\n");
	halt_tx_sig = signum;
}

/* Retrieve the hardware timestamp stored in CMSG */
static uint64_t get_timestamp(struct msghdr *msg)
{
	struct timespec *ts = NULL;
	struct cmsghdr *cmsg;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;

		switch (cmsg->cmsg_type) {
		case SO_TIMESTAMPNS:
		case SO_TIMESTAMPING:
			ts = (struct timespec *) CMSG_DATA(cmsg);
			break;
		default: /* Ignore other cmsg options */
			break;
		}
	}

	if (!ts) {
		if (verbose)
			fprintf(stderr, "Error: timestamp null. Is ptp4l initialized?\n");
		return 0;
	}

	return (ts[2].tv_sec * NSEC_PER_SEC + ts[2].tv_nsec);
}

static uint64_t extract_ts_from_cmsg(int sock, int recvmsg_flags)
{
	char data[256];
	struct msghdr msg;
	struct iovec entry;
	struct sockaddr_in from_addr;
	struct {
		struct cmsghdr cm;
		char control[512];
	} control;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &entry;
	msg.msg_iovlen = 1;
	entry.iov_base = data;
	entry.iov_len = sizeof(data);
	msg.msg_name = (caddr_t)&from_addr;
	msg.msg_namelen = sizeof(from_addr);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	recvmsg(sock, &msg, recvmsg_flags|MSG_DONTWAIT);

	return get_timestamp(&msg);
}

int init_tx_socket(struct user_opt *opt, int *sockfd,
		   struct sockaddr_ll *sk_addr)
{
	struct ifreq hwtstamp = { 0 };
	struct hwtstamp_config hwconfig = { 0 };
	int sock;

	/* Set up socket */
	sock = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_8021Q));
	if (sock < 0)
		exit_with_error("socket creation failed");

	sk_addr->sll_ifindex = opt->ifindex;
	memcpy(&sk_addr->sll_addr, dst_mac_addr, ETH_ALEN);

	if (setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &opt->socket_prio,
		       sizeof(opt->socket_prio)) < 0)
		exit_with_error("setsockopt() failed to set priority");

	/* Similar to: hwstamp_ctl -r 1 -t 1 -i <iface>
	 * This enables tx hw timestamping for all packets.
	 */
	int timestamping_flags = SOF_TIMESTAMPING_TX_HARDWARE |
				 SOF_TIMESTAMPING_RAW_HARDWARE;

	strncpy(hwtstamp.ifr_name, opt->ifname, sizeof(hwtstamp.ifr_name)-1);
	hwtstamp.ifr_data = (void *)&hwconfig;
	hwconfig.tx_type = HWTSTAMP_TX_ON;
	hwconfig.rx_filter = HWTSTAMP_FILTER_ALL;

	if (ioctl(sock, SIOCSHWTSTAMP, &hwtstamp) < 0) {
		fprintf(stderr, "%s: %s\n", "ioctl", strerror(errno));
		exit(1);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMPING, &timestamping_flags,
			sizeof(timestamping_flags)) < 0)
		exit_with_error("setsockopt SO_TIMESTAMPING");

	/* Set socket to use SO_TXTIME to pass the transmit time per packet */
	static struct sock_txtime sk_txtime;
	int use_deadline_mode = 0;
	int receive_errors = 0;

	sk_txtime.clockid = CLOCK_TAI;
	sk_txtime.flags = (use_deadline_mode | receive_errors);
	if (opt->enable_txtime && setsockopt(sock, SOL_SOCKET, SO_TXTIME,
					&sk_txtime, sizeof(sk_txtime))) {
		exit_with_error("setsockopt SO_TXTIME");
	}

	*sockfd = sock;
	return sock;
}

/* Thread which creates a socket on a specified priority and continuously
 * loops to send packets. Main thread may call multiples of this thread.
 */
void afpkt_send_thread(struct user_opt *opt, int *sockfd, struct sockaddr_ll *sk_addr)
{
	struct custom_payload *payload;
	struct timeval timeout;
	fd_set readfs, errorfs;
	uint64_t tx_timestampA;
	uint64_t tx_timestampB;
	uint64_t looping_ts;
	struct timespec ts;
	tsn_packet *tsn_pkt;
	void *payload_ptr;
	uint8_t *offset;
	int res;
	int ret;

	int interval_ns = opt->interval_ns;
	int count = opt->frames_to_send;
	clockid_t clkid = opt->clkid;
	int sock = *sockfd;
	uint32_t seq = 1;

	/* Create packet template */
	tsn_pkt = alloca(opt->packet_size);
	setup_tsn_vlan_packet(opt, tsn_pkt);

	/* TODO SO_TXTIME option but requires sendmsg which breaks sendto()*/
	
	looping_ts = get_time_sec(CLOCK_REALTIME) + (2 * NSEC_PER_SEC);
	looping_ts += opt->offset_ns;
	ts.tv_sec = looping_ts / NSEC_PER_SEC;
	ts.tv_nsec = looping_ts % NSEC_PER_SEC;

	payload_ptr = (void *) (&tsn_pkt->payload);
	payload = (struct custom_payload *) payload_ptr;

	offset = (uint8_t *) &tsn_pkt->vlan_prio;

	memcpy(&payload->tx_queue, &opt->socket_prio, sizeof(uint32_t));

	while (count && !halt_tx_sig) {
		ret = clock_nanosleep(clkid, TIMER_ABSTIME, &ts, NULL);
		if (ret) {
			fprintf(stderr, "Error: failed to sleep %d: %s", ret, strerror(ret));
			break;
		}

		tx_timestampA = get_time_nanosec(CLOCK_REALTIME);

		memcpy(&payload->seq, &seq, sizeof(uint32_t));
		memcpy(&payload->tx_timestampA, &tx_timestampA, sizeof(uint64_t));

		ret = sendto(sock,
				offset, /* AF_PACKET generates its own ETH HEADER */
				(size_t) (opt->packet_size) - 14,
				0,
				(struct sockaddr *) sk_addr,
				sizeof(struct sockaddr_ll));

		if (ret < 0)
			exit_with_error("sendto() failed");

		looping_ts += interval_ns;
		ts.tv_sec = looping_ts / NSEC_PER_SEC;
		ts.tv_nsec = looping_ts % NSEC_PER_SEC;

		count--;
		seq++;

		if (opt->enable_hwts) {
			//Note: timeout is duration not timestamp
			timeout.tv_usec = 8000;
			FD_ZERO(&readfs);
			FD_ZERO(&errorfs);
			FD_SET(sock, &readfs);
			FD_SET(sock, &errorfs);

			res = select(sock + 1, &readfs, 0, &errorfs, &timeout);
		} else {
			res = 0;
		}

		if (res > 0) {
			if (FD_ISSET(sock, &errorfs) && verbose)
				fprintf(stderr, "CSMG txtimestamp has error\n");

			tx_timestampB = extract_ts_from_cmsg(sock, MSG_ERRQUEUE);

			/* Result format: seq, user txtime, hw txtime */
			if (verbose)
				fprintf(stdout, "%u\t%lu\t%lu\n",
					seq - 1,
					tx_timestampA,
					tx_timestampB);
		} else {
			/* Print 0 if txtimestamp failed to return in time,
			 * either indicating hwtstamp is not enabled OR
			 * packet failed to transmit.
			 */
			if (verbose)
				fprintf(stdout, "%u %lu 0\n",
					seq - 1, tx_timestampA);
		}
		fflush(stdout);
	}

	close(sock);
	return;
}

/* Thread which creates a socket on a specified priority and continuously
 * loops to send packets. Main thread may call multiples of this thread.
 */
void afpkt_send_thread_etf(struct user_opt *opt, int *sockfd, struct sockaddr_ll *sk_addr)
{
	struct custom_payload *payload;
	struct timeval timeout;
	fd_set readfs, errorfs;
	uint64_t tx_timestamp;
	uint64_t tx_timestampA;
	uint64_t tx_timestampB;
	uint64_t looping_ts;
	struct timespec ts;
	tsn_packet *tsn_pkt;
	void *payload_ptr;
	int res;
	int ret;

	int interval_ns = opt->interval_ns;
	int count = opt->frames_to_send;
	clockid_t clkid = opt->clkid;
	int sock = *sockfd;
	uint32_t seq = 1;

	/* Create packet template */
	tsn_pkt = alloca(opt->packet_size);
	setup_tsn_vlan_packet(opt, tsn_pkt);

	struct cmsghdr *cmsg;
	struct msghdr msg;
	struct iovec iov;
	char control[CMSG_SPACE(sizeof(uint64_t))] = {};

	/* Construct the packet msghdr, CMSG and initialize packet payload */
	iov.iov_base = &tsn_pkt->vlan_prio;
	iov.iov_len = (size_t) opt->packet_size - 14;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = sk_addr;
	msg.msg_namelen = sizeof(struct sockaddr_ll);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control;
	msg.msg_controllen = sizeof(control);

	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_TXTIME;
	cmsg->cmsg_len = CMSG_LEN(sizeof(uint64_t));

	/* CMSG end? */

	looping_ts = get_time_sec(CLOCK_REALTIME) + (2 * NSEC_PER_SEC);
	looping_ts += opt->offset_ns;
	looping_ts -= opt->early_offset_ns;
	ts.tv_sec = looping_ts / NSEC_PER_SEC;
	ts.tv_nsec = looping_ts % NSEC_PER_SEC;

	payload_ptr = (void *) (&tsn_pkt->payload);
	payload = (struct custom_payload *) payload_ptr;

	memcpy(&payload->tx_queue, &opt->socket_prio, sizeof(uint32_t));

	while (count && !halt_tx_sig) {
		ret = clock_nanosleep(clkid, TIMER_ABSTIME, &ts, NULL);
		if (ret) {
			fprintf(stderr, "Error: failed to sleep %d: %s", ret, strerror(ret));
			break;
		}

		tx_timestampA = get_time_nanosec(CLOCK_REALTIME);
		memcpy(&payload->seq, &seq, sizeof(uint32_t));
		memcpy(&payload->tx_timestampA, &tx_timestampA, sizeof(uint64_t));

		/* Update CMSG tx_timestamp and payload before sending */
		tx_timestamp = looping_ts + opt->early_offset_ns;
		*((__u64 *) CMSG_DATA(cmsg)) = tx_timestamp;

		ret = sendmsg(sock, &msg, 0);
		if (ret < 1)
			printf("sendmsg failed: %m");

		looping_ts += interval_ns;
		ts.tv_sec = looping_ts / NSEC_PER_SEC;
		ts.tv_nsec = looping_ts % NSEC_PER_SEC;

		count--;
		seq++;

		if (opt->enable_hwts) {
			//Note: timeout is duration not timestamp
			timeout.tv_usec = 2000;
			FD_ZERO(&readfs);
			FD_ZERO(&errorfs);
			FD_SET(sock, &readfs);
			FD_SET(sock, &errorfs);

			res = select(sock + 1, &readfs, 0, &errorfs, &timeout);
		} else {
			res = 0;
		}

		if (res > 0) {
			if (FD_ISSET(sock, &errorfs) && verbose)
				fprintf(stderr, "CSMG txtimestamp has error\n");

			tx_timestampB = extract_ts_from_cmsg(sock, MSG_ERRQUEUE);

			/* Result format: seq, user txtime, hw txtime */
			if (verbose)
				fprintf(stdout, "%u\t%lu\t%lu\n",
					seq - 1,
					tx_timestampA,
					tx_timestampB);
		} else {
			/* Print 0 if txtimestamp failed to return in time,
			 * either indicating hwtstamp is not enabled OR
			 * packet failed to transmit.
			 */
			if (verbose)
				fprintf(stdout, "%u %lu 0\n",
					seq - 1, tx_timestampA);
		}
		fflush(stdout);
	}

	close(sock);
	return;
}

/* Create a RAW socket to receive all incoming packets from an interface. */
int init_rx_socket(uint16_t etype, int *sock, char *interface)
{
	struct sockaddr_ll addr;
	struct ifreq if_request;
	int rsock;
	int ret;
	int timestamping_flags;
	int rcvbuf_size;
	struct hwtstamp_config hwconfig = {0};

	rcvbuf_size = RCVBUF_SIZE;
	if (sock == NULL)
		return -1;
	*sock = -1;

	rsock = socket(PF_PACKET, SOCK_RAW, htons(etype));
	if (rsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name)-1);

	ret = ioctl(rsock, SIOCGIFINDEX, &if_request);
	if (ret < 0) {
		close(rsock);
		fprintf(stderr, "Error: Couldn't get interface index");
		return -1;
	}

	/* Increase receive buffer size - requires root priveleges. */
	if (setsockopt(rsock, SOL_SOCKET, SO_RCVBUFFORCE, &rcvbuf_size,
		sizeof(int)) < 0)
		fprintf(stderr, "Error setting sockopt: %s\n", strerror(errno));

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = if_request.ifr_ifindex;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(etype);

	ret = bind(rsock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret != 0) {
		fprintf(stderr, "%s - Error on bind %s\n",
			__func__, strerror(errno));
		close(rsock);
		return -1;
	}

	if (dst_mac_addr[0] != '\0') {
		struct packet_mreq mreq;

		mreq.mr_ifindex = addr.sll_ifindex;
		mreq.mr_type = PACKET_MR_MULTICAST;
		mreq.mr_alen = ETH_ALEN;
		memcpy(&mreq.mr_address, dst_mac_addr, ETH_ALEN);

		ret = setsockopt(rsock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
					&mreq, sizeof(struct packet_mreq));
		if (ret < 0) {
			perror("Couldn't set PACKET_ADD_MEMBERSHIP");
			close(rsock);
			return -1;
		}
	}

	/* Similar to: hwstamp_ctl -r 1 -t 1 -i <iface>
	 * This enables rx hw timestamping for all packets.
	 */
	if_request.ifr_data = (void *)&hwconfig;

	hwconfig.tx_type = HWTSTAMP_TX_ON;
	hwconfig.rx_filter = HWTSTAMP_FILTER_ALL;

	if (ioctl(rsock, SIOCSHWTSTAMP, &if_request) < 0) {
		fprintf(stderr, "%s: %s\n", "ioctl", strerror(errno));
		exit(1);
	}

	timestamping_flags = SOF_TIMESTAMPING_RX_HARDWARE |
				SOF_TIMESTAMPING_RAW_HARDWARE;

	if (setsockopt(rsock, SOL_SOCKET, SO_TIMESTAMPING, &timestamping_flags,
			sizeof(timestamping_flags)) < 0)
		exit_with_error("setsockopt SO_TIMESTAMPING");

	*sock = rsock;
	return 0;
}

int afpkt_recv_pkt(int sock, struct user_opt *opt)
{
	uint64_t rx_timestampC, rx_timestampD;
	struct sockaddr_in host_address;
	struct custom_payload *payload;
	struct msghdr msg;
	struct iovec iov;
	char buffer[MSG_BUFLEN];
	char control[1024];
	tsn_packet *tsn_pkt;
	void *payload_ptr;
	int ret;

	ret = 0;

	/* Initialize empty msghdrs and buffers */
	bzero(&host_address, sizeof(struct sockaddr_in));
	host_address.sin_family = AF_INET;
	host_address.sin_port = htons(0);
	host_address.sin_addr.s_addr = INADDR_ANY;

	iov.iov_base = buffer;
	iov.iov_len = 128; //TODO: use correct length based on VLAN header
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = &host_address;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_control = control;
	msg.msg_controllen = 128;

	/* Use non-blocking recvmsg to poll for packets, do nothing if none */
	ret = recvmsg(sock, &msg, MSG_DONTWAIT);
	if (ret <= 0) {
		usleep(1); /*No message in buffer, do nothing*/
		return 0;
	}
	rx_timestampD = get_time_nanosec(CLOCK_REALTIME);

	/* Point to payload's location in received packet's buffer */
	tsn_pkt = (tsn_packet *) (buffer - 4);
	payload_ptr = (void *) (&tsn_pkt->payload);
	payload = (struct custom_payload *) payload_ptr;

	if (opt->enable_hwts)
		rx_timestampC = get_timestamp(&msg);
	else
		rx_timestampC = 0;

	/* Do simple checks and filtering */
	if (payload->tx_queue > 8 || payload->seq > (50 * 1000 * 1000)) {
		if (verbose)
			fprintf(stderr, "Warn: Skipping invalid packet\n");
		return -1;
	} else if (rx_timestampC == 0) {
		if (verbose)
			fprintf(stderr, "Warn: No RX HW timestamp.\n");
	}

	/* Result format:
	 *   u2u latency, seq, queue, user txtime, hw rxtime, user rxtime
	 */
	fprintf(stdout, "%ld\t%d\t%d\t%ld\t%ld\t%ld\n",
			rx_timestampD - payload->tx_timestampA,
			payload->seq,
			payload->tx_queue,
			payload->tx_timestampA,
			rx_timestampC,
			rx_timestampD);
	fflush(stdout);
	glob_rx_seq = payload->seq;

	return 0;
}
