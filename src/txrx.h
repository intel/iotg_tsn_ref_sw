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
#ifndef TXRX_HEADER
#define TXRX_HEADER

#include <stdio.h>
#ifdef WITH_XDP
#include <bpf/xsk.h>
#endif
#include <time.h>
#include <string.h>
#include <stdbool.h>

#define NSEC_PER_SEC 1000000000L
#define ETH_VLAN_HDR_SZ (18)
#define IP_HDR_SZ (20)
#define UDP_HDR_SZ (8)

#define MODE_INVALID 99
#define MODE_AFXDP 1
#define MODE_AFPKT 2

#define MODE_TX 0
#define MODE_RX 1
#define MODE_FWD 2
#define MODE_BMR 3

#define XDP_MODE_SKB_COPY 0
#define XDP_MODE_NATIVE_COPY 1
#define XDP_MODE_ZERO_COPY 2

#define exit_with_error(s) {fprintf(stderr, "Error: %s\n", s); exit(EXIT_FAILURE);}

extern unsigned char src_mac_addr[];
extern unsigned char dst_mac_addr[];
extern unsigned char src_ip_addr[];
extern unsigned char dst_ip_addr[];

struct custom_payload {
	uint32_t tx_queue;
	uint32_t seq;
	uint64_t tx_timestampA;
	uint64_t rx_timestampD;
};

#ifdef WITH_XDP
/* Where we keep and track umem descriptor counters */
struct pkt_buffer {
	struct xsk_ring_prod rx_fill_ring;
	struct xsk_ring_cons tx_comp_ring;
	struct xsk_umem *umem;
	void *buffer;
};

struct xsk_opt {
	/* Note: XSK binds to queue instead of ports, 1 XSK limited to 1 queue */
	uint8_t queue;
	uint32_t xdp_flags;
	uint32_t xdp_bind_flags;

	uint16_t frame_size;		//"Maximum" packet size,
	uint16_t frames_per_ring;	//May be bounded by hardware?
};

struct xsk_info {
	struct xsk_socket *xskfd;
	struct xsk_ring_cons rx_ring;	//User process managed rings
	struct xsk_ring_prod tx_ring;	//  do not access directly
	uint32_t bpf_prog_id;

	uint32_t cur_tx;	//to track current addr (pkt count * frame_size)
	uint32_t cur_rx;

	struct pkt_buffer* pktbuff;	//UMEM and rings

	/* TODO: Some per-XDP socket statistics */
	uint64_t rx_npkts;
	uint64_t tx_npkts;
	uint64_t prev_rx_npkts;
	uint64_t prev_tx_npkts;
	uint32_t outstanding_tx;
};
#endif /* WITH_XDP */

struct user_opt {
	uint8_t mode;		//App mode: TX/RX/FWD
	uint8_t socket_mode;	//af_packet or af_xdp

	char *ifname;
	uint32_t ifindex;
	clockid_t clkid;
	int enable_hwts;

	/* TX control */
	uint32_t socket_prio;
	uint8_t vlan_prio;
	uint32_t packet_size;
	uint32_t frames_to_send;
	uint32_t interval_ns;		//Cycle time or time between packets
	uint32_t offset_ns;		//TXTIME transmission target offset from 0th second
	uint32_t early_offset_ns;	//TXTIME early offset before transmission

	/* XDP-specific */
	#ifdef WITH_XDP
	struct xsk_info *xsk;	//XDP-socket and ring information
	#endif
	/* x_opt can be defined regardless of if_xdp.h is available or not */
	struct xsk_opt x_opt;	//XDP-specific mandatory user params

	/* XDP-specific options*/
	/* Currently for txrx-afxdp only. */
	uint8_t xdp_mode;       //XDP mode: skb/nc/zc
	uint8_t enable_poll;    //XDP poll mode when sending/receiving
                               //TODO: need wake up & unaligned chunk
	uint8_t enable_txtime;
	bool need_wakeup;
	uint32_t poll_timeout;
};

/* Struct for VLAN packets with 1722 header */
typedef struct __attribute__ ((packed)) {
	/* Ethernet */
	uint8_t dst_mac[6];
	uint8_t src_mac[6];
	/* VLAN */
	uint16_t vlan_hdr;
	uint8_t vlan_prio;
	uint8_t vlan_id;
	/* Header */
	uint16_t eth_hdr;
	/* Payload */
	void *payload;
} tsn_packet;

uint64_t get_time_nanosec(clockid_t clkid);
uint64_t get_time_sec(clockid_t clkid);
void setup_tsn_vlan_packet(struct user_opt *opt, tsn_packet *pkt);

#endif
