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
/* pthread functionality */
#define _GNU_SOURCE
#include <dirent.h>

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
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <search.h>
#include <linux/errqueue.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if_arp.h>

/* Message queues for displaying Rx packet information */
#include <sys/msg.h>

/* For selection */
#define DEBUG 0		/* Print extra information */
#define UNICAST 1	/* Select Unicast or Multicast */
/* Temporary */
#define OUTPUT_FILE 1	/* Packet information to be written in a file */
#define CHECK_TIME 0	/* Check time taken for receiving packet process */
/******************/

#define FNAME_GRAPH "data_graph.dat"	/* Packet latency information */

/* #define CUSTOM_DATA */
#define VERSION_STR "1.0"	/* Application version number */

#define FLUSH_COUNT 100		/* Count before graph data file being flushed */

/* Message Queue */
#define MSG_KEY_IO 8899			 /* Message key for IO display */
#define MSG_KEY_GRAPH 2299		 /* Message key for graph display */
#define MSGQ_TYPE 1			 /* Message queue type */
#define MESSAGE_QUEUE_SIZE ((1024)*(64)) /* Message queue size */

/* Rx packet sizes */
#define PKT_SZ_RX (2048)      /* Size of iov buffer */
#define RX_PKT_CTRL_SZ (1024) /* Size of control buffer for RX timestamp */

/* Packet format */
#define TYPE_TSN 0x10	     /* TSN type in hex to insert to outgoing packets */

/* Header size */
#define IP_HEADER_SIZE (20)  /* Size of IP header */
#define UDP_HEADER_SIZE (8)  /* Size of UDP header */
#define RAW_PACKET_SIZE (64) /* Size of raw buffer for TSN packet */

#define MAX_LINE_LEN (256) /* Maximum line for window config file */

#define ONE_SEC 1000000000ULL /* One sec in nsec */

#define pr_err(s)	fprintf(stderr, s "\n") /* Error message output */
#define pr_info(s)	fprintf(stdout, s "\n") /* Standard message output */

/* globals */
static const char *version_str = "sample-app-taprio v" VERSION_STR "\n"
				 "Copyright (c) 2019, Intel Corporation\n";

volatile int halt_tx_sig; /* Global variable for signal handler */
FILE *fp;
FILE *fgp;
FILE *flat;

/* Enums for mode options (TX - sender / RX - receiver) */
enum {
	SEL_TX_TSN = 0,
	SEL_TX_ALL = 1,
};

enum {
	SEL_TSN = 1,
};

enum {
	SEL_TX_ONLY = 1,
	SEL_RX_ONLY = 2,
	SEL_TX_RX = 3
};

enum {
	SEL_IO_ONLY = 1,
	SEL_GRAPH_ONLY = 2,
	SEL_IO_GRAPH = 3
};

enum {
	SEL_OFF = 0,
	SEL_ON = 1
};

/* Structures */

/* TSN RX packet data information */
typedef struct __attribute__ ((packed)) {
	uint32_t seq;
	uint32_t pktType;
	uint64_t timestamp;
} TSN_data;

/* Packet data to be visualized on graph */
typedef struct __attribute__ ((packed)) {
	uint32_t seq;
	uint64_t latency;
	uint32_t packet_loss;
	uint32_t pktType;
	uint64_t rcv_timestamp;
	uint64_t sender_timestamp;
	uint64_t interpkt_delta;
} QBV_demo_data;

/* Message buffer that contains packet data */
typedef struct __attribute__ ((packed)) {
	long message_type;
	QBV_demo_data data;
} msg_buff;

/* Packet loss information */
typedef struct {
	uint8_t first_time_tsn;
	uint32_t packet_loss_tsn;
	uint32_t seq_count_tsn;
	uint32_t prev_seq_count_tsn;
} tsn_packet_loss_info;

/* Packet cached information */
typedef struct {
	uint64_t prev_rxtime;
} tsn_pkt_cached_data;

/****************/

/* TX packet header information */
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

	/* UDP data section */
	TSN_data data;
#ifdef CUSTOM_DATA
	uint8_t data[59];
#endif
} IP_UDP_QBVdemo_Header;

/* IP header information  */
typedef struct __attribute__ ((packed)) {
	uint32_t source;
	uint32_t dest;
	uint8_t zero;
	uint8_t protocol;
	uint16_t length;
} IP_PseudoHeader;

/*****************/

struct myqelem {
	struct myqelem *q_forw;
	struct myqelem *q_back;
	void *q_data;
};

/* Data structures to be used in processTx thread */
typedef struct {
	int fdTSN;
	clockid_t clkid;
	uint16_t udp_dest;
} socket_fd_ptrs;

typedef struct {
	uint64_t offset;
	uint64_t duration;
	uint8_t num_packets;
} tx_window;


#if __GNUC__ < 9
/*
 * The API for SO_TXTIME is the below struct and enum, which is
 * provided by uapi/linux/net_tstamp.h in modern systems.
 */

struct sock_txtime {
	clockid_t clockid;
	uint16_t flags;
};

enum txtime_flags {
	SOF_TXTIME_DEADLINE_MODE = (1 << 0),
	SOF_TXTIME_REPORT_ERRORS = (1 << 1),

	SOF_TXTIME_FLAGS_LAST = SOF_TXTIME_REPORT_ERRORS,
	SOF_TXTIME_FLAGS_MASK = (SOF_TXTIME_FLAGS_LAST - 1) |
				 SOF_TXTIME_FLAGS_LAST
};
#endif

/********** globals *************/

/* Variables to store Tx data */
IP_UDP_QBVdemo_Header *l4_headers;
IP_PseudoHeader pseudo_hdr;
char rawpktbuf[RAW_PACKET_SIZE];
static unsigned char tx_buffer[16];
int raw_index;
static struct in_addr mcast_addr;
size_t packet_size;
uint32_t seqNum = 0;
uint32_t pktType = 0;

/* Message queue variables */
int msqid;
int msqgraph;
msg_buff data_rcv;

/* Window default values */
#define MAX_WINDOWS_PER_CYCLE (16) /* Max number of windows per cycle */
#define PACKETS_PER_WINDOW_DEFAULT (1) /* Number of packets per window */
#define NUM_WINDOW_PER_CYCLE_DEFAULT (2) /* Number of window in one cycle */
#define CYCLE_TIME_DEFAULT (1000000)		/* TSN cycle time */
#define CYCLE_OFFSET (5) /* Base time if no base time provided */
#define TX_DELAY_DEFAULT (1000000) /* Delay before Launchtime */
#define TX_WINDOW_OFFSET (50000) /* Offset delay in window before Launchtime */

/* TSN packet default values */
#define VLAN_PRIO_TSN_DEFAULT ((uint16_t)5)	/* TSN packet VLAN priority */
#define MAX_NUM_VLAN_PRIO (8)			/* Max number of VLAN priorities */
#define VID_TSN_DEFAULT ((uint16_t)3)		/* TSN VLAN ID */
#define UDP_DST_PORT_DEFAULT ((uint16_t)4800)	/* Destination UDP port */
#define UDP_SRC_PORT_DEFAULT ((uint16_t)4800)	/* Source UDP port */

/* CPU thread default values */
#define CPU_NUM_DEFAULT (1)			 /* Run on CPU second core */
#define PTHREAD_PRIORITY_DEFAULT ((uint16_t)90)	 /* pthread priority default */

/* Mode and packet default parameters */
char *default_file = "zrx.log";
uint8_t selRxType = SEL_TSN;
uint8_t selTxType = SEL_TX_ALL;
uint8_t rxtxType = SEL_TX_RX;
uint16_t vid_TSN = VID_TSN_DEFAULT;

/* List of TSN VLAN priorities to display on RX */
int priority_filter_list[MAX_NUM_VLAN_PRIO];
uint16_t vpriority_TSN = VLAN_PRIO_TSN_DEFAULT;

/* Packet opening window default parameters */
uint64_t cycle_time = CYCLE_TIME_DEFAULT;
uint8_t num_packet_per_window = PACKETS_PER_WINDOW_DEFAULT;
uint8_t num_window_per_cycle = NUM_WINDOW_PER_CYCLE_DEFAULT;

/* Launchtime default variables */
static tx_window packet_windows[MAX_WINDOWS_PER_CYCLE];
static int receive_errors = 0;
static int tx_window_offset = TX_WINDOW_OFFSET;
static int use_so_txtime = 1;
static int use_deadline_mode = 0;
static int waketx_delay = TX_DELAY_DEFAULT;
static struct sock_txtime sk_txtime;

/* Timer variables for window opening */
static timer_t window_timer_id;
static int send_packet_now = 0;
static int close_window = 0;
static uint64_t base_time = 0;

/* Default IP Addresses */
unsigned char glob_l3_dest_addr[] = { 192, 168, 0, 2 };

/* Addresses */
unsigned char glob_l3_src_addr[] = { 0, 0, 0, 0 };
unsigned char glob_l2_src_addr[] = { 0, 0, 0, 0, 0, 0 };
/* For Tx */
unsigned char glob_l2_dest_addr[] = { 0, 0, 0, 0, 0, 0 };

#ifdef CUSTOM_DATA
static const unsigned char udp_data[59] = {
	0xdd, 0x81, /* .. */
	0x0d, 0xd8, 0xe6, 0x96, 0x60, 0x45, 0x85, 0x0e, /* ....`E.. */
	0x15, 0x4f, 0x91, 0x81, 0x30, 0x7c, 0x00, 0x6d, /* .O..0|.m */
	0x00, 0x01, 0x01, 0x00, 0x25, 0x00, 0x8d, 0x2e, /* ....%... */
	0x01, 0x00, 0x00, 0x00, 0x32, 0xbe, 0xc6, 0x51, /* ....2..Q */
	0xfd, 0xc8, 0xd0, 0x01, 0x03, 0x00, 0x07, 0x00, /* ........ */
	0x00, 0x00, 0x00, 0x07, 0x8d, 0x2e, 0x00, 0x00, /* ........ */
	0x09, 0xe5, 0xcb, 0x38, 0xa5, 0xe8, 0x07, 0xf5, /* ...8.... */
	0x13
};
#endif

/* For display printf */
uint8_t txDisplay = SEL_OFF;
uint8_t rxDisplay = SEL_GRAPH_ONLY;

/* RX timestamping definitions */
#ifdef NO_KERNEL_TS_INCLUDE
#include <time.h>
enum {
	SOF_TIMESTAMPING_TX_HARDWARE =  (1<<0),
	SOF_TIMESTAMPING_TX_SOFTWARE =  (1<<1),
	SOF_TIMESTAMPING_RX_HARDWARE =  (1<<2),
	SOF_TIMESTAMPING_RX_SOFTWARE =	(1<<3),
	SOF_TIMESTAMPING_SOFTWARE =	(1<<4),
	SOF_TIMESTAMPING_SYS_HARDWARE = (1<<5),
	SOF_TIMESTAMPING_RAW_HARDWARE = (1<<6),
	SOF_TIMESTAMPING_OPT_ID =	(1<<7),
	SOF_TIMESTAMPING_OPT_TSONLY =	(1<<11),

	SOF_TIMESTAMPING_MASK =
	(SOF_TIMESTAMPING_RAW_HARDWARE - 1) |
	SOF_TIMESTAMPING_RAW_HARDWARE
};
#else
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#endif

/* Launchtime API definitions */
#ifndef SO_TXTIME
#define SO_TXTIME (61)
#define SCM_TXTIME (SO_TXTIME)
#endif

#ifndef SO_EE_ORIGIN_TXTIME
#define SO_EE_ORIGIN_TXTIME (6)
#define SO_EE_CODE_TXTIME_INVALID_PARAM	(1)
#define SO_EE_CODE_TXTIME_MISSED (2)
#endif

/********** Function prototype **********/
void *processIO(void *arg);

/* functions */
static void usage(void)
{
	fprintf(stderr, "\n"
		"\n"
		"Usage: sample-app-taprio [-h] -i <interface-name> "
		"-c <ip-address> -x <mode> -w <config-file> [options]\n"
		"E.g. (TX mode): sample-app-taprio -i enp2s0 -c 192.168.0.2 "
		"-x 1 -w tsn_prio5.cfg\n"
		"E.g. (RX mode): sample-app-taprio -i enp2s0 -x 2 -y 3 -q '5 3'"
		"\n\n"
		"options:\n"
		"    -A  Set CPU affinity\n"
		"    -b  Base time to start the TSN cycle in nanoseconds\n"
		"    -B  Base time filename generated by scheduler.py\n"
		"    -c  Client IP address\n"
		"    -d  Turn Tx print display. On = 1, Off = 0 (Default)\n"
		"    -D	 Set deadline mode for SO_TXTIME\n"
		"    -e	 Set TX window offset\n"
		"    -E	 Enable error reporting on the socket error queue "
			 "for SO_TXTIME\n"
		"    -f  Set the name of the output for logging\n"
		"    -h  Show this message\n"
		"    -i  Specify interface for AVB connection\n"
		"    -n  UDP destination port\n"
		"    -o  UDP source port\n"
		"    -p  TSN priority (Default - Priority 5)\n"
		"    -P  Set Realtime priority\n"
		"    -q  Select one or multiple TSN Priority to be displayed\n"
		"    -S	 Do not use SO_TXTIME\n"
		"    -t  TSN cycle time in nanoseconds (1000000ns minimum)\n"
		"    -v  VLAN ID for TSN\n"
		"    -w  Config file name for window configuration "
			 "(Default -tsn_prio5.config)\n"
		"    -x  Select Rx and/or Tx. 1 = Tx only, 2 = Rx only, "
			 "3 = Tx & Rx (Default)\n"
		"    -y  Turn Rx I/O process display and logging and/or graph. "
			 "1 = I/O only, 2 = Graph only (Default), "
			 "3 = I/O & Graph\n"
		"	 -z	 Delta from wake up to txtime in nanoseconds "
			 "(default 1ms)\n"
		"\n%s\n", version_str);
	exit(EXIT_FAILURE);
}

/**
 * run_ethtool_cmd - Run ethtool command from code
 *
 * @cmd: Command as string
 */
static int run_ethtool_cmd(char *cmd)
{
	fp = popen(cmd, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to run command %s\n. Exiting...", cmd);
		return -1;
	}
	pclose(fp);
	return 1;
}

/**
 * run_check_filter_cmd - Run command to retrieve available filters
 *
 * @cmd: Command as string
 * @buf: Buffer to store output upon running the command
 */
int run_check_filter_cmd(char *cmd, char **buf)
{
	char line[256];
	size_t line_count = 0;

	fp = popen(cmd, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to run command %s\n. Exiting...", cmd);
		return -1;
	}

	while (fgets(line, sizeof(line)-1, fp) != NULL) {
		buf[line_count] = strdup(line);
		++line_count;
	}
	pclose(fp);
	free(fp);
	fp = NULL;

	return line_count;
}

/**
 * delete_filter - Deletes filters based on their rules
 * e.g : "ethtool -N eth0 delete 15"
 *
 * @iface: Interface name
 * @filters: List of filters available
 * @size: Size of filter buffer
 */
int delete_filter(char *iface, char **filters, int size)
{
	char delete_filter_cmd_buf[256];
	char *filter_cmd = "ethtool -N ";
	char *delete_filter_cmd = " delete ";

	for (int count = 0; count < size; count++) {
		snprintf(delete_filter_cmd_buf, sizeof(delete_filter_cmd_buf),
			 "%s%s%s%s", filter_cmd, iface, delete_filter_cmd,
			 filters[count]);
		if (run_ethtool_cmd(delete_filter_cmd_buf) < 0)
			return -1;
	}

	return 1;
}

/**
 * check_filter - Checks for existing filter rules
 * e.g: "ethtool -n eth0 | grep Filter | awk -F': ' \
 *	 '{print $2}'"
 *
 * @iface: Interface name
 */
int check_filter(char *iface)
{
	char show_filter_ID_cmd_buf[256];
	char *output_buf[20];
	int size = 0;
	char *filter_show_cmd = "ethtool -n ";
	char *get_filter_ID_cmd = " | grep Filter | awk -F': ' '{print $2}'";

	snprintf(show_filter_ID_cmd_buf, sizeof(show_filter_ID_cmd_buf),
		 "%s%s%s", filter_show_cmd, iface, get_filter_ID_cmd);

	/*
	 * Runs the command to show existing filters and
	 * Returns the number of filters to the variable size
	 */
	size = run_check_filter_cmd(show_filter_ID_cmd_buf, output_buf);

	if (size < 0) {
		pr_err("Something is wrong. Exiting...\n");
		return -1;
	}

	if (output_buf[0] == NULL) {
		pr_info("No filters available\n");
		return 1;
	}

	/* Sends the list of filters to be deleted */
	if (output_buf != NULL) {
#if DEBUG
		pr_info("Filters exists. Deleting them...\n");
#endif
		if (delete_filter(iface, output_buf, size) < 0)
			return -1;
	}

	return 1;
}

/**
 * create_filter - Run the command to create filters
 * e.g : "ethtool -N eth0 flow-type ether vlan 0x0003
 *	  vlan-mask 0x1fff action 2"
 *
 * @iface: Interface name
 * @prio_list: List of alll vlan priorities
 */
int create_filter(char *iface)
{
	char prio_buf[20];
	char create_filter_cmd_buf[256];
	uint32_t vprio;
	char *create_filter_cmd = "ethtool -N ";
	char *filter_part_2 = " flow-type ether vlan ";
	char *vlan_mask_cmd = " vlan-mask 0x1fff action ";
	char *str_head = "0x";
	int prio_list[] = {0, 1, 2, 3, 4, 5, 6, 7};
	int hw_queue = 0;

	/* Create RX filters */
	for (int count = 0; count < MAX_NUM_VLAN_PRIO; count++) {
		vprio = (prio_list[count] << 13 | vid_TSN);
		snprintf(prio_buf, sizeof(prio_buf), "%s%04x", str_head, vprio);

		if (prio_list[count] == 0 || prio_list[count] == 1)
			hw_queue = 3;
		else if (prio_list[count] == 2 || prio_list[count] == 3)
			hw_queue = 2;
		else if (prio_list[count] == 4 || prio_list[count] == 5)
			hw_queue = 1;
		else if (prio_list[count] == 6 || prio_list[count] == 7)
			hw_queue = 0;

		snprintf(create_filter_cmd_buf,
			 sizeof(create_filter_cmd_buf), "%s%s%s%s%s%d",
			 create_filter_cmd, iface, filter_part_2, prio_buf,
			 vlan_mask_cmd, hw_queue);
		if (run_ethtool_cmd(create_filter_cmd_buf) < 0)
			return -1;
	}
	return 1;
}

/**
 * config_rx_filter - Steer incoming packets to its queue
 *
 * @interface: Network interface name
 *
 * Incoming packets steering configuration
 *	Queue 0: Priority 6 and 7
 *	Queue 1: Priority 4 and 5
 *	Queue 2: Priority 2 and 3
 *	Queue 3: Priority 0 and 1
 */
static int config_rx_filter(char *interface)
{
	if (check_filter(interface) < 0) {
		pr_err("Cannot run command to delete filters. Exiting...\n");
		return -1;
	}
	if (create_filter(interface) < 0) {
		pr_err("Cannot run command to create filters. Exiting...\n");
		return -1;
	}
	return 1;
}

/**
 * check_packet_windows - Check packet window information are properly initialized
 */
static int check_packet_windows(void)
{
	for (int i = 0; i < num_window_per_cycle; ++i) {
		if (packet_windows[i].offset == 0 || packet_windows[i].duration == 0) {
			pr_err("Error in window config");
			return -1;
		}
		if (packet_windows[i].num_packets == 0)
			packet_windows[i].num_packets = num_packet_per_window;
	}
	return 1;
}

/**
 * parse_tokens - Extract details from window config file
 *
 * @line: One line from the config file
 */
static int parse_tokens(char *line)
{
	char *win_token;
	char *tmp = NULL;
	char *token = strtok(line, " \t");
	int win_index = 0;

	while (token != NULL) {
		if (strcmp(token, "#") == 0 || strcmp(token, "\n") == 0)
			break;
		else if (strcmp(token, "cycle_time") == 0) {
			token = strtok(NULL, " \t");
				if (token == NULL)
					goto err_out;
			cycle_time = strtol(token, NULL, 10);
			break;
		} else if (strcmp(token, "priority") == 0) {
			token = strtok(NULL, " \t");
				if (token == NULL)
					goto err_out;
			vpriority_TSN = strtol(token, NULL, 10);
			break;
		} else if (strcmp(token, "number_of_windows") == 0) {
			token = strtok(NULL, " \t");
				if (token == NULL)
					goto err_out;
			num_window_per_cycle = strtol(token, NULL, 10);
			if (num_window_per_cycle > MAX_WINDOWS_PER_CYCLE) {
				pr_err("Window config error: number of windows "
				       "exceed max allowed");
				goto err_out;
			}
			break;
		} else {
			tmp = (char *) malloc(strlen(token) * sizeof(char));
			if (tmp == NULL)
				goto err_out;

			strcpy(tmp, token);
			token = strtok(NULL, " \t");
			if (token == NULL)
				goto err_out;

			win_token = strtok(tmp, "_");
			if (win_token == NULL)
				goto err_out;
			if (strcmp(win_token, "window") == 0) {
				win_token = strtok(NULL, "_");
				if (win_token == NULL)
					goto err_out;
				win_index = strtol(win_token, NULL, 10);
				if (win_index > num_window_per_cycle) {
					pr_err("Window config error: index is "
					       "larger than the number of "
					       "window specified");
					goto err_out;
				}

				win_token = strtok(NULL, "_");
				if (win_token == NULL)
					goto err_out;
				if (strcmp(win_token, "offset") == 0) {
					int off = strtol(token, NULL, 10);

					if (off > cycle_time) {
						pr_err("Window config error: "
						       "offset cannot be larger "
						       "than cycle time");
						goto err_out;
					}
					packet_windows[win_index - 1].offset = off;
				} else if (strcmp(win_token, "duration") == 0)
					packet_windows[win_index - 1].duration =
								   strtol(token,
									  NULL,
									  10);
				else if (strcmp(win_token, "packets") == 0)
					packet_windows[win_index - 1].num_packets =
								   strtol(token,
									  NULL,
									  10);
				else {
					pr_err("Window config error: invalid "
					       "token");
					goto err_out;
				}
			}
			break;
		}
	}
	if (tmp)
		free(tmp);
	return 1;
err_out:
	if (tmp)
		free(tmp);
	return -1;
}

/**
 * parse_config - Parse all window information
 *
 * @filename: File name of the config
 */
static int parse_config(char *filename)
{
	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open config file %s\n!", filename);
		return -1;
	}

	char line[MAX_LINE_LEN];

	while (fgets(line, sizeof(line), fp))
		if (parse_tokens(line) < 0)
			return -1;

	fclose(fp);
	fp = NULL;
	/* Check packet windows structure are properly initialized */
	return check_packet_windows();
}

/**
 * convert_input_IP - Convert, validate and decode string input IP address
 *
 * @input: String input of IP address
 * @output: Stores IP address in unsigned char array format.
 */
static int convert_input_IP(char *input, unsigned char *output)
{
	char convert[4];
	int status = -1;
	int progress = 0;
	uint8_t count = 0;
	uint8_t j = 0;

	if (input == NULL)
		return status;

	if (output == NULL)
		return status;

	while ((*input != '\0') || (progress == 1)) {
		if ((*input == '.') || (*input == '\0')) {
			if (count >= 4)
				break;

			convert[count] = '\0';

			/* Convert & copy data */
			output[j++] = atoi(convert);

			if (*input != '\0')
				input++;

			count = 0;
			progress = 0;
		} else {
			if (count >= 4)
				break;
			progress = 1;
			convert[count++] = *input++;
		}
	}

	/* Check complete */
	if (j == 4)
		status = 0;

	return status;
}

/**
 * sigint_handler - Catching signal interrupt
 *
 * @signum: none
 */
void sigint_handler(int signum)
{
	printf("got SIGINT\n");
	halt_tx_sig = signum;
}

/****************************
 *	Initialization
 ****************************/
#if !UNICAST
/**
 * l3_to_l2_multicast - Convert multicast IP address to multicast MAC
 *						address
 *
 * @l2: Multicast MAC address stored here
 * @l3: Passing multicast IP address
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
#endif

/**
 * get_mac_address - Get MAC address from the specified interface
 *
 * @interface: Network Interface name
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

	memcpy(glob_l2_src_addr, if_request.ifr_hwaddr.sa_data,
	       sizeof(glob_l2_src_addr));
	close(lsock);
	return 0;
}

#if DEBUG
/**
 * print_ip_address - Prints IP address on the terminal
 *
 * @interface: Network Interface name
 */
int print_ip_address(char *interface)
{
	struct ifreq if_request;
	int rc;
	int lsock;

	lsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (lsock < 0)
		return -1;

	/* IPv4 IP address type */
	memset(&if_request, 0, sizeof(if_request));
	if_request.ifr_addr.sa_family = AF_INET;
	strncpy(if_request.ifr_name, interface, IFNAMSIZ - 1);

	/* Get the ip address */
	rc = ioctl(lsock, SIOCGIFADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	printf("IP address of %s: %s\n", interface,
	       inet_ntoa(((struct sockaddr_in *)&if_request.ifr_addr)->sin_addr));

	close(lsock);
	return 0;
}
#endif

/**
 * get_remote_mac_address - Get MAC address of another network device
 *				by using IP address
 *
 * @interface: Network Interface name
 * @l2: MAC address obtained is stored here
 * @l3: The remote IP address
 */
int get_remote_mac_address(char *interface, unsigned char *l2,
			   unsigned char *l3)
{
	int lsock;
	struct arpreq arprequest;
	struct sockaddr_in *sin;
	unsigned char *eap = NULL;
	unsigned char *ip = NULL;
	unsigned int i = 0;

	memset(&arprequest, 0, sizeof(arprequest));
	strncpy(arprequest.arp_dev, interface, sizeof(arprequest.arp_dev) - 1);

	sin = (struct sockaddr_in *) &arprequest.arp_pa;
	sin->sin_family = AF_INET;

	/* Copy IP address from unsigned char array to in_addr_t (uint32) */
	ip = (unsigned char *)&sin->sin_addr.s_addr;
	for (i = 0; i < 4 ; i++)
		ip[i] = l3[i];

#if DEBUG
	printf("get_remote_mac_address() input IP address: %s\n",
	       inet_ntoa(sin->sin_addr));
#endif

	lsock = socket(AF_INET, SOCK_DGRAM, 0);
	if (lsock < 0) {
		printf("socket failed - (%s) Error\n", strerror(errno));
		return errno;
	}

	if (ioctl(lsock, SIOCGARP, &arprequest) < 0) {
		printf("ioctl failed - (%s) Error\n", strerror(errno));
		goto no_api;
	}

	if (arprequest.arp_flags & ATF_COM) {
		eap = (unsigned char *) &arprequest.arp_ha.sa_data[0];
		/* Copy destination MAC address */
		for (i = 0; i < 6; i++)
			l2[i] = eap[i];

		printf("Dest MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
		       l2[0], l2[1], l2[2], l2[3], l2[4], l2[5]);
		printf("\n");
	} else {
		printf("get_remote_mac_address() error.\n");
		goto no_api;
	}

	close(lsock);

	return 0;
no_api:
	close(lsock);
	return -1;
}

/**
 * find_TSN_prio - Lookup TSN priority in filtering list
 *
 * @a[] - List of TSN priorities to list
 * @num_elements - Number of priorities
 * @value - TSN priority to lookup
 */
int find_TSN_prio(int a[], int num_elements, uint16_t value)
{
	for (int i = 0; i < num_elements; i++)
		if (a[i] == value)
			return(value);  /* Found */

	return -1;  /* Not found */
}

/**
 * extract_timestamp - Given a packet msg, extract the timestamp(s)
 *
 * @msg: msghdr structure
 */
static uint64_t extract_timestamp(struct msghdr *msg)
{
	struct cmsghdr *cmsg;
	struct timespec *ts = NULL;
	uint64_t ts_int = 0;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET)
			continue;

		switch (cmsg->cmsg_type) {
		case SO_TIMESTAMPING:
			ts = (struct timespec *) CMSG_DATA(cmsg);
			break;
		default:
			/* Ignore other cmsg options */
			break;
		}
	}
	if (ts != NULL)
		ts_int = ts[2].tv_sec * 1000000000ULL + ts[2].tv_nsec;

	return ts_int;
}

/**
 * init_rx_socket - Initialize receive socket
 *
 * @etype: socket protocol type
 * @sock: File descriptor
 * @mac_addr - Receiver MAC address
 * @interface: Network Interface name
 */
int init_rx_socket(u_int16_t etype, int *sock, unsigned char *mac_addr,
		   char *interface)
{
	struct sockaddr_ll addr;
	struct ifreq if_request;
	int lsock;
	int rc;
	struct packet_mreq multicast_req;

	if (sock == NULL)
		return -1;
	if (mac_addr == NULL)
		return -1;

	memset(&multicast_req, 0, sizeof(multicast_req));
	*sock = -1;

	lsock = socket(PF_PACKET, SOCK_RAW, htons(etype));
	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));

	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name) - 1);

	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memset(&if_request, 0, sizeof(if_request));

	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name) - 1);

	rc = ioctl(lsock, SIOCGIFINDEX, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = if_request.ifr_ifindex;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(etype);

	rc = bind(lsock, (struct sockaddr *)&addr, sizeof(addr));
	if (rc != 0) {
#if LOG_ERRORS
		fprintf(stderr, "%s - Error on bind %s",
			__func__, strerror(errno));
#endif
		close(lsock);
		return -1;
	}

	rc = setsockopt(lsock, SOL_SOCKET, SO_BINDTODEVICE, interface,
			strlen(interface));
	if (rc != 0) {
		close(lsock);
		return -1;
	}

	multicast_req.mr_ifindex = if_request.ifr_ifindex;
	multicast_req.mr_type = PACKET_MR_MULTICAST;
	multicast_req.mr_alen = 6;
	memcpy(multicast_req.mr_address, mac_addr, 6);

	rc = setsockopt(lsock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			&multicast_req, sizeof(multicast_req));
	if (rc != 0) {
		close(lsock);
		return -1;
	}

	/* Use hardware timestamping for this socket */
	int enabled = SOF_TIMESTAMPING_RX_HARDWARE |
		      SOF_TIMESTAMPING_RAW_HARDWARE |
		      SOF_TIMESTAMPING_SYS_HARDWARE;
	rc = setsockopt(lsock, SOL_SOCKET, SO_TIMESTAMPING, &enabled,
			sizeof(int));
	if (rc != 0) {
		close(lsock);
		return -1;
	}

	*sock = lsock;

	return 0;
}

/**
 * qbv_rx - Process to decode TSN & BE information from packet header
 *
 * @rcvPkt: igb_packet
 *
 * The process decodes, determines packet lost and pass the data to
 * message queue for another process to do further processing.
 */
static int qbv_rx(int sock, uint16_t udp_dest)
{
	unsigned char *test_stream_id;
	/* Store packet loss information based on its VLAN priority */
	static tsn_packet_loss_info pkt_loss_info[MAX_NUM_VLAN_PRIO];
	/* Store previous packet information based on VLAN priority */
	static tsn_pkt_cached_data cached_data[MAX_NUM_VLAN_PRIO];

	TSN_data *ptsn;

	/* Initialize empty msghdrs */
	struct msghdr msg;
	struct iovec iov;
	struct sockaddr_in host_address;
	char buffer[PKT_SZ_RX];
	char control[RX_PKT_CTRL_SZ];

	int rc = 0;

	/* Initialize empty addr */
	bzero(&host_address, sizeof(struct sockaddr_in));
	host_address.sin_family = AF_INET;
	host_address.sin_port = htons(0);
	host_address.sin_addr.s_addr = INADDR_ANY;

	/* recvmsg header structure */
	iov.iov_base = buffer;
	iov.iov_len = 128;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_name = &host_address;
	msg.msg_namelen = sizeof(struct sockaddr_in);
	msg.msg_control = control;
	msg.msg_controllen = 128;

	/* Get received packet information */
	rc = recvmsg(sock, &msg, MSG_DONTWAIT);

	if (!rc && errno == EAGAIN)
		return 0;
	if (rc <= 0) {
		/* No message in buffer, exit */
		usleep(100);
		return 0;
	}

	/********* Store payload & extract timestamp **********/
	test_stream_id = (unsigned char *) buffer
					+ IP_HEADER_SIZE
					+ UDP_HEADER_SIZE + 14;

#if DEBUG
	printf("test_stream_id[4]: 0x%02x\n", test_stream_id[4]);
	uint8_t i;

	/* Print out the received packet hex data */
	printf("received a packet ! hex dump\n");
	for (i = 0; i < 64; i++)
		printf("%02x ", buffer[i]);

	printf("\n\n");
#endif /* DEBUG*/

	/* Check source port of the packets */
	uint32_t dest_port = ((buffer[36] & 0xff) << 8) | (buffer[37] & 0xff);

	/* Check packet type. */
	if ((test_stream_id[4] & 0xf0) == TYPE_TSN && dest_port == udp_dest) {
		/********* Store received data **********/
		ptsn = (TSN_data *)test_stream_id;
		data_rcv.data.seq = ptsn->seq;
		data_rcv.data.pktType = ptsn->pktType;
		data_rcv.data.sender_timestamp = ptsn->timestamp;

		/*
		 * Extract RX timestamp
		 * ptp4l+phc2sys MUST be running to receive accurate timestamp
		 */
		data_rcv.data.rcv_timestamp = extract_timestamp(&msg);
		/* Extract VLAN priority information */
		uint16_t vprio = data_rcv.data.pktType & 0x0f;

		if (vprio >= MAX_NUM_VLAN_PRIO) {
			pr_err("Acquired VLAN priority is larger than expected");
			return -1;
		}

		/* Calculate latency */
		data_rcv.data.latency = data_rcv.data.rcv_timestamp
					- data_rcv.data.sender_timestamp;
		/* Calculate interpacket latency */
		if (cached_data[vprio].prev_rxtime != 0)
			data_rcv.data.interpkt_delta = data_rcv.data.rcv_timestamp
					- cached_data[vprio].prev_rxtime;

		/********* Determine Packet loss **********/
		/* TSN packet loss check */
		if (pkt_loss_info[vprio].first_time_tsn == 0) {
			pkt_loss_info[vprio].seq_count_tsn =
							data_rcv.data.seq;
			pkt_loss_info[vprio].prev_seq_count_tsn =
							data_rcv.data.seq;
			pkt_loss_info[vprio].first_time_tsn = 1;
		} else {
			if (data_rcv.data.seq ==
			    pkt_loss_info[vprio].prev_seq_count_tsn) {
#if	OUTPUT_FILE
				fprintf(fp,
					"Same sequence number as previous\n");
#endif
				printf("Same sequence number as previous\n");
			} else {
				if (++pkt_loss_info[vprio].seq_count_tsn !=
				    data_rcv.data.seq) {
#if	OUTPUT_FILE
					fprintf(fp, "seq_count_tsn: %d "
						"Received Seq: %d\n",
						pkt_loss_info[vprio].seq_count_tsn,
						data_rcv.data.seq);
#endif
					if (data_rcv.data.seq >
					    pkt_loss_info[vprio].seq_count_tsn)
						pkt_loss_info[vprio].packet_loss_tsn =
							data_rcv.data.seq -
							pkt_loss_info[vprio].seq_count_tsn;
					else if (data_rcv.data.seq <
						 pkt_loss_info[vprio].seq_count_tsn)
#if	OUTPUT_FILE
						fprintf(fp,
							"Packet sequence "
							"number out of order. "
							"Restart the program.\n");
#endif
				}
			}
			pkt_loss_info[vprio].prev_seq_count_tsn =
						data_rcv.data.seq;
		}

		/* copy packet loss count */
		data_rcv.data.packet_loss =
			pkt_loss_info[vprio].packet_loss_tsn;

		/********* Send Packet via msgsnd **********/
		data_rcv.message_type = MSGQ_TYPE;
		if ((rxDisplay & 0x01) == 0x01) {
			/* Send data to message queue - IO */
			if (msgsnd(msqid, (void *)&data_rcv,
			    sizeof(data_rcv.data), IPC_NOWAIT) == -1) {
				if (errno == EAGAIN)
					/* printf("Message queue is full\n");*/
					fprintf(fp, "msgsnd(): Message queue "
						"is full\n");
				else
					printf("msgsnd (msqid) failed (%s) -\n",
						strerror(errno));
			}
		}

		if ((rxDisplay & 0x02) == 0x02) {
			/* Send data to message queue - graph */
			if (msgsnd(msqgraph, (void *)&data_rcv,
			    sizeof(data_rcv.data), IPC_NOWAIT) == -1) {
				if (errno == EAGAIN)
					/* printf("Message queue is full\n");*/
					fprintf(fgp, "# msgsnd(): Message queue "
						"is full\n");
				else
					printf("msgsnd (msqgraph) failed (%s) "
					       "-\n", strerror(errno));
			}
		}

		/* Cached current rxtime of packet */
		cached_data[vprio].prev_rxtime = data_rcv.data.rcv_timestamp;
	}

	return 0;
}

/****************************
 * Process I/O
 ****************************/

/**
 * processIO - Write received data to log file.
 *
 * @arg: Argument called from pthread_create() is not used in this function
 */
void *processIO(void *arg)
{
	(void) arg;		/* unused */
	msg_buff trans_data;

	memset(&trans_data, 0, sizeof(trans_data));

	while (!halt_tx_sig) {
		if (msgrcv(msqid, (void *)&trans_data, sizeof(trans_data.data),
			 MSGQ_TYPE, IPC_NOWAIT) == -1) {
			if (errno != ENOMSG)
				printf("msgrcv (msqid) failed (%s) -\n",
				       strerror(errno));

			usleep(1);
		} else {
			uint16_t vprio = trans_data.data.pktType & 0x0f;

			if ((trans_data.data.pktType & 0xf0) == TYPE_TSN) {
				if (find_TSN_prio(priority_filter_list,
				    MAX_NUM_VLAN_PRIO, vprio) != -1) {
					uint32_t seq = trans_data.data.seq;
					uint64_t latency =
							trans_data.data.latency;
					uint64_t delta =
							trans_data.data.interpkt_delta;
					printf("TSN VLAN Priority %d - Seq: %u "
					       "Latency = %ld ns "
					       "Inter-packet latency = %ld ns\n",
					       vprio, seq, latency, delta);
					fprintf(fp, "TSN VLAN Priority %d - "
						"Seq: %u Latency = %ld ns "
						"Inter-packet latency = %ld ns\n",
						vprio, seq, latency, delta);
				}
			}
		}
	}

	pthread_exit(NULL);
}

/**
 * processGraphData - Write to log file for graph data plotting use
 *
 * @arg: Argument called from pthread_create() is not used in this function
 */
void *processGraphData(void *arg)
{
	(void) arg;	/* unused */
	QBV_demo_data tsn_data;
	msg_buff trans_data;
	uint16_t vprio;
	uint32_t fgp_buffer[9][3];
	static int first_data = 1;
	uint32_t loop = 0;
	uint32_t count = 0;
	double time_x = 0.0;
	uint64_t graph_cycle = cycle_time / 2000; /* Sample 2x rate */
	double period_x = ((double)graph_cycle / 1000); /* X-axis in ms */

	memset(&trans_data, 0, sizeof(trans_data));
	memset(&tsn_data, 0, sizeof(tsn_data));

	while (!halt_tx_sig) {
		usleep(graph_cycle);
		for (loop = 0; loop < 2; loop++) {
			if (msgrcv(msqgraph, (void *)&trans_data,
			    sizeof(trans_data.data),
			    MSGQ_TYPE, IPC_NOWAIT) == -1) {
				if (errno != ENOMSG)
					printf("msgrcv (msqgraph) failed (%s) "
					       "-\n", strerror(errno));
			} else {
				if ((trans_data.data.pktType & 0xf0) ==
				    TYPE_TSN) {
					if (first_data)
						pr_info("Started writing to graph data files");
					first_data = 0;

					vprio = trans_data.data.pktType & 0x0f;
					fgp_buffer[vprio][0] =
						trans_data.data.seq;
					fgp_buffer[vprio][1] =
						trans_data.data.latency;
					fgp_buffer[vprio][2] =
						trans_data.data.packet_loss;
					fprintf(flat, "%d %ld\n", vprio,
							trans_data.data.interpkt_delta);
				} else
					fprintf(fgp, "# ERROR: UNKNOWN packet "
						"type\n");
			}
		}

		/*
		 * Output packet information to the dat file defined by FNAME_GRAPH
		 * -------------
		 * Legends:
		 * - <queue number>:S - sequence
		 * - <queue number>:L - latency
		 * - <queue number>:PL - packet loss
		 *
		 * e.g.:
		 * <Time> 0:S 0:L 0:PL 1:S 1:L 1:PL 2:S 2:L 2:PL 3:S 3:L 3:PL
		 *	   4:S 4:L 4:PL 5:S 5:L 5:PL 6:S 6:L 6:PL 7:S 7:L 7:PL
		 */
		fprintf(fgp, "%lf ", time_x);
		/* Print all stored sequence, latency & packet loss info */
		for (int i = 0; i < MAX_NUM_VLAN_PRIO; i++)
			fprintf(fgp, "%u %d %u ", fgp_buffer[i][0],
				fgp_buffer[i][1], fgp_buffer[i][2]);

		fprintf(fgp, "\n");

		if (++count >= FLUSH_COUNT) {
			count = 0;
			fflush(fgp);
			fflush(flat);
		}

		time_x += period_x;
	}

	pthread_exit(NULL);
}

/****************************
 * Tx data generation use
 ****************************/

/**
 * inet_checksum_sg - Calculates Checksum for ethernet packet
 *
 * @buf_iov: Data
 * @buf_iovlen: Buffer length
 */
uint16_t inet_checksum_sg(struct iovec *buf_iov, size_t buf_iovlen)
{
	size_t i;
	uint8_t residual;
	uint32_t sum = 0;  /* Assume 32 bit long, 16 bit short */
	int has_residual = 0;

	for (i = 0; i < buf_iovlen; ++i, ++buf_iov) {
		if (has_residual) {
			if (buf_iov->iov_len > 0) {
				/* If high order bit set, fold */
				if (sum & 0x80000000)
					sum = (sum & 0xFFFF) + (sum >> 16);
				sum += residual |
					(*((uint8_t *) buf_iov->iov_base) << 8);
				buf_iov->iov_base += 1;
				buf_iov->iov_len -= 1;
			} else {
				/* If high order bit set, fold */
				if (sum & 0x80000000)
					sum = (sum & 0xFFFF) + (sum >> 16);
				sum += (uint16_t) residual;
			}
			has_residual = 0;
		}
		while (buf_iov->iov_len > 1) {
			/* If high order bit set, fold */
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

/****************************
 * Tx
 ****************************/

/**
 * udp_open - Wrapper to open RAW socket
 *
 * @name: Interface name
 * @priority: Socket priority
 * @ip_ptr: Pointer to ip address
 * @udp_dest: User input for UDP port number
 */
static int udp_open(const char *name, int priority, char *ip_ptr, uint16_t port,
					 clockid_t clkid)
{
	int fd = 1;
	struct ifreq if_request;

	if (!inet_aton(ip_ptr, &mcast_addr))
		return -1;

	fd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW);
	if (fd == -1) {
		fprintf(stderr, "Raw socket error.\n");
		return -1;
	}

	/* Get the index of the interface to send on */
	memset(&if_request, 0, sizeof(struct ifreq));
	strncpy(if_request.ifr_name, name, sizeof(if_request.ifr_name) - 1);
	if (ioctl(fd, SIOCGIFINDEX, &if_request) < 0) {
		fprintf(stderr, "Failed to get index of %s.\n", name);
		close(fd);
		return -1;
	}
	raw_index  = if_request.ifr_ifindex;

	if (setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &priority,
		       sizeof(priority))) {
		pr_err("Couldn't set socket priority: %m\n");
		goto no_option;
	}

	memset(&if_request, 0, sizeof(struct ifreq));
	strncpy(if_request.ifr_name, name, sizeof(if_request.ifr_name) - 1);

	/* Set SO_TXTIME socket option */
	sk_txtime.clockid = clkid;
	sk_txtime.flags = (use_deadline_mode | receive_errors);
	if (use_so_txtime && setsockopt(fd, SOL_SOCKET, SO_TXTIME, &sk_txtime,
	    sizeof(sk_txtime))) {
		pr_err("setsockopt SO_TXTIME failed: %m");
		goto no_option;
	}

	return fd;

no_option:
	close(fd);
	return -1;
}

/**
 * udp_send - Wrapper to open socket
 *
 * @fd: Socket file descriptor
 * @buf: Buffer for data/payload
 * @len: Buffer length
 * @txtime: Packet transmit time to pass to CMSG
 * @udp_dest: UDP destination port number
 */
static int udp_send(int fd, void *payloadbuf, int len, short udp_dest,
		    __u64 txtime)
{
	char control[CMSG_SPACE(sizeof(__u64))] = {};
	ssize_t cnt;
	struct cmsghdr *cmsg;
	struct iovec iov;
	struct msghdr msg;
	struct sockaddr_ll sin;
	struct timespec now;

	memcpy(&l4_headers->data.seq, payloadbuf, 4);
	memcpy(&l4_headers->data.pktType, payloadbuf + 4, 4);

	if (l4_headers->data.pktType == TYPE_TSN | vpriority_TSN) {
		rawpktbuf[14] = ((vpriority_TSN << 13 | vid_TSN)) >> 8;
		rawpktbuf[15] = ((vpriority_TSN << 13 | vid_TSN)) & 0xFF;
	}

	l4_headers->cksum = 0;
	l4_headers->dest_port = htons(udp_dest);
	{
		struct iovec iv[2];

		iv[0].iov_base = &pseudo_hdr;
		iv[0].iov_len = sizeof(pseudo_hdr);
		iv[1].iov_base = ((uint8_t *)l4_headers) + 20;
		iv[1].iov_len = packet_size - 18 - 20;
		l4_headers->cksum = inet_checksum_sg(iv, 2);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sll_family = AF_PACKET;
	sin.sll_ifindex = raw_index;		/* Network device's index */
	sin.sll_halen = ETH_ALEN;		/* Address length */
	sin.sll_addr[0] = glob_l2_dest_addr[0];	/* Destination MAC */
	sin.sll_addr[1] = glob_l2_dest_addr[1];
	sin.sll_addr[2] = glob_l2_dest_addr[2];
	sin.sll_addr[3] = glob_l2_dest_addr[3];
	sin.sll_addr[4] = glob_l2_dest_addr[4];
	sin.sll_addr[5] = glob_l2_dest_addr[5];

	memset(&iov, 0, sizeof(iov));
	iov.iov_base = rawpktbuf;
	iov.iov_len = sizeof(rawpktbuf);

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &sin;
	msg.msg_namelen = sizeof(sin);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	/* Specify the transmission time in the CMSG. */
	if (use_so_txtime && txtime > 0) {
		msg.msg_control = control;
		msg.msg_controllen = sizeof(control);

		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_TXTIME;
		cmsg->cmsg_len = CMSG_LEN(sizeof(__u64));
		*((__u64 *) CMSG_DATA(cmsg)) = txtime;
	}

	/* Record current time and put in buffer */
	clock_gettime(CLOCK_TAI, &now);
	uint64_t txtime_software = now.tv_sec * ONE_SEC + now.tv_nsec;

	memcpy(&l4_headers->data.timestamp, &txtime_software, sizeof(txtime_software));

	cnt = sendmsg(fd, &msg, 0);
	if (cnt < 1) {
		fprintf(stderr, "sendmsg failed - %d: %s\n", errno,
			strerror(errno));
		return cnt;
	}
	return cnt;
}

/**
 * set_realtime - Set specified pthread to run at specified priority and cpu.
 *
 * @pthread_t: pthread_t ID
 * @priority: Run at user specified priority
 * @cpu: Run on user specified cpu
 */
static int set_realtime(pthread_t thread, int priority, int cpu)
{
	cpu_set_t cpuset;
	struct sched_param sp;
	int err, policy;

	int min = sched_get_priority_min(SCHED_FIFO);
	int max = sched_get_priority_max(SCHED_FIFO);

	fprintf(stderr, "min %d max %d\n", min, max);

	if (priority < 0)
		return 0;

	err = pthread_getschedparam(thread, &policy, &sp);
	if (err) {
		fprintf(stderr, "pthread_getschedparam: %s\n", strerror(err));
		return -1;
	}

	sp.sched_priority = priority;

	err = pthread_setschedparam(thread, SCHED_FIFO, &sp);
	if (err) {
		fprintf(stderr, "pthread_setschedparam: %s\n", strerror(err));
		return -1;
	}

	if (cpu < 0)
		return 0;

	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);
	err = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (err) {
		fprintf(stderr, "pthread_setaffinity_np: %s\n", strerror(err));
		return -1;
	}

	return 0;
}

/**
 * normalize - Shift nanosecond overflow.
 *
 * @ts: timestamp timespec data structure
 */
static void normalize(struct timespec *ts)
{
	while (ts->tv_nsec > 999999999) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000;
	}
}

/**
 * calibrate_basetime - Align packet sending with TAPrio cycle
 *
 * @base_time: Time when TSN cycle starts
 * @clkid: Clock ID used to acquire time
 */
static void calibrate_basetime(uint64_t *base_time, clockid_t clkid)
{
	struct timespec now_ts;
	uint64_t now, new_basetime, nsec_left;

	clock_gettime(clkid, &now_ts);
	now = now_ts.tv_sec * ONE_SEC + now_ts.tv_nsec;

	/*
	 * If TAPrio base time has passed or time left until base time is larger
	 * or equal to waketx_delay, start the software cycle at the next
	 * TAPrio cycle + 1 sec ahead
	 */
	if (now >= *base_time || (now < *base_time &&
	    (*base_time - now <= waketx_delay))) {
		nsec_left = cycle_time - ((now - *base_time) % cycle_time);
		new_basetime = now + nsec_left + ONE_SEC;
		*base_time = new_basetime;
	}
}

/**
 * process_socket_error_queue - Get error queue message after sending packets
 *				with SO_TXTIME
 *
 * @fd: Socket file descriptor
 */
static int process_socket_error_queue(int fd)
{
	uint8_t msg_control[CMSG_SPACE(sizeof(struct sock_extended_err))];
	unsigned char err_buffer[sizeof(tx_buffer)];
	struct sock_extended_err *serr;
	struct cmsghdr *cmsg;
	__u64 tstamp = 0;

	struct iovec iov = {
		.iov_base = err_buffer,
		.iov_len = sizeof(err_buffer)
	};
	struct msghdr msg = {
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = msg_control,
		.msg_controllen = sizeof(msg_control)
	};

	if (recvmsg(fd, &msg, MSG_ERRQUEUE) == -1) {
		pr_err("recvmsg failed");
		return -1;
	}

	cmsg = CMSG_FIRSTHDR(&msg);
	while (cmsg != NULL) {
		serr = (void *) CMSG_DATA(cmsg);
		if (serr->ee_origin == SO_EE_ORIGIN_TXTIME) {
			tstamp = ((__u64) serr->ee_data << 32) + serr->ee_info;

			switch (serr->ee_code) {
			case SO_EE_CODE_TXTIME_INVALID_PARAM:
				fprintf(stderr, "packet with tstamp %llu dropped "
						"due to invalid params\n",
						tstamp);
				return 0;
			case SO_EE_CODE_TXTIME_MISSED:
				fprintf(stderr, "packet with tstamp %llu dropped "
						"due to missed deadline\n",
						tstamp);
				return 0;
			default:
				return -1;
			}
		}

		cmsg = CMSG_NXTHDR(&msg, cmsg);
	}

	return 0;
}

/**
 * send_no_so_txtime - Send packets without SO_TXTIME
 *
 * @window: Window struct that contains information on each window
 * @sockfd: Socket file descriptor
 * @clkid: Clock ID to acquire time
 * @udp_dest: Destination address
 * @txtime: Launchtime (not needed here, value is zero)
 */
static void send_no_so_txtime(tx_window *window, int sockfd,
			      clockid_t clkid, uint16_t udp_dest,
			      struct timespec *ts)
{
	struct timespec ts_timer;
	struct timespec ts_now_timer;

	close_window = 0;
	send_packet_now = 1;
	uint64_t now_time;
	uint64_t closing_time;

	ts_timer.tv_sec = ts->tv_sec;
	ts_timer.tv_nsec = ts->tv_nsec + window->duration;
	normalize(&ts_timer);
	closing_time = ts_timer.tv_sec * ONE_SEC +
		       ts_timer.tv_nsec;
	while (1) {
		if (send_packet_now) {
			/* Send multiple packets in a window */
			for (int i = 0;
			     i < window->num_packets; ++i) {
				++seqNum;
				memcpy(tx_buffer, &seqNum, sizeof(seqNum));
				memcpy(tx_buffer + 4, &pktType,
				       sizeof(pktType));
				/* check if window is closed */
				clock_gettime(clkid, &ts_now_timer);
				now_time = ts_now_timer.tv_sec * ONE_SEC +
					   ts_now_timer.tv_nsec;
				if (now_time >= closing_time) {
					close_window = 1;
					break;
				}
				if (udp_send(sockfd, tx_buffer,
					     sizeof(tx_buffer),
					     udp_dest, -1) < 1) {
					pr_err("udp_send failed");
					//exit(EXIT_FAILURE);
				}
			}
			send_packet_now = 0;
		}
		/* check if window is closed */
		clock_gettime(clkid, &ts_now_timer);
		now_time = ts_now_timer.tv_sec * ONE_SEC + ts_now_timer.tv_nsec;
		if (now_time >= closing_time)
			close_window = 1;
		if (close_window)
			break;
	}
}

/**
 * send_so_txtime - Send packets with SO_TXTIME
 *
 * @window: Window struct that contains information on each window
 * @seqNum: Pointer to packet sequence number
 * @sockfd: Socket file desciptor
 * @clkid: Clock ID to acquire time
 * @udp_dest: Destination address
 * @p_fd: Poll file desciptor
 * @txtime: Launchtime
 */
static void send_so_txtime(tx_window *window, int sockfd, clockid_t clkid,
			   uint16_t udp_dest, struct pollfd *p_fd, __u64 txtime)
{
	for (int i = 0; i < window->num_packets; ++i) {
		++seqNum;
		memcpy(tx_buffer, &seqNum, sizeof(seqNum));
		memcpy(tx_buffer + 4, &pktType, sizeof(pktType));

		if (udp_send(sockfd, tx_buffer, sizeof(tx_buffer), udp_dest,
		    txtime) < 1) {
			pr_err("udp_send failed");
			//exit(EXIT_FAILURE);
		}
		if (window->num_packets > 1)
			txtime += ((window->duration/2) / (window->num_packets + 1));

		/* Check if errors are pending on the error queue. */
		int err = poll(p_fd, 1, 0);

		if (err == 1 && p_fd->revents & POLLERR) {
			if (!process_socket_error_queue(sockfd)) {
				pr_err("process socket error queue error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

/**
 * processTx - Continuously execute Transmit packet process until break
 *
 * @args: Arguments passed from main()
 */
static void *processTx(void *args)
{
	int err;
	socket_fd_ptrs *argsX = args;
	int fdTSN = argsX->fdTSN;
	uint16_t udp_dest = argsX->udp_dest;
	clockid_t clkid = argsX->clkid;

	struct timespec ts;
	/* Mask packet type to take into account its priority */
	pktType = TYPE_TSN | vpriority_TSN;
	__u64 txtime = 0;
	uint64_t first_offset = packet_windows[0].offset;
	struct pollfd p_fd = {
		.fd = fdTSN,
	};

	printf("\nTSN VLAN ID: %d\n", vid_TSN);
	printf("TSN priority: %d\n", vpriority_TSN);
	printf("Cycle time: %luns\n", cycle_time);

	if (use_so_txtime) {
		printf("\nLaunchtime ");
		if (use_deadline_mode)
			printf(" deadline mode ");
		printf("enabled\n");
	} else {
		printf("Launchtime disabled\n");
	}

	if (base_time)
		printf("TSN cycle starts at (ns): %ld\n", base_time);
	else
		printf("No base time specified. "
		       "Cycle will start %d seconds from now\n",
		       CYCLE_OFFSET);

	/* If no base time specified, starts cycle CYCLE_OFFSET secs ahead */
	if (base_time == 0) {
		clock_gettime(clkid, &ts);
		ts.tv_sec += CYCLE_OFFSET;
		ts.tv_nsec = CYCLE_OFFSET * ONE_SEC - waketx_delay;
		normalize(&ts);
		if (use_so_txtime) {
			txtime = ts.tv_sec * ONE_SEC + ts.tv_nsec;
			txtime += waketx_delay;
		}
	} else {
		calibrate_basetime(&base_time, clkid);
		base_time += (first_offset + tx_window_offset);
		if (use_so_txtime)
			txtime = base_time;
		base_time -= waketx_delay;
		ts.tv_sec = base_time / ONE_SEC;
		ts.tv_nsec = base_time % ONE_SEC;
		normalize(&ts);
	}

	/*
	 * Packet sending process:
	 *
	 * With SO_TXTIME:
	 * .....................____|    |____________________|    |________
	 *   -- waketx_delay --        ^                         ^
	 * The program will start sending the packet from the software side
	 * sendmsg @waketx_delay before Launchtime and txtime info is placed in
	 * cmsg struct. Launchtime is being set TX_WINDOW_OFFSET within the
	 * window.
	 *
	 * Without SO_TXTIME:
	 * ____|    |____________________|    |_______
	 *      ...^                      ...^
	 * As Launchtime is not used, the time the packet will leave the hardware
	 * cannot be guaranteed. The program use clock_nanosleep to ensure packet
	 * will be sent at anytime within its respective windows
	 *
	 */
	while (!halt_tx_sig) {
		for (int win_index = 0; win_index < num_window_per_cycle;
		      ++win_index) {
			err = clock_nanosleep(clkid, TIMER_ABSTIME, &ts, NULL);
			switch (err) {
			case 0:
				if (use_so_txtime)
					send_so_txtime(&packet_windows[win_index],
						       fdTSN, clkid, udp_dest,
						       &p_fd, txtime);
				else
					send_no_so_txtime(&packet_windows[win_index],
							  fdTSN, clkid,
							  udp_dest, &ts);

				if (win_index == num_window_per_cycle - 1) {
					ts.tv_nsec += (cycle_time -
						       packet_windows[win_index].offset +
						       first_offset);
					if (use_so_txtime) {
						txtime += (cycle_time -
							   packet_windows[win_index].offset +
							   first_offset);
					}
				} else {
					ts.tv_nsec += (packet_windows[win_index + 1].offset -
						       packet_windows[win_index].offset);
					if (use_so_txtime) {
						txtime += (packet_windows[win_index + 1].offset -
							   packet_windows[win_index].offset);
					}
				}
				normalize(&ts);
				break;
			case EINTR:
				continue;
			default:
				fprintf(stderr, "clock_nanosleep returned %d: %s\n",
					err, strerror(err));
				break;
			}
		}
	}
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	pthread_t t1, t2, t3;

	t1 = -1; t2 = -1; t3 = -1;
	int err = 0;
	int c = 0;
	int rc = 0;
	int pterr[3] = { 0 };

	/* For message queue */
	struct msqid_ds mqbuf = {0};

	/* For Tx */
	struct sockaddr_in local;
	struct ifreq if_request;
	unsigned int l4_local_address = 0;
	int sd = -1;
	/* For Tx launch time */
	int fdTSN = -1;

	/* For Tx data */
	clockid_t clkid = CLOCK_TAI;

	/* For Rx */
	int sock = -1;

	/* Input arguments variables */
	char *basetime_filename = NULL;
	char *interface = NULL;
	char *selectFilter = NULL;
	char *ip_ptr = NULL;
	char *file_name = default_file;
	uint16_t udp_src = UDP_SRC_PORT_DEFAULT;
	uint16_t udp_dest = UDP_DST_PORT_DEFAULT;
	int cpu = CPU_NUM_DEFAULT;
	int priority = PTHREAD_PRIORITY_DEFAULT;

	/* Config file name for packet window */
	char *win_config_filename = NULL;

	/* Populate priority_filter_list */
	for (int filter_count = 0; filter_count < MAX_NUM_VLAN_PRIO;
	     ++filter_count)
		priority_filter_list[filter_count] = -1;

#if CHECK_TIME
	clock_t start, end;
	double cpu_time_used;
#endif

	for (;;) {
		c = getopt(argc, argv,
			   "A:b:B:c:d:De:Ef:hi:n:o:p:P:q:s:St:v:w:x:y:z:");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'A':
			cpu = atoll(optarg);
			break;
		case 'b':
			base_time = atoll(optarg);
			break;
		case 'B':
			basetime_filename = strdup(optarg);
			break;
		case 'c':
			ip_ptr = strdup(optarg);
#if DEBUG
			printf("IP input argument: %s\n", ip_ptr);
#endif
			err = convert_input_IP(ip_ptr,
					       glob_l3_dest_addr);
			if (err) {
				printf("Error: Incorrect IP address "
				       "input\n\n");
				usage();
			}
			break;
		case 'd':
			txDisplay = strtoul(optarg, NULL, 10);
			break;
		case 'D':
			use_deadline_mode = SOF_TXTIME_DEADLINE_MODE;
			break;
		case 'e':
			tx_window_offset = strtoul(optarg, NULL, 10);
			break;
		case 'E':
			receive_errors = SOF_TXTIME_REPORT_ERRORS;
			break;
		case 'f':
			file_name = strdup(optarg);
			break;
		case 'i':
			if (interface) {
				printf("Only one interface per daemon "
				       "is supported\n");
				usage();
			}
			interface = strdup(optarg);
			break;
		case 'n':
			udp_dest = strtoul(optarg, NULL, 10);
			break;
		case 'o':
			udp_src = strtoul(optarg, NULL, 10);
			break;
		case 'p':
			vpriority_TSN = strtoul(optarg, NULL, 10);
			break;
		case 'P':
			priority = atoll(optarg);
			break;
		case 'q':
			selectFilter = strdup(optarg);
			char *array[MAX_NUM_VLAN_PRIO];
			/* Split string to store in an array */
			char *p = strtok(selectFilter, " ");
			int i = 0;
			int number_of_argument = 0;

			while (p != NULL) {
				array[i++] = p;
				p = strtok(NULL, " ");
			}
			number_of_argument = i;
			/*
			 * Store string in priority_filter_list array
			 * to be used in processIO/processGraphData
			 * as filtering mechanism.
			 */
			for (i = 0; i < number_of_argument; i++) {
				priority_filter_list[i] =
						atoi(array[i]);
			}
			break;
		case 'S':
			use_so_txtime = 0;
			break;
		case 't':
			cycle_time = strtoul(optarg, NULL, 10);
			if ((cycle_time < 1000000) ||
			    (cycle_time >= 500000000))
				usage();
			break;
		case 'v':
			vid_TSN = strtoul(optarg, NULL, 10);
			break;
		case 'w':
			win_config_filename = strdup(optarg);
			break;
		case 'x':
			rxtxType = strtoul(optarg, NULL, 10);
			break;
		case 'y':
			rxDisplay = strtoul(optarg, NULL, 10);
			break;
		case 'z':
			waketx_delay = strtoul(optarg, NULL, 10);
			break;
		}
	}
	if (optind < argc)
		usage();
	if (interface == NULL) {
		printf("Must specify an interface.\n");
		usage();
	}
	if ((rxtxType & 0x01) == 0x01 && ip_ptr == NULL) {
		printf("Must specify client IP address.\n");
		usage();
	}
	if ((rxtxType & 0x01) == 0x01 &&
	    win_config_filename == NULL) {
		pr_err("Must specify TSN config file");
		usage();
	}

#if DEBUG
	err = print_ip_address(interface);
	if (err)
		printf("print_ip_address failed - (%s) Error\n",
			strerror(errno));
#endif

	/****************************
	 * Initialization
	 *	1) init ethernet driver
	 *	2) init tx buffer
	 *	3) init receive buffer
	 *	4) setup receive filtering
	 *	5) init tx data and socket
	 *	6) init rx socket
	 ****************************/
	halt_tx_sig = 0;

	err = get_mac_address(interface);
	if (err) {
		printf("get_mac_address failed - (%s) Error\n",
		strerror(errno));
		goto err_out;
	}

	if ((rxtxType & 0x01) == 0x01) {
		/* IP & MAC information */
		printf("Dest IP: %d.%d.%d.%d\n", glob_l3_dest_addr[0],
		       glob_l3_dest_addr[1], glob_l3_dest_addr[2],
		       glob_l3_dest_addr[3]);
		printf("UDP source port: %d\n", udp_src);
		printf("UDP destination port: %d\n", udp_dest);

		err = get_mac_address(interface);

		if (err) {
			printf("Error: failed to open interface\n");
			goto err_out;
		}

#if UNICAST
		err = get_remote_mac_address(interface, glob_l2_dest_addr,
					     glob_l3_dest_addr);
		if (err) {
			printf("get_remote_mac_address failed - (%s) Error\n",
			strerror(errno));
			goto err_out;
		}
#else
		l3_to_l2_multicast(glob_l2_dest_addr, glob_l3_dest_addr);
#endif

		/* Parse variables from window config file */
		if (parse_config(win_config_filename) < 0) {
			pr_err("Config file parsing error!");
			goto err_out;
		}

		/* Read base time from file generated by scheduler */
		if (basetime_filename) {
			fp = fopen(basetime_filename, "r");
			if (fp == NULL) {
				pr_err("Unable to open file for writing!\n");
				goto err_out;
			} else {
				if (fscanf(fp, "%ld", (long *) &base_time) == 0) {
					printf("Error extracting data from %s\n",
					       basetime_filename);
					goto err_out;
				}
			}
			fclose(fp);
			fp = NULL;
		}

		/* Create tx socket */
		memset(&local, 0, sizeof(local));
		local.sin_family = PF_INET;
		local.sin_addr.s_addr = htonl(INADDR_ANY);
		local.sin_port = htons(udp_dest);

		memset(&if_request, 0, sizeof(if_request));
		strncpy(if_request.ifr_name, interface,
			sizeof(if_request.ifr_name) - 1);
		sd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sd == -1) {
			printf("Failed to open socket: %s\n", strerror(errno));
			goto err_out;
		}
		if (ioctl(sd, SIOCGIFADDR, &if_request) != 0) {
			printf("Failed to get interface address (ioctl) "
			       "on socket: %s\n", strerror(errno));
			goto err_out;
		}
		memcpy(&l4_local_address,
		       &((struct sockaddr_in *)&if_request.ifr_addr)->sin_addr,
		       sizeof(l4_local_address));

		/* Store local IP in global */
		memcpy(&glob_l3_src_addr,
		       &((struct sockaddr_in *)&if_request.ifr_addr)->sin_addr,
		       sizeof(glob_l3_src_addr));
		printf("Src IP: %d.%d.%d.%d\n", glob_l3_src_addr[0],
		       glob_l3_src_addr[1],
		       glob_l3_src_addr[2],
		       glob_l3_src_addr[3]);

		halt_tx_sig = 0;

		fdTSN = udp_open(interface, vpriority_TSN, ip_ptr, udp_dest,
				 clkid);
		if (fdTSN < 0)
			goto err_out;

		packet_size = 18 + sizeof(*l4_headers);

		memset(rawpktbuf, 0, RAW_PACKET_SIZE);
		memset(rawpktbuf, 0, packet_size);

		/* MAC header */
		memcpy(rawpktbuf, glob_l2_dest_addr, sizeof(glob_l2_dest_addr));
		memcpy(rawpktbuf + 6, glob_l2_src_addr,
		       sizeof(glob_l2_src_addr));

		/* Q-tag header*/
		rawpktbuf[12] = 0x81;
		rawpktbuf[13] = 0x00;
		rawpktbuf[14] = ((vpriority_TSN << 13 | vid_TSN)) >> 8;
		rawpktbuf[15] = ((vpriority_TSN << 13 | vid_TSN)) & 0xFF;
		rawpktbuf[16] = 0x08;	/* IP eth type */
		rawpktbuf[17] = 0x00;

		pseudo_hdr.source = l4_local_address;
		memcpy(&pseudo_hdr.dest, glob_l3_dest_addr,
		       sizeof(pseudo_hdr.dest));
		pseudo_hdr.zero = 0;
		pseudo_hdr.protocol = 0x11;
		pseudo_hdr.length = htons(packet_size-18-20);

		l4_headers = (IP_UDP_QBVdemo_Header *)(rawpktbuf + 18);
		l4_headers->version_length = 0x45;
		l4_headers->DSCP_ECN = 0x00;
		l4_headers->ip_length = htons(packet_size - 18);
		l4_headers->id = 0;
		l4_headers->fragmentation = 0x40;
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

		l4_headers->source_port = htons(udp_src);
		l4_headers->dest_port = htons(udp_dest);
		l4_headers->udp_length = htons(packet_size-18-20);
		l4_headers->data.seq = 0;
		l4_headers->data.pktType = 0;
		l4_headers->data.timestamp = 0;

	}
	/****************************
	 *  RX Initialization
	 *  1) init ethernet driver
	 *  2) init receive buffer
	 *  3) setup receive filtering
	 *  4) init rx socket
	 ****************************/
	if ((rxtxType & 0x02) == 0x02) {
#if UNICAST
		/* Create rx socket */
		init_rx_socket(0x0800, &sock, glob_l2_src_addr, interface);
#else
		l3_to_l2_multicast(glob_l2_src_addr, glob_l3_src_addr);

		/* Create rx socket */
		/* TODO: not tested. */
		init_rx_socket(0x0000, &sock, glob_l2_src_addr, interface);
#endif

		/* Run RX filter command via ethtool */
		if (config_rx_filter(interface) < 0)
			goto rx_out;
		pr_info("Filter configuration done");

		/* Create file for data logging */
		if (file_name)
			fp = fopen(file_name, "w");
		else {
			pr_err("File is not initialized!");
			goto rx_out;
		}
		if (fp == NULL)	{
			printf("Unable to open file for writing!\n");
			goto rx_out;
		}

		/* Create file for graph plotting use */
		fgp = fopen(FNAME_GRAPH, "w");
		if (fgp == NULL) {
			printf("Unable to open file for writing!\n");
			goto rx_out;
		}

		fprintf(fgp, "# <Time> "
			     "<TSN-0 seq> <TSN-0 Latency> <TSN-0 Packet loss> "
			     "<TSN-1 seq> <TSN-1 Latency> <TSN-1 Packet loss> "
			     "<TSN-2 seq> <TSN-2 Latency> <TSN-2 Packet loss> "
			     "<TSN-3 seq> <TSN-3 Latency> <TSN-3 Packet loss> "
			     "<TSN-4 seq> <TSN-4 Latency> <TSN-4 Packet loss> "
			     "<TSN-5 seq> <TSN-5 Latency> <TSN-5 Packet loss> "
			     "<TSN-6 seq> <TSN-6 Latency> <TSN-6 Packet loss> "
			     "<TSN-7 seq> <TSN-7 Latency> <TSN-7 Packet loss> "
		       );
		fflush(fgp);

		/* Create file for inter-packet latency distribution graph */
		flat = fopen("latencies.dat", "w");
		if (flat == NULL) {
			pr_err("Unable to open file for writing!\n");
			goto rx_out;
		}

		signal(SIGINT, sigint_handler);

		/****************************
		 * Message queue initialization
		 ****************************/
		/* Init data input buffer */
		memset(&data_rcv, 0, sizeof(data_rcv));

		if ((rxDisplay & 0x01) == 0x01) {
			msqid = msgget((key_t)MSG_KEY_IO, 0644 | IPC_CREAT);
			if (msqid == -1) {
				fprintf(stderr, "msgget (msqid) failed with "
					"error: %d\n", errno);
				goto rx_out;
			}

			if (msgctl(msqid, IPC_STAT, &mqbuf) == -1)
				fprintf(stderr, "msgctl(IPC_STAT) failed\n");

			mqbuf.msg_qbytes = MESSAGE_QUEUE_SIZE;

			if (msgctl(msqid, IPC_SET, &mqbuf) == -1)
				fprintf(stderr, "msgctl(IPC_SET) failed\n");
		}

		if ((rxDisplay & 0x02) == 0x02) {
			msqgraph =
				msgget((key_t)MSG_KEY_GRAPH, 0644 | IPC_CREAT);
			if (msqgraph == -1) {
				fprintf(stderr, "msgget (msqgraph) failed "
					"with error: %d\n", errno);
				goto rx_out;
			}
		}
	}

	/****************************
	 * Create transmit process and I/O process thread
	 ****************************/

	if ((rxDisplay & 0x01) == 0x01) {
		pterr[0] = pthread_create(&t1, NULL, processIO, NULL);
		if (pterr[0]) {
			fprintf(stderr, "pthread_create processIO() "
				"error: %d\n", pterr[0]);
			goto rx_out;
		}
	}

	if ((rxtxType & 0x01) == 0x01) {
		socket_fd_ptrs *args = malloc(sizeof(*args));

		if (args == NULL) {
			pr_err("malloc error");
			goto rx_out;
		}

		args->fdTSN = fdTSN;
		args->clkid = clkid;
		args->udp_dest = udp_dest;

		pterr[1] = pthread_create(&t2, NULL, processTx, args);
		if (pterr[1]) {
			fprintf(stderr, "pthread_create processTx() "
				"error: %d\n", pterr[1]);
			goto rx_out;
		}

		pterr[1] = set_realtime(t2, priority, cpu);
		if (pterr[1]) {
			fprintf(stderr, "set_realtime for pthread error: %d\n",
				pterr[1]);
			goto rx_out;
		}
		pthread_join(t2, NULL);
	}

	if ((rxtxType & 0x02) == 0x02)
		pr_info("Waiting for incoming packet data...");

	if (((rxDisplay & 0x02) == 0x02) && ((rxtxType & 0x02) == 0x02)) {
		pterr[2] = pthread_create(&t3, NULL, processGraphData, NULL);
		if (pterr[2]) {
			fprintf(stderr, "pthread_create processGraphData() "
				"error: %d\n", pterr[2]);
			goto rx_out;
		}
	}

	/****************************
	 * While main loop to process receive and sleep
	 ****************************/
	err = nice(-20);
	if (err == -1)
		printf("nice() failed: (%s)\n", strerror(errno));

	while (!halt_tx_sig) {
		/*********************************************
		 * Check Receive packets
		 *********************************************/
		if ((rxtxType & 0x02) == 0x02) {
#if CHECK_TIME
			start = clock();
#endif

			/* Process received packets */
			err = qbv_rx(sock, udp_dest);
			if (err) {
				printf("qbv_rx failed - (%s) Error\n",
				strerror(errno));
			}

#if CHECK_TIME
			end = clock();
			cpu_time_used = ((double) (end - start)) /
					CLOCKS_PER_SEC;
			fprintf(fp, "qbv_rx() time: %f\n", cpu_time_used);
#endif

		}
	}

	/****************************
	 * Cleanup
	 ****************************/
	if (fdTSN != -1)
		close(fdTSN);
	if (sd != -1)
		close(sd);
	if (sock != -1)
		close(sock);

	if (basetime_filename)
		free(basetime_filename);
	if (file_name != default_file)
		free(file_name);
	if (interface)
		free(interface);
	if (ip_ptr)
		free(ip_ptr);
	if (win_config_filename)
		free(win_config_filename);
	if (selectFilter)
		free(selectFilter);

	return EXIT_SUCCESS;

err_out:
	if (sd != -1)
		close(sd);

	if (fp)
		fclose(fp);

	if (basetime_filename)
		free(basetime_filename);

	if (interface)
		free(interface);

	if (ip_ptr)
		free(ip_ptr);

	if (file_name != default_file)
		free(file_name);

	if (win_config_filename)
		free(win_config_filename);

	if (selectFilter)
		free(selectFilter);

	return EXIT_FAILURE;

rx_out:
	if (fp)
		fclose(fp);

	if (fgp)
		fclose(fgp);

	if (flat)
		fclose(flat);

	err = nice(0);
	if (err == -1)
		printf("nice() failed: (%s)\n", strerror(errno));

	/* Clean Tx buffers */
	if (fdTSN != -1)
		close(fdTSN);
	if (sd != -1)
		close(sd);
	if (sock != -1)
		close(sock);

	if (basetime_filename)
		free(basetime_filename);

	if (interface)
		free(interface);

	if (ip_ptr)
		free(ip_ptr);

	if (file_name != default_file)
		free(file_name);

	if (selectFilter)
		free(selectFilter);

	if (win_config_filename)
		free(win_config_filename);

	if (!pterr[0])
		pthread_cancel(t1);

	if (!pterr[1])
		pthread_cancel(t2);

	if (!pterr[2])
		pthread_cancel(t3);

	if ((rxDisplay & 0x01) == 0x01)
		if (msgctl(msqid, IPC_RMID, 0) == -1)
			fprintf(stderr, "msgctl(IPC_RMID) failed\n");

	if ((rxDisplay & 0x02) == 0x02)
		if (msgctl(msqgraph, IPC_RMID, 0) == -1)
			fprintf(stderr, "msgctl(IPC_RMID) failed\n");

	pthread_exit(NULL);

	return rc;
}
