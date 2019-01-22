/******************************************************************************
 *
 * Copyright (c) 2019, Intel Corporation
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
 *  3. Neither the name of the Intel Corporation nor the names of its
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
 ******************************************************************************/

#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <linux/if.h>

#include "talker_mrp_client.h"
#include "avb_gptp.h"
#include "avb_avtp.h"

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <linux/if_packet.h>
#include <sys/types.h>
#include <sys/stat.h>

#define VERSION_STR "1.0"

#ifndef SO_TXTIME
#define SO_TXTIME 61
#endif

#ifndef CLOCK_INVALID
#define CLOCK_INVALID -1
#endif

static clockid_t get_clockid(int fd)
{
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)((~(clockid_t)(fd) << 3) | CLOCKFD)
	return FD_TO_CLOCKID(fd);
}

/*
 * Structure to hold ETF socket option
 */
struct sock_txtime {
	clockid_t clockid;
	uint16_t flags;
};

/*
 * SO_TXTIME's socket flags for ETF usage
 */
enum txtime_flags {
	SOF_TXTIME_DEADLINE_MODE = (1 << 0),
	SOF_TXTIME_REPORT_ERRORS = (1 << 1),

	SOF_TXTIME_FLAGS_LAST = SOF_TXTIME_REPORT_ERRORS,
	SOF_TXTIME_FLAGS_MASK = (SOF_TXTIME_FLAGS_LAST - 1) |
				 SOF_TXTIME_FLAGS_LAST
};


static struct sock_txtime sk_txtime;
#define SRC_CHANNELS (2)
#define GAIN (0.5)
#define L16_PAYLOAD_TYPE (96)			/*
						 * For layer 4 transport -
						 * should be negotiated
						 * via RTSP
						 */

#define ID_B_HDR_EXT_ID (0)			/*
						 * For layer 4 transport -
						 * should be negotiated
						 * via RTSP
						 */
#define L2_SAMPLES_PER_FRAME (6)
#define L4_SAMPLES_PER_FRAME (60)
#define L4_SAMPLE_SIZE (2)
#define CHANNELS (2)
#define RTP_SUBNS_SCALE_NUM (20000000)
#define RTP_SUBNS_SCALE_DEN (4656613)
#define XMIT_DELAY (2000000)			/* ns */
#define RENDER_DELAY (XMIT_DELAY+2000000)	/* ns */
#define L2_PACKET_IPG (125000)			/*
						 * (1) packet every
						 * 125 usec
						 */

#define L4_PACKET_IPG (1250000)			/*
						 * (1) packet every
						 * 1.25 millisec
						 */
#define L4_PORT ((uint16_t)5004)
#define PKT_SZ (100)

#define PKT_BUF_MAX 512
#define ETH_PTP_DEV_PATH "/sys/class/net/%s/device/ptp/"
#define ETH_PTP_PATH_SZ 256
#define PTP_DEV_NAME "/dev/%s"
#define PTP_BUFSZ_MAX 512
#define SENDMSG_DELAY_REDUCTION 700		/* Constant to
						 * reduce usleep
						 * duration for
						 * set_qdisc = 0
						 */

typedef long double FrequencyRatio;
int32_t set_qdisc = 1;
volatile int32_t *halt_tx_sig;			/* Global variable
						 * for signal handler
						 */
uint32_t use_etf = 0;
uint32_t transport = -1;

/*
 * Variables used in sleep-time correction algorithm.
 */
int packet_count = 0;
uint64_t before_sleep_time_ns = 0;
uint64_t after_sleep_time_ns = 0;
uint64_t oversleep_time_ns = 0;
uint64_t txtime = 0;

/*
 * CBS & ETF configuration for IEEE1722
 */
int32_t CBS_IDLESLOPE = 7808;
int32_t CBS_SENDSLOPE = -992192;
int32_t CBS_HICREDIT = 12;
int32_t CBS_LOCREDIT = -97;
int32_t CBS_OFFLOAD = 1;
int32_t ETF_DELTA = 5000000;

/*
 * CBS & ETF configuration for RTP
 */
int32_t CBS_IDLESLOPE_RTP = 21381;
int32_t CBS_SENDSLOPE_RTP = -978619;
int32_t CBS_HICREDIT_RTP = 33;
int32_t CBS_LOCREDIT_RTP = -285;
int32_t CBS_OFFLOAD_RTP = 1;
int32_t ETF_DELTA_RTP = 200000;
/*
 * Header structure for RDP packets
 */
typedef struct __attribute__ ((packed)) {
	uint8_t version_length;
	uint8_t DSCP_ECN;
	uint16_t ip_length;
	uint16_t id;
	uint16_t fragmentation;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t hdr_cksum;
	uint32_t src;
	uint32_t dest;

	uint16_t source_port;
	uint16_t dest_port;
	uint16_t udp_length;
	uint16_t cksum;

	uint8_t version_cc;
	uint8_t mark_payload;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc;

	uint8_t tag[2];
	uint16_t total_length;
	uint8_t tag_length;
	uint8_t seconds[3];
	uint32_t nanoseconds;
} IP_RTP_Header;

/*
 * Supporting structure for RDP packets
 */
typedef struct __attribute__ ((packed)) {
	uint32_t source;
	uint32_t dest;
	uint8_t zero;
	uint8_t protocol;
	uint16_t length;
} IP_PseudoHeader;

/* globals */
static const char *version_str = "simple-talker-csmg v" VERSION_STR "\n"
				 "Copyright (c) 2019, Intel Corporation\n";

unsigned char glob_station_addr[] = { 0, 0, 0, 0, 0, 0 };
unsigned char glob_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char glob_l2_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0e, 0x80 };
unsigned char glob_l3_dest_addr[] = { 224, 0, 0, 115 };

uint16_t inet_checksum(uint8_t *ip, int len)
{
	uint32_t sum = 0;	/* assume 32 bit long, 16 bit short */

	while (len > 1) {
		sum += *((uint16_t *) ip);
		ip += 2;
		/* if high order bit set, fold */
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	/* take care of left over byte */
	if (len)
		sum += (uint16_t) *(uint8_t *)ip;

	while (sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

/*
 * Checksum calculation for RDP packets
 */
uint16_t inet_checksum_sg(struct iovec *buf_iov, size_t buf_iovlen)
{
	size_t i;
	uint32_t sum = 0;	/* assume 32 bit long, 16 bit short */
	uint8_t residual;
	int has_residual = 0;

	for (i = 0; i < buf_iovlen; ++i, ++buf_iov) {
		if (has_residual) {
			if (buf_iov->iov_len > 0) {
				/* if high order bit set, fold */
				if (sum & 0x80000000)
					sum = (sum & 0xFFFF) + (sum >> 16);
				sum += residual |
					(*((uint8_t *) buf_iov->iov_base)
					 << 8);
				buf_iov->iov_base += 1;
				buf_iov->iov_len -= 1;
			} else {
				/* if high order bit set, fold */
				if (sum & 0x80000000)
					sum = (sum & 0xFFFF) + (sum >> 16);
				sum += (uint16_t) residual;
			}
			has_residual = 0;

		}
		while (buf_iov->iov_len > 1) {
			/* if high order bit set, fold */
			if (sum & 0x80000000)
				sum = (sum & 0xFFFF) + (sum >> 16);
			sum += *((uint16_t *) buf_iov->iov_base);
			buf_iov->iov_base += 2;
			buf_iov->iov_len -= 2;
		}
		if (buf_iov->iov_len) {
			residual = *((uint8_t *) buf_iov->iov_base);
			has_residual = 1;
		}
	}
	if (has_residual)
		sum += (uint16_t) residual;

	while (sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

static inline uint64_t ST_rdtsc(void)
{
	uint64_t ret;
	uint64_t c, d;

	asm volatile ("rdtsc":"=a" (c), "=d"(d));

	ret = d;
	ret <<= 32;
	ret |= c;
	return ret;
}

void gensine32(int32_t *buf, unsigned int count)
{
	long double interval = (2 * ((long double)M_PI)) / count;
	unsigned int i;

	for (i = 0; i < count; ++i) {
		buf[i] =
		    (int32_t)(MAX_SAMPLE_VALUE * sinl(i * interval) * GAIN);
	}
}

int get_samples(unsigned int count, int32_t *buffer)
{
	static int init = 0;
	static int32_t samples_onechannel[100];
	static unsigned int index = 0;

	if (init == 0) {
		gensine32(samples_onechannel, 100);
		init = 1;
	}

	while (count > 0) {
		int i;

		for (i = 0; i < SRC_CHANNELS; ++i)
			*(buffer++) = samples_onechannel[index];
		index = (index + 1) % 100;
		--count;
	}

	return 0;
}

void sigint_handler(int signum)
{
	*halt_tx_sig = signum;
}

/*
 * Remap L3 packet destination address to
 * L2 destination address
 */
void l3_to_l2_multicast(unsigned char *l2, unsigned char *l3)
{
	 l2[0]  = 0x1;
	 l2[1]  = 0x0;
	 l2[2]  = 0x5e;
	 l2[3]  = l3[1] & 0x7F;
	 l2[4]  = l3[2];
	 l2[5]  = l3[3];
}

/*
 * Request MAC address from the interface
 */
int get_mac_address(char *interface)
{
	struct ifreq if_request;
	int lsock;
	int rc;

	lsock = socket(PF_PACKET, SOCK_RAW, htons(0x800));
	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));
	strncpy(if_request.ifr_name, interface,
		sizeof(if_request.ifr_name) - 1);
	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memcpy(glob_station_addr, if_request.ifr_hwaddr.sa_data,
	       sizeof(glob_station_addr));
	close(lsock);
	return 0;
}

/*
 * Supporting function to execute external application
 * and return its result in a buffer.
 */
int run_tc_cmd(char *cmd, char **buf)
{
	FILE *fp;
	char line[256];
	size_t i = 0;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to run command %s\n. Exiting...", cmd);
		return -1;
	}

	while (fgets(line, sizeof(line)-1, fp) != NULL) {
		buf[i] = strdup(line);
		++(i);
		if (i > sizeof(buf)) {
			i = -1;
			break;
		}
	}

	pclose(fp);
	free(fp);
	fp = NULL;
	return i;
}

/*
 * Support function to delete queue discipline
 */
int32_t delete_qdisc(char *iface)
{
	/*
	 * Command to be executed:
	 *
	 * tc qdisc del dev <iface> root
	 */
	char *qdisc_del_cmd = "tc qdisc del dev ";
	char *root = " root";
	char delete_qdisc_cmd_buf[256];
	char *output_buf[256];

	snprintf(delete_qdisc_cmd_buf, sizeof(delete_qdisc_cmd_buf), "%s%s%s",
		 qdisc_del_cmd, iface, root);

	if (run_tc_cmd(delete_qdisc_cmd_buf, output_buf) == -1) {
		printf("run_tc_cmd failed!\n");
		return -1;
	}

	/* Give time for the adapter to reset */
	sleep(3);
	return 1;
}

/*
 * Check for previously declared queue discipline.
 *
 * This function will run delete_qdisc if there is any
 * previously declared queue discipline.
 */
int check_qdisc(char *iface)
{
	int size = 0;
	char *qdisc_show_cmd = "tc qdisc show dev ";
	char show_cmd_buf[256];
	char *output_buf[256];

	snprintf(show_cmd_buf, sizeof(show_cmd_buf), "%s%s",
		 qdisc_show_cmd, iface);

	size = run_tc_cmd(show_cmd_buf, output_buf);
	if (size <= 0) {
		fprintf(stderr, "Something is wrong. Exiting...\n");
		return -1;
	}

	char *token = strtok(output_buf[0], " ");

	if (token == NULL) {
		fprintf(stderr, "Token is NULL!");
		return -1;
	}
	token = strtok(NULL, " ");
	if (token == NULL) {
		fprintf(stderr, "Token is NULL!");
		return -1;
	}
	if (strcmp("mqprio", token) == 0) {
		fprintf(stdout, "mqprio exist. Resetting it...\n");
		size = 0;
		if (delete_qdisc(iface) < 0)
			return -1;
	}

	return 1;
}

/************************************************************************
 * Credit Based Shaper (CBS) queue discipline (qdisc).
 *
 * This qdisc implments the shaping algorithm, which define rate
 * limiting method to the traffic.
 ************************************************************************/
int set_cbs_qdisc(char *iface, int etf_on)
{
	char *qdisc_add_cmd = "tc qdisc replace dev ";
	char *cbs_cmd = " parent 100:1 cbs ";
	char *cbs_etf_cmd = " handle 200";
	char set_cbs_qdisc_cmd_buf[256];
	char *output_buf[256];
	int32_t idleslope;
	int32_t sendslope;
	int32_t hicredit;
	int32_t locredit;
	int32_t offload;

	printf("etf_on: %d\n", etf_on);

	if (transport == 2) {
		idleslope = CBS_IDLESLOPE;
		sendslope = CBS_SENDSLOPE;
		hicredit = CBS_HICREDIT;
		locredit = CBS_LOCREDIT;
		offload = CBS_OFFLOAD;
	} else {
		idleslope = CBS_IDLESLOPE_RTP;
		sendslope = CBS_SENDSLOPE_RTP;
		hicredit = CBS_HICREDIT_RTP;
		locredit = CBS_LOCREDIT_RTP;
		offload = CBS_OFFLOAD_RTP;
	}

	if (etf_on == 2) {
		printf("Setting CBS with ETF.\n");
		snprintf(set_cbs_qdisc_cmd_buf, sizeof(set_cbs_qdisc_cmd_buf),
			 "%s%s%s%s%s%d%s%d%s%d%s%d%s%d",
			 qdisc_add_cmd, iface, cbs_etf_cmd, cbs_cmd,
			 "idleslope ", idleslope, " sendslope ",
			 sendslope, " hicredit ", hicredit,
			 " locredit ", locredit, " offload ", offload);
	} else if (etf_on == 1) {
		snprintf(set_cbs_qdisc_cmd_buf, sizeof(set_cbs_qdisc_cmd_buf),
			 "%s%s%s%s%d%s%d%s%d%s%d%s%d",
			 qdisc_add_cmd, iface, cbs_cmd, "idleslope ",
			 idleslope, " sendslope ", sendslope,
			 " hicredit ", hicredit, " locredit ",
			 locredit, " offload ", offload);
	}

	if (run_tc_cmd(set_cbs_qdisc_cmd_buf, output_buf) == -1) {
		printf("run_tc_cmd failed!\n");
		return -1;
	}

	/* Give time for the adapter to reset */
	sleep(3);
	return 1;
}

/************************************************************************
 * ETF (formerly known as 'tbs') queue discipline (qdisc).
 *
 * ETF qdisc allows application to control the instant when a packet
 * should leave the network controller. ETF achieves that by buffering
 * packets until a configurable time before their transmission time
 * (i.e. txtime, or deadline), which can be configured through the
 * delta option.
 *
 * In this application, we pair ETF qdisc with CBS. This combination
 * should further reduce the latency jitter compared to using CBS only.
 *
 * ETF Reference: http://man7.org/linux/man-pages/man8/tc-etf.8.html
 *
 ************************************************************************/
int set_etf_qdisc(char *iface)
{
	/*
	 * Command to be executed:
	 *
	 * tc qdisc replace dev enp1s0
	 *      parent 200:1
	 *      etf delta
	 *      clockid CLOCK_TAI
	 *      offload
	 */

	char *qdisc_add_cmd = "tc qdisc replace dev ";
	char *etf_cmd = " parent 200:1 etf delta ";
	char *etf_cmd_clkid = " clockid CLOCK_TAI offload";
	char set_etf_qdisc_cmd_buf[256];
	char *output_buf[256];
	int32_t etf_delta;

	if (transport == 2)
		etf_delta = ETF_DELTA;
	else
		etf_delta = ETF_DELTA_RTP;

	snprintf(set_etf_qdisc_cmd_buf, sizeof(set_etf_qdisc_cmd_buf),
		 "%s%s%s%d%s", qdisc_add_cmd, iface, etf_cmd, etf_delta, etf_cmd_clkid);

	if (run_tc_cmd(set_etf_qdisc_cmd_buf, output_buf) == -1) {
		printf("run_tc_cmd failed!\n");
		return -1;
	}

	return 1;
}

/************************************************************************
 * Multi Queue Priority queue discipline .
 *
 * mqprio allow mapping traffic flows to hardware queue ranges using
 * priorities and a configurable priority to traffic class mapping.
 *
 * Traffic class is a set of contiguous qdisc classess which map 1:1
 * to a set of hardware exposed queues.
 *
 ************************************************************************/
int map_prio(char *iface)
{
	/*
	 * Command to be executed:
	 *
	 * tc qdisc replace dev <iface> parent root handle 100 mqprio num_tc 3
	 * map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2 queues 1@0 1@1 2@2 hw 0
	 */
	char *qdisc_replace_cmd = "tc qdisc replace dev ";
	char *map_prio_cmd =	" parent root handle 100 mqprio num_tc 3"
				" map 2 2 1 0 2 2 2 2 2 2 2 2 2 2 2 2"
				" queues 1@0 1@1 2@2 hw 0";
	char map_prio_cmd_buf[256];
	char *output_buf[256];

	snprintf(map_prio_cmd_buf, sizeof(map_prio_cmd_buf), "%s%s%s",
		 qdisc_replace_cmd, iface, map_prio_cmd);

	if (run_tc_cmd(map_prio_cmd_buf, output_buf) == -1) {
		printf("run_tc_cmd failed!\n");
		return -1;
	}

	return 1;
}
/*
 * Open socket with additional socket options required by
 * CBS and ETF.
 */
int open_socket(struct sockaddr_ll *sock_addr,
		char *interface, uint8_t *dest_addr)
{
	int sockfd;
	struct ifreq if_idx;
	int priority = 3;

	/* Open RAW socket to send packets */
	sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if (sockfd == -1) {
		fprintf(stderr, "Raw socket error.\n");
		return -1;
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, interface, sizeof(if_idx.ifr_name) - 1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
		fprintf(stderr, "Failed to get index of %s.\n", interface);
		goto no_option;
	}

	sock_addr->sll_family = AF_PACKET;
	/* Index of the network device */
	sock_addr->sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	sock_addr->sll_halen = ETH_ALEN;
	/* Destination MAC */
	memcpy((void *)(sock_addr->sll_addr), (void *) dest_addr, ETH_ALEN);

	if (set_qdisc != 0) {
		/*
		 * Setting extra socket option for CBS usage.
		 *
		 * SO_PRIORITY were used by CBS to map priority to
		 * traffic classes / Tx queues.
		 */
		if (setsockopt(sockfd, SOL_SOCKET, SO_PRIORITY, &priority,
		    sizeof(priority))) {
			fprintf(stderr, "Couldn't set priority\n");
			goto no_option;
		}

		/*
		 * Setting extra socket option for ETF usage.
		 *
		 * SO_TXTIME were used by ETF for receiving ther per-packet
		 * timestamp (txtime) as well as the config flags
		 * for each socket.
		 */
		if (use_etf == 1) {
			sk_txtime.clockid = CLOCK_TAI;
			/*
			 * Turn off deadline_mode (bit 0)
			 * Turn off report errors (bit 1)
			 */
			sk_txtime.flags = 0 << 0 | 0 << 1;
			/*
			 * Uncomment line below to receive error
			 * from qdisc
			 */
			/* sk_txtime.flags = SOF_TXTIME_REPORT_ERRORS; */
			if (setsockopt(sockfd, SOL_SOCKET, SO_TXTIME,
			    &sk_txtime, sizeof(sk_txtime))) {
				fprintf(stderr,
					"setsockopt SO_TXTIME failed: %m\n");
				goto no_option;
			}
		}

	}

	return sockfd;

no_option:
	close(sockfd);
	return -1;
}

/*
 * Wrapper function for packet preparation before requesting
 * kernel to send the packet.
 */
int send_message(int sockfd, struct sockaddr_ll sock_addr, void *tx_buf,
		 size_t tx_len, uint64_t txtime)
{
	struct msghdr msg;
	/*
	 * Structure for ETF usage
	 *
	 * launchtime (txtime) were added to the packet via cmsg
	 */
	struct cmsghdr *cmsg;
	struct iovec iov;
	union {
		char buf[CMSG_SPACE(sizeof(uint64_t))];
		struct cmsghdr align;
	} u;

	/*
	 * A struct iovec is a base pointer/length pair
	 * that is one element of a scatter/gather vector;
	 *
	 * we only need a 1-element vector.
	 */
	iov.iov_base = tx_buf;
	iov.iov_len = tx_len;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &sock_addr;
	msg.msg_namelen = sizeof(sock_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/*
	 * adding CMSG to the final msg
	 */
	if (use_etf == 1) {
		msg.msg_control = u.buf;
		msg.msg_controllen = sizeof(u.buf);

		/*
		 * Specify launchtime (txtime) in CMSG
		 */
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SO_TXTIME;
		cmsg->cmsg_len = CMSG_LEN(sizeof(uint64_t));
		*((uint64_t *) CMSG_DATA(cmsg)) = txtime;
	}

	/*
	 * Ask kernel to send the packet.
	 */
	return sendmsg(sockfd, &msg, 0);
}

/*
 * Helper function to print how to use the application.
 */
static void usage(void)
{
	fprintf(stderr, "\n"
		"usage: simple-talker-cmsg [-h] -i interface-name"
		"\n"
		"options:\n"
		"    -C [num] posix clock selection:\n"
		"             0 - TAI (default)\n"
		"             1 - Ethernet adapter's PTP\n"
		"    -h       show this message\n"
		"    -i       specify interface for AVB connection\n"
		"    -t       transport equal to 2 for 1722 or 3 for RTP\n"
		"    -q       set qdisc combination:\n"
		"             -q 0 will disable mqprio, CBS and ETF\n"
		"             -q 1 will enable mqprio and CBS\n"
		"             -q 2 will enable mqprio, CBS and ETF\n"
		"\n%s\n", version_str);
	exit(EXIT_FAILURE);
}


/*
 * Main function for the application.
 */
int main(int argc, char *argv[])
{
	char ptp_devname_buf[PTP_BUFSZ_MAX];
	int clksel = 0, ptpfd = -1;
	clockid_t clkid = CLOCK_TAI;
	struct timespec ts;
	char eth_ptp_path_buf[ETH_PTP_PATH_SZ], *ptpdev_index = NULL;

	int sockfd = -1;
	char send_buf[PKT_BUF_MAX];
	int tx_len = 0;
	struct sockaddr_ll socket_addr;

	unsigned int i;
	struct mrp_talker_ctx *ctx = malloc(sizeof(struct mrp_talker_ctx));
	int c;
	int rc = 0;
	char *interface = NULL;
	uint16_t seqnum;
	uint32_t rtp_timestamp;
	uint64_t time_stamp;
	unsigned int total_samples = 0;
	int32_t sample_buffer[L4_SAMPLES_PER_FRAME * SRC_CHANNELS];

	seventeen22_header *l2_header0;
	six1883_header *l2_header1;
	six1883_sample *sample;

	IP_RTP_Header *l4_headers;
	IP_PseudoHeader pseudo_hdr;
	unsigned int l4_local_address = 0;
	int sd = -1;
	struct sockaddr_in local;
	struct ifreq if_request;

	uint64_t curtime_ns;
	uint64_t prevtxtime_ns;
	uint8_t dest_addr[6];
	size_t packet_size;
	struct mrp_domain_attr *class_a = malloc(sizeof(struct mrp_domain_attr));
	struct mrp_domain_attr *class_b = malloc(sizeof(struct mrp_domain_attr));

	for (;;) {
		c = getopt(argc, argv, "C:hi:t:q:");
		if (c < 0)
			break;
		switch (c) {
		case 'C':
			clksel = atoi(optarg);
			break;
		case 'h':
			usage();
			break;
		case 'i':
			if (interface) {
				printf("only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			if (interface == NULL) {
				printf("No interface was specified\n");
				usage();
			}
			if (strlen(interface) >= IFNAMSIZ) {
				fprintf(stderr, "Interface character is longer"
					" than ifreq.ifr_name\n");
				usage();
			}
			break;
		case 't':
			transport = strtoul(optarg, NULL, 10);
			break;
		case 'q':
			if ((strcmp(optarg,"0") == 0) |
			    (strcmp(optarg,"1") == 0) |
			    (strcmp(optarg,"2") == 0) ) {
				set_qdisc = (int) strtoul(optarg, NULL, 10);
				if (set_qdisc == 2)
					use_etf = 1;
			}
			else{
				set_qdisc = -1;
			}
			break;
		}
	}
	if (optind < argc)
		usage();
	if (interface == NULL)
		usage();

	if (transport < 2 || transport > 4) {
		fprintf(stderr, "Must specify valid transport\n");
		usage();
	}

	if (set_qdisc < 0 || set_qdisc > 2) {
		fprintf(stderr, "Must specify valid qdisc\n");
		usage();
	}

	if (clksel > 1) {
		fprintf(stderr, "posix clock selection outside of range\n");
		usage();
	} else if (clksel) {
		/*
		 * Getting current time from PTP clock
		 */
		DIR *pd;
		struct dirent *pde;

		snprintf(eth_ptp_path_buf, ETH_PTP_PATH_SZ,
			 ETH_PTP_DEV_PATH, interface);
		pd = opendir(eth_ptp_path_buf);
		if (pd != NULL) {
			while ((pde = readdir(pd)) != NULL) {
				if (pde->d_name[0] == 'p' &&
				    pde->d_name[1] == 't') {
					ptpdev_index = pde->d_name;
					fprintf(stdout, "ptpdev_index: %s\n",
						ptpdev_index);
					break;
				}
			}
			if (!ptpdev_index) {
				fprintf(stderr, "Couldn't find Ethernet "
					"adpater's PTP\n");
				closedir(pd);
				goto err_out;
			}
		} else {
			fprintf(stderr, "Couldn't find Ethernet "
				"adapther's PTP\n");
			goto err_out;
		}

		snprintf(ptp_devname_buf, PTP_BUFSZ_MAX, "%s%s",
			 "/dev/", ptpdev_index);
		ptpfd = open(ptp_devname_buf, O_RDWR);
		if (ptpfd < 0) {
			fprintf(stderr, "Failed opening %s: %s\n",
				ptp_devname_buf, strerror(errno));
			closedir(pd);
			goto err_out;
		}

		clkid = get_clockid(ptpfd);
		if (clkid == CLOCK_INVALID) {
			fprintf(stderr, "Failed to get clock id\n");
			closedir(pd);
			close(ptpfd);
			goto err_out;
		}

		closedir(pd);
		close(ptpfd);
	}

	if (ctx == NULL) {
		printf("ctx of mrp_talker_ctx is NULL!\n");
		goto err_out;
	}

	rc = mrp_talker_client_init(ctx);

	halt_tx_sig = &ctx->halt_tx;

	rc = mrp_connect(ctx);
	if (rc) {
		printf("socket creation failed\n");
		goto err_out;
	}

	signal(SIGINT, sigint_handler);
	rc = get_mac_address(interface);
	if (rc) {
		printf("failed to open interface\n");
		usage();
	}
	if (transport == 2) {
		memcpy(dest_addr, glob_l2_dest_addr, sizeof(dest_addr));
	} else {
		/*
		 * Preparing socket to send RDP packets
		 */
		memset(&local, 0, sizeof(local));
		local.sin_family = PF_INET;
		local.sin_addr.s_addr = htonl(INADDR_ANY);
		local.sin_port = htons(L4_PORT);
		l3_to_l2_multicast(dest_addr, glob_l3_dest_addr);
		memset(&if_request, 0, sizeof(if_request));
		strncpy(if_request.ifr_name, interface,
			sizeof(if_request.ifr_name)-1);
		sd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sd == -1) {
			printf("Failed to open socket: %s\n",
			       strerror(errno));
			goto err_out;
		}
		if (bind(sd, (struct sockaddr *) &local,
			sizeof(local)) != 0) {
			printf("Failed to bind on socket: %s\n",
			       strerror(errno));
			goto err_out;
		}
		if (ioctl(sd, SIOCGIFADDR, &if_request) != 0) {
			printf("Failed to get interface "
			       "address (ioctl) on socket: %s\n",
			       strerror(errno));
			goto err_out;
		}
		memcpy(&l4_local_address,
		       &((struct sockaddr_in *)
		       &if_request.ifr_addr)->sin_addr,
		       sizeof(l4_local_address));
	}

	if (class_a == NULL || class_b == NULL) {
		printf("Class a or class b of mrp_domain_attr "
		      "is NULL\n");
		goto err_out;
	}

	rc = mrp_get_domain(ctx, class_a, class_b);
	if (rc) {
		printf("failed calling msp_get_domain()\n");
		goto err_out;
	}
	printf("detected domain Class A PRIO=%d VID=%04x...\n",
	       class_a->priority, class_a->vid);

	rc = mrp_register_domain(class_a, ctx);
	if (rc) {
		printf("mrp_register_domain failed\n");
		goto err_out;
	}

	rc = mrp_join_vlan(class_a, ctx);
	if (rc) {
		printf("mrp_join_vlan failed\n");
		goto err_out;
	}

	/*
	 * Running command to:
	 *     check qdisc (and delete if needed)
	 */
	if (check_qdisc(interface) < 0) {
		fprintf(stderr, "Cannot run command to "
			"delete qdisc. Exiting...\n");
		goto err_out;
	}

	if (set_qdisc == 1 || set_qdisc == 2) {
		/*
		 * Running command to:
		 *     add mqprio and cbs
		 */
		if (map_prio(interface) < 0) {
			fprintf(stderr, "Cannot run command to "
				"map priority. Exiting...\n");
			goto err_out;
		}

		if (set_cbs_qdisc(interface, set_qdisc) < 0) {
			fprintf(stderr, "Cannot run command to "
				"set cbs qdisc. Exiting...\n");
			goto err_out;
		}
	}

	if (set_qdisc == 2) {
		/*
		 * Running command to:
		 *     add ETF
		 */
		if (set_etf_qdisc(interface) < 0) {
			fprintf(stderr, "Cannot run command to "
				"set etf qdisc. Exiting...\n");
			goto err_out;
		}
	}

	/*
	 * Constructing UDP packet
	 */
	memset(glob_stream_id, 0, sizeof(glob_stream_id));
	memcpy(glob_stream_id, glob_station_addr, sizeof(glob_station_addr));

	/* define the packet size */
	if (transport == 2) {
		packet_size = PKT_SZ;
	} else {
		packet_size = 18 + sizeof(*l4_headers) +
			      (L4_SAMPLES_PER_FRAME * CHANNELS * L4_SAMPLE_SIZE);
	}

	seqnum = 0;
	rtp_timestamp = 0; /* Should be random start */

	memset(send_buf, 0, PKT_BUF_MAX);

	/* setting up packet destination address */
	memcpy(send_buf, dest_addr, sizeof(dest_addr));
	/* setting up packet source address */
	memcpy(send_buf + 6, glob_station_addr, sizeof(glob_station_addr));

	/*
	 * Setting up the 802.1Q Tag
	 *
	 * this will set a Virtual LAN tagging on IEEE802.3 Ethernet network.
	 */
	send_buf[12] = 0x81;
	send_buf[13] = 0x00;
	send_buf[14] = ((ctx->domain_class_a_priority << 13 |
			ctx->domain_class_a_vid)) >> 8;
	send_buf[15] = ((ctx->domain_class_a_priority << 13 |
			ctx->domain_class_a_vid)) & 0xFF;

	/*
	 * Setting up the ethertype
	 *
	 * This will determine the protocol used for this packet.
	 */
	if (transport == 2) {
		/* IEEE1722 Ethernet type*/
		send_buf[16] = 0x22;
		send_buf[17] = 0xF0;
	} else {
		/* IP Ethernet type */
		send_buf[16] = 0x08;
		send_buf[17] = 0x00;
	}

	/*
	 * Setting up the payload
	 */
	if (transport == 2) {
		/*
		 * The payload for this packet contains
		 * the packet header for IEEE1722
		 */
		l2_header0 = (seventeen22_header *)(send_buf + 18);
		l2_header0->cd_indicator = 0;
		l2_header0->subtype = 0;
		l2_header0->sid_valid = 1;
		l2_header0->version = 0;
		l2_header0->reset = 0;
		l2_header0->reserved0 = 0;
		l2_header0->gateway_valid = 0;
		l2_header0->reserved1 = 0;
		l2_header0->timestamp_uncertain = 0;
		memset(&(l2_header0->stream_id), 0,
		       sizeof(l2_header0->stream_id));
		memcpy(&(l2_header0->stream_id), glob_station_addr,
		       sizeof(glob_station_addr));
		l2_header0->length = htons(32);
		l2_header1 = (six1883_header *)(l2_header0 + 1);
		l2_header1->format_tag = 1;
		l2_header1->packet_channel = 0x1F;
		l2_header1->packet_tcode = 0xA;
		l2_header1->app_control = 0x0;
		l2_header1->reserved0 = 0;
		l2_header1->source_id = 0x3F;
		l2_header1->data_block_size = 1;
		l2_header1->fraction_number = 0;
		l2_header1->quadlet_padding_count = 0;
		l2_header1->source_packet_header = 0;
		l2_header1->reserved1 = 0;
		l2_header1->eoh = 0x2;
		l2_header1->format_id = 0x10;
		l2_header1->format_dependent_field = 0x02;
		l2_header1->syt = 0xFFFF;
		tx_len = 18 + sizeof(seventeen22_header) +
				sizeof(six1883_header) + (L2_SAMPLES_PER_FRAME *
				CHANNELS * sizeof(six1883_sample));
	} else {
		/*
		 * The payload for this packet contains
		 * the packet header for RDP protocol
		 */
		pseudo_hdr.source = l4_local_address;
		memcpy(&pseudo_hdr.dest, glob_l3_dest_addr,
		       sizeof(pseudo_hdr.dest));
		pseudo_hdr.zero = 0;
		pseudo_hdr.protocol = 0x11;
		pseudo_hdr.length = htons(packet_size-18-20);

		l4_headers = (IP_RTP_Header *)(send_buf + 18);
		l4_headers->version_length = 0x45;
		l4_headers->DSCP_ECN = 0x20;
		l4_headers->ip_length = htons(packet_size-18);
		l4_headers->id = 0;
		l4_headers->fragmentation = 0;
		l4_headers->ttl = 64;
		l4_headers->protocol = 0x11;
		l4_headers->hdr_cksum = 0;
		l4_headers->src = l4_local_address;
		memcpy(&l4_headers->dest, glob_l3_dest_addr,
		       sizeof(l4_headers->dest));
		{
			struct iovec iv0;

			iv0.iov_base = l4_headers;
			iv0.iov_len = 20;
			l4_headers->hdr_cksum = inet_checksum_sg(&iv0, 1);
		}
		l4_headers->source_port = htons(L4_PORT);
		l4_headers->dest_port = htons(L4_PORT);
		l4_headers->udp_length = htons(packet_size-18-20);

		l4_headers->version_cc = 2;
		l4_headers->mark_payload = L16_PAYLOAD_TYPE;
		l4_headers->sequence = 0;
		l4_headers->timestamp = 0;
		l4_headers->ssrc = 0;

		l4_headers->tag[0] = 0xBE;
		l4_headers->tag[1] = 0xDE;
		l4_headers->total_length = htons(2);
		l4_headers->tag_length = (6 << 4) | ID_B_HDR_EXT_ID;

		tx_len = 18 + sizeof(*l4_headers) +
			 (L4_SAMPLES_PER_FRAME * CHANNELS * L4_SAMPLE_SIZE);
	}

	/*
	 * subtract 16 bytes for the MAC header/Q-tag
	 * ==========================================
	 * pktsz is limited to the data payload of the ethernet frame.
	 *
	 * IPG is scaled to the Class (A) observation
	 * interval of packets per 125 usec.
	 */
	fprintf(stderr, "advertising stream ...\n");
	if (transport == 2) {
		rc = mrp_advertise_stream(glob_stream_id, dest_addr,
					  PKT_SZ - 16,
					  L2_PACKET_IPG / 125000,
					  3900, ctx);
	} else {
		/*
		 * 1 is the wrong number for frame rate, but fractional values
		 * not allowed, not sure the significance of the value 6, but
		 * using it consistently
		 */
		rc = mrp_advertise_stream(glob_stream_id, dest_addr,
					  sizeof(*l4_headers) +
					  L4_SAMPLES_PER_FRAME *
					  CHANNELS * 2 + 6,
					  1, 3900, ctx);
	}
	if (rc) {
		printf("mrp_advertise_stream failed\n");
		goto err_out;
	}

	fprintf(stderr, "awaiting a listener ...\n");
	rc = mrp_await_listener(glob_stream_id, ctx);
	if (rc) {
		printf("mrp_await_listener failed\n");
		goto err_out;
	}
	printf("===== Press ENTER to continue =====\n");
	getchar();
	ctx->listeners = 1;
	printf("got a listener ...\n");
	ctx->halt_tx = 0;

	get_samples(L2_SAMPLES_PER_FRAME, sample_buffer);
	/*
	 * Make sure the application were executed on the highest priority.
	 */
	rc = nice(-20);

	sockfd = open_socket(&socket_addr, interface, dest_addr);
	if (sockfd < 0)
		goto err_out;

	/*
	 * Get the current time.
	 *
	 * This will be used by ETF to set the launchtime txtime.
	 */
	clock_gettime(clkid, &ts);
	curtime_ns = (uint64_t)ts.tv_sec * 1000000000UL + (uint64_t)ts.tv_nsec;

	/* We delay the start of demo transmission by XMIT_DELAY */
	prevtxtime_ns = curtime_ns + XMIT_DELAY;

	/*
	 * We round-up the demo transmission to ms granularity for ease
	 * of time measurement readability purpose
	 */
	prevtxtime_ns = prevtxtime_ns - (prevtxtime_ns % 1000000) + 1000000;
	time_stamp = prevtxtime_ns + RENDER_DELAY;

	while (ctx->listeners && !ctx->halt_tx) {
		/*
		 * Here we send 100 packets back-to-back
		 * before the program sleeps for 12.5ms and
		 * then repeats the process.
		 *
		 * This is to simulate general AV
		 * application where Audio Video sampling
		 * data are sent in chunk to reduce sample
		 * reconstruct interference. The sleep is
		 * to ensure we don't overrun the buffer.
		 *
		 * As we want to achieve inter-packet
		 * transmission delta time of 125us,
		 * for 100 packets, we calculate it as
		 * 125us x 100 = 12.5ms.
		 *
		 * SENDMSG_DELAY_REDUCTION is a constant
		 * time value based on experiment so that
		 * in scenario 1 (without CBS + ETF qdics),
		 * we can achieve 8000 packets/s sending rate.
		 *
		 * The sleep operation here is non-deterministic,
		 * so we introduced a correction algorithm to
		 * adjust the sleep time dynamically so that we can
		 * maintain the ideal send-rate of 8000 packets/second.
		 *
		 */
		if (set_qdisc == 0 && packet_count == 100) {
			uint64_t batch_sleep_us;

			packet_count = 0;

			batch_sleep_us = 12500 - SENDMSG_DELAY_REDUCTION -
					 (oversleep_time_ns/1000);
			if (batch_sleep_us < 10000 || batch_sleep_us > 12500)
				printf("batch_sleep_us=%ld\n", batch_sleep_us);

			/*
			 * We check whether we need to slow down every time
			 * we complete a batch of 100 packets.
			 */
			clock_gettime(clkid, &ts);
			before_sleep_time_ns = (uint64_t)ts.tv_sec *
					       1000000000UL +
					       (uint64_t)ts.tv_nsec;

			if (batch_sleep_us > 500) {
				usleep(batch_sleep_us);
				clock_gettime(clkid, &ts);
				after_sleep_time_ns = (uint64_t)ts.tv_sec *
						      1000000000UL +
						      (uint64_t)ts.tv_nsec;
				oversleep_time_ns = (after_sleep_time_ns -
						    before_sleep_time_ns -
						    (batch_sleep_us * 1000)) *
						    1.5;
			} else {
				oversleep_time_ns = 0;
			}
		}

		if (transport == 2) {
			/*
			 * adding IEC61883 packet header,
			 * inside IEEE1722 packet.
			 *
			 * The payload for IEC61883 is the audio sample.
			 */
			uint32_t timestamp_l;

			l2_header0 = (seventeen22_header *)(send_buf + 18);
			l2_header1 = (six1883_header *)(l2_header0 + 1);

			/*
			 * Recalculate the launchtime txtime to be
			 * after the last launchtime plus a constant.
			 */

			/*
			 * Unfortuntely unless this thread is at rtprio
			 * you get pre-empted between fetching the time
			 * and programming the packet and get a late packet
			 */
			txtime = prevtxtime_ns + L2_PACKET_IPG;
			prevtxtime_ns += L2_PACKET_IPG;

			/*
			 * Adding Sequence number to the payload.
			 */
			l2_header0->seq_number = seqnum++;

			if (seqnum % 4 == 0)
				l2_header0->timestamp_valid = 0;
			else
				l2_header0->timestamp_valid = 1;

			/*
			 * Add timestamp to the packet.
			 */
			timestamp_l = time_stamp;
			l2_header0->timestamp = htonl(timestamp_l);
			time_stamp += L2_PACKET_IPG;
			/*
			 * Write the audio sample to the packet payload.
			 */
			l2_header1->data_block_continuity = total_samples;
			total_samples += L2_SAMPLES_PER_FRAME*CHANNELS;
			sample = (six1883_sample *)(send_buf +
					(18 + sizeof(seventeen22_header) +
					sizeof(six1883_header)));

			for (i = 0; i < L2_SAMPLES_PER_FRAME * CHANNELS; ++i) {
				uint32_t tmp = htonl(sample_buffer[i]);

				sample[i].label = 0x40;
				memcpy(&(sample[i].value), &(tmp),
				       sizeof(sample[i].value));
			}
		} else {
			/*
			 * Finalize RDP packet.
			 */
			uint16_t *l16_sample;
			uint8_t *tmp;

			get_samples(L4_SAMPLES_PER_FRAME, sample_buffer);

			l4_headers = (IP_RTP_Header *)(send_buf + 18);

			l4_headers->sequence =  seqnum++;
			l4_headers->timestamp = rtp_timestamp;

			txtime = prevtxtime_ns + L4_PACKET_IPG;
			prevtxtime_ns += L4_PACKET_IPG;

			l4_headers->nanoseconds = time_stamp/1000000000;
			tmp = (uint8_t *) &l4_headers->nanoseconds;
			l4_headers->seconds[0] = tmp[2];
			l4_headers->seconds[1] = tmp[1];
			l4_headers->seconds[2] = tmp[0];
			{
				uint64_t tmp;

				tmp  = time_stamp % 1000000000;
				tmp *= RTP_SUBNS_SCALE_NUM;
				tmp /= RTP_SUBNS_SCALE_DEN;
				l4_headers->nanoseconds = (uint32_t) tmp;
			}
			l4_headers->nanoseconds =
				htons(l4_headers->nanoseconds);

			time_stamp += L4_PACKET_IPG;

			l16_sample = (uint16_t *)(l4_headers+1);

			for (i = 0; i < L4_SAMPLES_PER_FRAME * CHANNELS; ++i) {
				uint16_t tmp = sample_buffer[i]/65536;

				l16_sample[i] = htons(tmp);
			}
			l4_headers->cksum = 0;
			{
				struct iovec iv[2];

				iv[0].iov_base = &pseudo_hdr;
				iv[0].iov_len = sizeof(pseudo_hdr);
				iv[1].iov_base = ((uint8_t *)l4_headers) + 20;
				iv[1].iov_len = packet_size-18-20;
				l4_headers->cksum = inet_checksum_sg(iv, 2);
			}
		}

		/*
		 * Send the prepared packet to support function
		 * send_message.
		 *
		 * this function receive txtime, which will be use by
		 * CBS.
		 */
		if (send_message(sockfd, socket_addr, send_buf,
		    tx_len, txtime) < 1) {
			fprintf(stderr, "sendmsg failed: %m\n");
			break;
		}
		++packet_count;
	}
	rc = nice(0);
	if (ctx->halt_tx == 0)
		printf("listener left ...\n");
	ctx->halt_tx = 1;
	if (transport == 2) {
		rc = mrp_unadvertise_stream(glob_stream_id, dest_addr,
					    PKT_SZ - 16,
					    L2_PACKET_IPG / 125000,
					    3900, ctx);
	} else {
		rc = mrp_unadvertise_stream(glob_stream_id, dest_addr,
					    sizeof(*l4_headers) +
					    L4_SAMPLES_PER_FRAME *
					    CHANNELS*2 + 6, 1,
					    3900, ctx);
	}
	if (rc)
		printf("mrp_unadvertise_stream failed\n");

	rc = mrp_disconnect(ctx);
	if (rc)
		printf("mrp_disconnect failed\n");

	if (sockfd != -1)
		close(sockfd);
	if (sd != -1)
		close(sd);

	if (ctx)
		free(ctx);
	if (interface)
		free(interface);
	if (class_a)
		free(class_a);
	if (class_b)
		free(class_b);
	pthread_exit(NULL);

	return EXIT_SUCCESS;

err_out:
	if (sd != -1)
		close(sd);

	if (ctx)
		free(ctx);
	if (interface)
		free(interface);
	if (class_a)
		free(class_a);
	if (class_b)
		free(class_b);

	return EXIT_FAILURE;
}
