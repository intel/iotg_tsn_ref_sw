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
#include <argp.h>
#include <stdarg.h>
#include <signal.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <pthread.h>

#include "txrx-afxdp.h"
#include "txrx-afpkt.h"

#define VLAN_ID 3
#define DEFAULT_NUM_FRAMES 1000
#define DEFAULT_TX_PERIOD 100000
#define DEFAULT_PACKETS 10
#define DEFAULT_SOCKET_PRIORITY 0
#define DEFAULT_PACKET_SIZE 64
#define DEFAULT_TXTIME_OFFSET 0
#define DEFAULT_EARLY_OFFSET 100000
#define DEFAULT_XDP_FRAMES_PER_RING 4096 //Minimum is 4096
#define DEFAULT_XDP_FRAMES_SIZE 4096
#define BATCH_SIZE 64	//for l2fwd

/* Globals */
unsigned char src_mac_addr[] = { 0xaa, 0x00, 0xaa, 0x00, 0xaa, 0x00};
unsigned char dst_mac_addr[] = { 0x22, 0xbb, 0x22, 0xbb, 0x22, 0xbb};
unsigned char src_ip_addr[] = { 169, 254, 1, 11};
unsigned char dst_ip_addr[] = { 169, 254, 1, 22};
struct xsk_info *glob_xskinfo_ptr;
uint32_t glob_xdp_flags;
int glob_ifindex;
int halt_tx_sig;
int verbose;

uint64_t get_time_nanosec(clockid_t clkid)
{
	struct timespec now;

	clock_gettime(clkid, &now);
	return now.tv_sec * NSEC_PER_SEC + now.tv_nsec;
}

uint64_t get_time_sec(clockid_t clkid)
{
	struct timespec now;

	clock_gettime(clkid, &now);
	return now.tv_sec * NSEC_PER_SEC;
}

/* Pre-fill TSN packet with default and user-defined parameters */
void setup_tsn_vlan_packet(struct user_opt *opt, tsn_packet *pkt)
{
	memset(pkt, 0xab, opt->packet_size);

	memcpy(&pkt->src_mac, src_mac_addr, sizeof(pkt->src_mac));
	memcpy(&pkt->dst_mac, dst_mac_addr, sizeof(pkt->dst_mac));

	pkt->vlan_hdr = htons(ETHERTYPE_VLAN);
	pkt->vlan_id = VLAN_ID;
	pkt->vlan_prio = opt->vlan_prio;

	pkt->eth_hdr = htons(ETH_P_TSN);

	/* WORKAROUND: transmit only 0xb62c ETH UADP header packets
	 *  due to a bug where IEEE 1722 (ETH_P_TSN) packets are
	 *  always steered into RX Q0 regardless of its VLAN
	 *  priority
	 */
	pkt->eth_hdr = htons(0xb62c);
}

/* Argparse */
static struct argp_option options[] = {
	{"interface",	'i',	"NAME",	0, "interface name"},

	{0,0,0,0, "Socket Mode:" },
	{"afxdp",	'X',	0,	0, "run using AF_XDP socket"},
	{"afpkt",	'P',	0,	0, "run using AF_PACKET socket"},

	{0,0,0,0, "Mode:" },
	{"transmit",	't',	0,	0, "transmit only"},
	{"receive",	'r',	0,	0, "receive only"},
	{"forward",	'f',	0,	0, "L2-forward (receive-send) packets (AF_XDP only)"},
	{"boomerang",	'b',	0,	0, "L2-send-receive packets (AF_XDP only)"},

	{0,0,0,0, "XDP Mode:" },
	{"zero-copy",	'z',	0,	0, "zero-copy mode"},
	{"native-copy",	'c',	0,	0, "direct/native copy mode"},
	{"skb",		's',	0,	0, "skb mode"},

	{0,0,0,0, "AF_XDP control:" },
	{"launchtime",	'T',	0,	0, "enable time-based per-packet tx scheduling (TBS)"},
	{"polling",	'p',	0,	0, "enable polling mode"},
	{"wakeup",	'w',	0,	0, "enable need_wakeup"},
	{"vlan-prio",	'q',	"NUM",	0, "packet vlan priority, also socket priority\n"
					   "	Def: 0 | Min: 0 | Max: 7"},
	/* Reserved: u / w */

	{0,0,0,0, "TX control:" },
	{"packet-size",	'l',	"NUM",	0, "packet size/length incl. headers in bytes\n"
					   "	Def: 64 | Min: 64 | Max: 1500"},
	{"cycle-time",	'y',	"NSEC",	0, "tx period/interval/cycle-time\n"
					   "	Def: 100000ns | Min: 25000ns | Max: 50000000ns"},
	{"frames-to-send", 'n', "NUM",	0, "number of packets to transmit\n"
					   "	Def: 1000 | Min: 1 | Max: 10000000"},
	{"dst-mac-addr",   'd', "MAC_ADDR",	0, "destination mac address\n"
						   "	Def: 22:bb:22:bb:22:bb"},

	{0,0,0,0, "LaunchTime/TBS-specific:\n(where base is the 0th ns of current second)" },
	{"transmit-offset",'o', "NSEC",	0, "packet txtime positive offset\n"
					   "	Def: 0ns | Min: 0ns | Max: 100000000ns"},
	{"early-offset",   'e', "NSEC",	0, "early execution negative offset\n"
					   "	Def: 100000ns | Min: 0ns | Max: 10000000ns"},

	{0,0,0,0, "Misc:" },
	{"hw-timestamps",	'h',	0,	0, "retrieve per-packet hardware timestamps (AF_PACKET)"},
	{"verbose",	'v',	0,	0, "verbose & print warnings"},
	{ 0 }
};

static error_t parser(int key, char *arg, struct argp_state *state)
{
	/* Get the input argument from argp_parse, which we */
	/* know is a pointer to our user_opt structure. */
	struct user_opt *opt = state->input;
	int ret;
	int len = 0;
	char *str_end = NULL;
	errno = 0;
	long res = 0;

	switch (key) {
	case 'v':
		verbose = 1;
		break;
	case 'h':
		opt->enable_hwts = 1;
		break;
	case 'i':
		opt->ifname = strdup(arg);
		break;
	case 'q':
		len = strlen(arg);
		res = strtol((const char *)arg, &str_end, 10);
		if (errno || res < 0 || res >= 7 || str_end != &arg[len])
			exit_with_error("Invalid queue number/socket priority. Check --help");
		opt->x_opt.queue = opt->socket_prio = (uint32_t)res;
		opt->vlan_prio = opt->socket_prio * 32;
		break;
	case 'X':
		opt->socket_mode = MODE_AFXDP;
		break;
	case 'P':
		opt->socket_mode = MODE_AFPKT;
		break;
	case 't':
		opt->mode = MODE_TX;
		break;
	case 'r':
		opt->mode = MODE_RX;
		break;
	case 'f':
		opt->mode = MODE_FWD;
		break;
	case 'b':
		opt->mode = MODE_BMR;
		break;
	case 'z':
		opt->xdp_mode = XDP_MODE_ZERO_COPY;
		break;
	case 'c':
		opt->xdp_mode = XDP_MODE_NATIVE_COPY;
		break;
	case 's':
		opt->xdp_mode = XDP_MODE_SKB_COPY;
		break;
	case 'T':
		opt->enable_txtime = 1;
		break;
	case 'p':
		opt->enable_poll = 1;
		break;
	case 'w':
		opt->need_wakeup = 1;
		break;
	case 'l':
		len = strlen(arg);
		res = strtol((const char *)arg, &str_end, 10);
		if (errno || res < 64 || res > 1500 || str_end != &arg[len])
			exit_with_error("Invalid packet size. Check --help");
		opt->packet_size = (uint32_t)res;
		break;
	case 'y':
		len = strlen(arg);
		res = strtol((const char *)arg, &str_end, 10);
		if (errno || res < 25000 || res > 50000000 || str_end != &arg[len])
			exit_with_error("Invalid cycle time. Check --help");
		opt->interval_ns = (uint32_t)res;
		break;
	case 'n':
		len = strlen(arg);
		res = strtol((const char *)arg, &str_end, 10);
		if (errno || res < 1 || res > 10000000 || str_end != &arg[len])
			exit_with_error("Invalid number of frames to send. Check --help");
		opt->frames_to_send = (uint32_t)res;
		break;
	case 'o':
		len = strlen(arg);
		res = strtol((const char *)arg, &str_end, 10);
		if (errno || res < 0 || res > 100000000 || str_end != &arg[len])
			exit_with_error("Invalid offset. Check --help");
		opt->offset_ns = (uint32_t)res;
		break;
	case 'e':
		len = strlen(arg);
		res = strtol((const char *)arg, &str_end, 10);
		if (errno || res  < 0 || res > 10000000 || str_end != &arg[len])
			exit_with_error("Invalid early offset. Check --help");
		opt->early_offset_ns = (uint32_t)res;
		break;
	case 'd':
		ret = sscanf(arg, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			&dst_mac_addr[0], &dst_mac_addr[1], &dst_mac_addr[2],
			&dst_mac_addr[3], &dst_mac_addr[4], &dst_mac_addr[5]);
		if (ret != 6)
			exit_with_error("Invalid destination MAC addr. Check --help");
		break;
	/* Reserved: u / w */
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static char usage[] = "-i <interface> -P [r|t]\n"
		      "-i <interface> -X [r|t|f] [z|c|s] -q <queue>";

static char summary[] = "  AF_XDP & AF_PACKET Transmit-Receive Application";

static struct argp argp = { options, parser, usage, summary };

int main(int argc, char *argv[])
{
	struct pollfd fds[1];
	struct user_opt opt;
	int ret = 0;

	/* Defaults */
	opt.socket_mode = MODE_INVALID;
	opt.mode = MODE_INVALID;
	opt.ifname = NULL;
	verbose = 0;

	opt.socket_prio = DEFAULT_SOCKET_PRIORITY;
	opt.vlan_prio = DEFAULT_SOCKET_PRIORITY;
	opt.frames_to_send = DEFAULT_NUM_FRAMES;
	opt.packet_size = DEFAULT_PACKET_SIZE;
	opt.interval_ns = DEFAULT_TX_PERIOD;

	opt.x_opt.frames_per_ring =  DEFAULT_XDP_FRAMES_PER_RING;
	opt.x_opt.frame_size = DEFAULT_XDP_FRAMES_SIZE;
	opt.early_offset_ns = DEFAULT_EARLY_OFFSET;
	opt.offset_ns = DEFAULT_TXTIME_OFFSET;
	opt.xdp_mode = XDP_MODE_ZERO_COPY;
	opt.clkid = CLOCK_REALTIME;
	opt.enable_poll = 0;
	opt.enable_hwts = 0;
	opt.enable_txtime = 0;
	opt.need_wakeup = false;
	opt.poll_timeout = 1000;

	argp_parse(&argp, argc, argv, 0, 0, &opt);

	/* Parse user inputs */

	if (!opt.ifname)
		exit_with_error("Please specify interface using -i\n");

	opt.ifindex = if_nametoindex(opt.ifname);
	if (!opt.ifindex) {
		fprintf(stderr, "ERROR: interface \"%s\" do not exist\n",
			opt.ifname);
		exit(EXIT_FAILURE);
	}

	char buff[opt.packet_size];
	pthread_t thread1;

	switch (opt.socket_mode) {
	case MODE_AFPKT:
		signal(SIGINT, afpkt_sigint_handler);
		signal(SIGTERM, afpkt_sigint_handler);
		signal(SIGABRT, afpkt_sigint_handler);

		struct sockaddr_ll sk_addr = {
			.sll_family = AF_PACKET,
			.sll_protocol = htons(ETH_P_8021Q),
			.sll_halen = ETH_ALEN,
		};
		int sockfd;

		switch (opt.mode) {
		case MODE_TX:
			ret = init_tx_socket(&opt, &sockfd, &sk_addr);
			if (!ret)
				perror("init_tx_socket failed");

			if (!opt.enable_txtime)
				afpkt_send_thread(&opt, &sockfd, &sk_addr);
			else
				afpkt_send_thread_etf(&opt, &sockfd, &sk_addr);

			break;
		case MODE_RX:
			/* WORKAROUND: receive only 0xb62c ETH UADP header packets
			 *  due to a bug where IEEE 1722 (ETH_P_TSN) packets are
			 *  always steered into RX Q0 regardless of its VLAN
			 *  priority
			 */
			ret = init_rx_socket(0xb62c, &sockfd, opt.ifname);
			if (ret != 0)
				perror("initrx_socket failed");

			while (!halt_tx_sig)
				afpkt_recv_pkt(sockfd, &opt);

			close(sockfd);
			break;
		case MODE_FWD: /* FALLTHRU */
		default:
			exit_with_error("Invalid AF_XDP mode: Please specify -t, -r, or -f.");
			break;
		}
		close(sockfd);
		break;
	case MODE_AFXDP:

		init_xdp_socket(&opt); /* Same for all modes */

		if (!opt.xsk)
			afxdp_exit_with_error(EXIT_FAILURE);

		signal(SIGINT, afxdp_sigint_handler);
		signal(SIGTERM, afxdp_sigint_handler);
		signal(SIGABRT, afxdp_sigint_handler);

		switch (opt.mode) {
		case MODE_TX:
			usleep(7000000);
			afxdp_send_thread(&opt);

			break;
		case MODE_RX:
			//usleep(3000000);
			while (!halt_tx_sig)
				afxdp_recv_pkt(opt.xsk, buff);
			break;
		case MODE_FWD:
			memset(fds, 0, sizeof(fds));
			fds[0].fd = xsk_socket__fd(opt.xsk->xskfd);
			fds[0].events =  POLLOUT | POLLIN;
			while (!halt_tx_sig) {
				if (opt.enable_poll) {
					ret = poll(fds, 1, opt.poll_timeout);
					if (ret <= 0)
						continue;
				}
				afxdp_fwd_pkt(opt.xsk, fds, &opt);
			}
			break;
		case MODE_BMR:
			ret = pthread_create(&thread1, NULL, afxdp_send_thread, &opt);
			if (ret) {
				fprintf(stderr, "pthread_create failed\n");
				break;
			}
			while (!halt_tx_sig) {
				afxdp_recv_pkt(opt.xsk, buff);
			}

			ret = pthread_join(thread1, NULL);
			if (ret)
				fprintf(stderr, "pthread_join returned %d", ret);

			break;
		default:
			exit_with_error("Invalid AF_XDP mode: Please specify -t, -r, -f or -b.");
			break;
		}
		/* Close XDP Application */
		xdpsock_cleanup();
		break;
	default:
		exit_with_error("Invalid socket type: Please specify -X or -P.");
		break;
	}

	return 0;
}
