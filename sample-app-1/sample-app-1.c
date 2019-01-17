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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include <linux/ptp_clock.h>
#include <linux/version.h>
#include <regex.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0))
	#define HAS_PTP_1588_CLOCK_PINS
#endif

#define DAEMON_NAME		"daemon_cl"
#define PTP_DEVICE		"/dev/ptp0"
#define DEFAULT_ETS_CHAN	1
#define DEFAULT_ETS_INDEX	1
#define DEFAULT_PPS_CHAN	0
#define DEFAULT_PPS_INDEX	0
#define MAX_CHANNELS		2
#define MAX_INDEXES		4

#include "open62541.h"

/* These macros are defined in linux/posix-timers.h, but not able to
 * include the file.
 */
#define CPUCLOCK_MAX		3
#define CLOCKFD			CPUCLOCK_MAX
#define FD_TO_CLOCKID(fd)	((~(clockid_t) (fd) << 3) | CLOCKFD)
#define DEFAULT_OPC_UA_PORT	4840

UA_Boolean running = 1;

int verbose;
int globptp;

#define LOG(x...)	do { if (verbose) fprintf(stderr, x); } while (0)

static UA_StatusCode
read_ets(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
	const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
	const UA_NumericRange *range, UA_DataValue *value)
{
	struct ptp_extts_event e;
	struct pollfd pfd;
	unsigned long n;
	int ready, timeout_ms;
	char buf[32];

	int fd =  globptp;

	if (range) {
		value->hasStatus = true;
		value->status = UA_STATUSCODE_BADINDEXRANGEINVALID;
		return UA_STATUSCODE_GOOD;
	}

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	timeout_ms = 100;

	ready = poll(&pfd, 1, timeout_ms);
	if (ready < 0) {
		fprintf(stderr, "Failed to poll: %d (%s)\n", errno, strerror(errno));
		return -1;
	} else if (ready == 0) {
		value->hasValue = false;
		return UA_STATUSCODE_GOOD;
	}

	while (ready-- > 0) {
		n = read(fd, &e, sizeof(e));
		if (n != sizeof(e)) {
			fprintf(stderr, "read returns %lu bytes, expecting %lu bytes\n",
				n, sizeof(e));
			return -1;
		}

		LOG("%d event index %d at %lld.%09u\n", ready,
		    e.index, e.t.sec, e.t.nsec);
	}
	snprintf(buf, sizeof(buf), "%lld.%09u", e.t.sec, e.t.nsec);

	UA_String string = UA_String_fromChars(buf);

	UA_Variant_setScalarCopy(&value->value, &string,
				 &UA_TYPES[UA_TYPES_STRING]);

	value->hasValue = true;
	if (sourceTimeStamp) {
		value->hasSourceTimestamp = true;
		value->sourceTimestamp = UA_DateTime_now();
	}

	return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
write_ets(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
		 const UA_NodeId *nodeId, void *nodeContext,
		 const UA_NumericRange *range, const UA_DataValue *data)
{       /* Dummy implementation to avoid BadWriteNotSupported error */
	return UA_STATUSCODE_GOOD;
}

static int get_ets(int fd, int index)
{
	struct ptp_extts_request req;

	/* Init the server. */
	UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_default,
							    DEFAULT_OPC_UA_PORT, NULL);
	UA_ServerConfig *config = UA_ServerConfig_new_default();

	config->networkLayers = &nl;
	config->networkLayersSize = 1;
	UA_Server *server = UA_Server_new(config);

	/* Add a variable datasource node. */
	/* 1) Set the variable attributes. */
	UA_VariableAttributes attr = UA_VariableAttributes_default;

	attr.displayName = UA_LOCALIZEDTEXT("en_US", "Time stamp datasource");
	attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

	/* 2) Define where the variable shall be added with which browse name */
	UA_NodeId new_node_id = UA_NODEID_STRING(1, "time-stamp-datasource");
	UA_NodeId parent_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
	UA_NodeId parent_ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
	UA_NodeId variable_type = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
	UA_QualifiedName browse_name = UA_QUALIFIEDNAME(1, "Time stamp datasource");

	UA_DataSource data_source;

	data_source.read = read_ets;
	data_source.write = write_ets;

	/* 3) Add the variable with data source. */
	UA_Server_addDataSourceVariableNode(server, new_node_id,
					    parent_node_id,
					    parent_ref_node_id,
					    browse_name,
					    variable_type, attr,
					    data_source,
					    NULL, NULL);

	memset(&req, 0, sizeof(req));
	req.index = index;
	req.flags = PTP_ENABLE_FEATURE;
	if (ioctl(fd, PTP_EXTTS_REQUEST, &req)) {
		perror("PTP_EXTTS_REQUEST enable");
		UA_Server_delete(server);
		nl.deleteMembers(&nl);
		return -1;
	}

	/* Run the server loop. */
	UA_Server_run(server, &running);

	/* Disable the external time stamp feature. */
	req.flags = 0;
	if (ioctl(fd, PTP_EXTTS_REQUEST, &req)) {
		perror("PTP_EXTTS_REQUEST disable");
		UA_Server_delete(server);
		nl.deleteMembers(&nl);
		return -1;
	}
	UA_Server_delete(server);
	nl.deleteMembers(&nl);

	return 0;
}

static int set_pps(int fd, clockid_t clk_id, int index, int period_s)
{
	struct ptp_perout_request req;
	struct timespec ts;

	if (clock_gettime(clk_id, &ts)) {
		perror("clock_gettime");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.index = index;
	req.start.sec = ts.tv_sec + 2;
	req.period.sec = period_s;
	if (ioctl(fd, PTP_PEROUT_REQUEST, &req)) {
		perror("PTP_PEROUT_REQUEST");
		return -1;
	}

	return 0;
}

static int config_ptp(int fd, const char *dev, int c_ets,
		      int i_ets, int c_pps, int i_pps)
{
	struct ptp_clock_caps caps;

	if (ioctl(fd, PTP_CLOCK_GETCAPS, &caps)) {
		fprintf(stderr, "Failed to get PTP clock capabilities: %d (%s)\n",
			errno, strerror(errno));
		return -1;
	}
#ifdef HAS_PTP_1588_CLOCK_PINS
	struct ptp_pin_desc desc;

	LOG("%s capabilities:\n"
		"  Number of SDP: %d\n"
		"  Number of programmable periodic signals: %d\n"
		"  Number of external time stamp channels : %d\n",
		dev, caps.n_pins, caps.n_per_out, caps.n_ext_ts);

	if ((caps.n_pins < 2) || (caps.n_per_out == 0) ||
	    (caps.n_ext_ts == 0)) {
		fprintf(stderr, "Not enough SDP for PPS and time stamping\n");
		return -1;
	}

	/* Configure SDP for periodic output. */
	memset(&desc, 0, sizeof(desc));
	desc.chan = c_pps;
	desc.index = i_pps;
	desc.func = PTP_PF_PEROUT;
	if (ioctl(fd, PTP_PIN_SETFUNC, &desc)) {
		perror("PTP_PIN_SETFUNC PPS");
		return -1;
	}
	LOG("SDP%d channel %d configured for periodic output\n", i_pps, c_pps);

	/* Configure SDP for time stamp external signal. */
	memset(&desc, 0, sizeof(desc));
	desc.chan = c_ets;
	desc.index = i_ets;
	desc.func = PTP_PF_EXTTS;
	if (ioctl(fd, PTP_PIN_SETFUNC, &desc)) {
		perror("PTP_PIN_SETFUNC ETS");
		return -1;
	}
	LOG("SDP%d channel %d configured for external time stamp signal\n",
	    i_ets, c_ets);
#else /* ndef HAS_PTP_1588_CLOCK_PINS */
	LOG("%s capabilities:\n"
		"  Number of programmable periodic signals: %d\n"
		"  Number of external time stamp channels : %d\n",
		dev, caps.n_per_out, caps.n_ext_ts);

	if ((caps.n_per_out == 0) || (caps.n_ext_ts == 0)) {
		fprintf(stderr, "Not enough SDP for PPS and time stamping\n");
		return -1;
	}
#endif /* ndef HAS_PTP_1588_CLOCK_PINS */

	return 0;
}

static char *get_name(char *base)
{
	char *name;

	/* Extract the name from base. */
	name = strrchr(base, '/');
	name = name ? name + 1 : base;

	return name;
}

static pid_t get_proc_pid(const char *name)
{
	FILE *file;
	char *endptr;
	char buf[512];
	pid_t found = -1;

	file = popen("pgrep daemon_cl", "r");
	if (file) {
		if (fgets(buf, sizeof(buf), file) != NULL)
			found = strtol(buf, &endptr, 10);
		pclose(file);
		free(file);
	}
	return found;
}

static void sig_handler(int sig)
{
	/* Reset signal. */
	signal(sig, sig_handler);
	LOG("Received signal %d\n", sig);
	running = 0;
}

static void usage(char *prog_name)
{
	fprintf(stderr,
			"Usage: %s [options]\n"
			"Options:\n"
			"  -d name     PTP device to open.\n"
			"     Default: %s\n"
			"  -g name     gPTP daemon name.\n"
			"     Default: %s\n"
			"  -h          Print this message and exit.\n"
			"  -v          Verbose output\n"
			"",
			prog_name,
			PTP_DEVICE,
			DAEMON_NAME);
}

/*
 * Main function for the application.
 */
int main(int argc, char *argv[])
{
	char *daemon, *prog_name, *ptp_dev;
	clockid_t clk_id;
	pid_t daemon_pid;
	int c_ets, i_ets;
	int c_pps, i_pps;
	int fd, opt;

	/* Installing signal handlers. */
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);

	daemon = DAEMON_NAME;
	prog_name = get_name(argv[0]);
	ptp_dev = PTP_DEVICE;

	c_ets = DEFAULT_ETS_CHAN;
	i_ets = DEFAULT_ETS_INDEX;
	c_pps = DEFAULT_PPS_CHAN;
	i_pps = DEFAULT_PPS_INDEX;

	/* Parse command line arguments. */
	while ((opt = getopt(argc, argv, "d:g:hv")) != EOF) {
		switch (opt) {
		case 'd':
			ptp_dev = optarg;
			break;
		case 'g':
			daemon = optarg;
			break;
		case 'h':
			usage(prog_name);
			return 0;
		case 'v':
			verbose++;
			break;
		default: /* '?' */
			usage(prog_name);
			return -1;
		}
	}

	LOG("Hello from %s!\n", prog_name);

	/* Get daemon_cl process ID */
	daemon_pid = get_proc_pid(daemon);
	LOG("gPTP daemon: %s\nPID: %d\n", daemon, daemon_pid);
	if (daemon_pid == -1) {
		/* Just print warning but still let the program continue. */
		fprintf(stderr,
			"%s is not running!\nThere is no time sync.\n",
			daemon);
	}

	/* Open ptp clock */
	fd = open(ptp_dev, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %d (%s)\n",
			ptp_dev, errno, strerror(errno));
		return -1;
	}
	LOG("PTP device: %s\n", ptp_dev);
	globptp = fd;
	/* get clock_id from ptp clock */
	clk_id = FD_TO_CLOCKID(fd);

	/* configure PTP clock */
	if (config_ptp(fd, ptp_dev, c_ets, i_ets, c_pps, i_pps)) {
		fprintf(stderr, "Failed to configure %s: %d (%s)\n",
			ptp_dev, errno, strerror(errno));
		close(fd);
		return -1;
	}

	/* set package per second */
	if (set_pps(fd, clk_id, i_pps, 1)) {
		fprintf(stderr, "Cannot start PPS at SDP%d\n", i_pps);
		close(fd);
		return -1;
	}
	LOG("PPS started at SDP%d\n", i_pps);

	LOG("Reading external time stamps at SDP%d\n", i_ets);
	if (get_ets(fd, i_ets)) {
		fprintf(stderr, "Cannot get external time stamps at SDP%d\n",
			i_ets);
		/* Fall through for cleanup. */
	}

	/* Cleanup. */
	if (set_pps(fd, clk_id, i_pps, 0)) {
		fprintf(stderr, "Cannot stop PPS at SDP%d\n", i_pps);
		close(fd);
		return -1;
	}
	LOG("PPS stopped at SDP%d\n", i_pps);

	close(fd);
	LOG("Bye from %s!\n", prog_name);

	return 0;
}
