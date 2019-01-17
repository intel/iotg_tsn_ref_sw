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

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "open62541.h"

#define DEFAULT_LOG	"timestamps.txt"
#define DELAY_USEC	10000
#define MAX_BUF_LINE	256
#define MAX_SERVERS	3

int running = 1;
int verbose;

#define LOG(x...)	do { if (verbose) fprintf(stderr, x); } while (0)

static char *get_name(char *base)
{
	char *name;

	/* Extract the name from base. */
	name = strrchr(base, '/');
	name = name ? name + 1 : base;

	return name;
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
			"Usage: %s servers [options]\n"
			"Servers:\n"
			"  List of OPC UA servers, separated by space\n"
			"  e.g. opc.tcp://localhost:4840\n"
			"Options:\n"
			"  -d value    Delay in usec between every read request.\n"
			"     Default: %d\n"
			"  -l name     Log file name.\n"
			"     Default: %s\n"
			"  -h          Print this message and exit.\n"
			"  -v          Verbose output\n"
			"",
			prog_name,
			DELAY_USEC,
			DEFAULT_LOG);
}

int main(int argc, char *argv[])
{
	int delay_s, delay_us, fd, i, m, n;
	struct timeval tv;
	char buf[MAX_BUF_LINE];
	char *log_fn, *prog_name;
	char *endpoint[MAX_SERVERS];

	UA_Client *client[MAX_SERVERS];
	UA_StatusCode retval;
	UA_Variant value;
	UA_String timestamp_str;

	/* Installing signal handlers. */
	signal(SIGHUP, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);

	delay_s = 0;
	delay_us = DELAY_USEC;
	log_fn = DEFAULT_LOG;
	prog_name = get_name(argv[0]);
	for (i = 0; i < MAX_SERVERS; i++) {
		endpoint[i] = NULL;
		client[i] = NULL;
	}

	/* Connect to servers (client) if the URL is valid. */
	for (m = n = 0, i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'd':
				if (++i < argc) {
					delay_us = strtol(argv[i], NULL, 0);
					LOG("%d: Set delay to %d usec\n", i, delay_us);
					break;
				}
				usage(prog_name);
				return -1;
			case 'l':
				if (++i < argc) {
					log_fn = argv[i];
					LOG("%d: Save the log to '%s'\n", i, log_fn);
					break;
				}
				usage(prog_name);
				return -1;
			case 'v':
				verbose++;
				break;
			default:
				usage(prog_name);
				return -1;
			}
		} else if (strncmp("opc.tcp://", argv[i], 10) == 0) {
			if (n < MAX_SERVERS) {
				LOG("Saving server#%d: %s\n", n, argv[i]);
				endpoint[n] = argv[i];
				m = ++n;
			} else {
				if (m == n) {
					fprintf(stderr, "Not enough buffer to store servers.\n"
							"Please consider increase the buffer size (%d).\n",
							m);
					fprintf(stderr, "The following servers are not connected:\n");
				}
				fprintf(stderr, "%d: Ignoring server#%d: %s\n", i, m, argv[i]);
				++m;
			}
		} else {
			fprintf(stderr, "%d: Ignoring server not start with \"opc.tcp://\": %s\n",
					i, argv[i]);
		}
	}
	if (n == 0) {
		fprintf(stderr, "No server is specified!\nExit now.\n");
		usage(prog_name);
		return 0;
	}

	LOG("Number of servers to connect: %d\n", n);
	for (m = i = 0; i < n; i++) {
		LOG("Connecting server#%d: %s\n", i, endpoint[i]);

		/* Create a client and connect. */
		client[m] = UA_Client_new(UA_ClientConfig_default);

		/* Connect with username would be:
		 * retval = UA_Client_connect_username(client, "opc.tcp://localhost:4840", "user1", "password");
		 */
		retval = UA_Client_connect(client[m], endpoint[i]);
		if (retval != UA_STATUSCODE_GOOD) {
			fprintf(stderr, "Unable to connect to server#%d: %s\n", i, endpoint[i]);
			UA_Client_delete(client[m]);
			client[m] = NULL;
			endpoint[i] = NULL;
			continue;
		}

		LOG("Server#%d connected: %s\n", m, endpoint[i]);
		++m;
	}
	n = m;
	LOG("Number of servers connected: %d\n", n);

	fd = open(log_fn, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		fprintf(stderr, "Fail to open log file '%s'\n", log_fn);
	else
		LOG("Writing log to '%s'\n", log_fn);

	delay_s = delay_us / 1000000;
	delay_us = delay_us % 1000000;

	UA_Variant_init(&value);

	/* NodeId of the variable holding the current time,
	 * set by the server in sample-app-1.c
	 */
	UA_NodeId nodeId = UA_NODEID_STRING(1, "time-stamp-datasource");

	while (running) {
		for (i = 0; i < n; i++) {

			retval = UA_Client_readValueAttribute(client[i], nodeId, &value);

			if (retval == UA_STATUSCODE_GOOD &&
			    UA_Variant_hasScalarType(&value, &UA_TYPES[UA_TYPES_STRING])) {

				timestamp_str = *(UA_String *) value.data;
				timestamp_str.data[timestamp_str.length] = 0;
				LOG("server#%d: %s\n", i, timestamp_str.data);

				if (fd >= 0) {
					snprintf(buf, sizeof(buf), "server#%d: %s\n",
						 i, timestamp_str.data);
					if (write(fd, buf, strlen(buf)) == -1)
						fprintf(stderr, "Error during write: %m\n");
				}
			}
		}

		/* Delay some time before next read. */
		tv.tv_sec = delay_s;
		tv.tv_usec = delay_us;
		select(1, NULL, NULL, NULL, &tv);
	}

	if (fd >= 0)
		close(fd);

	/* Disconnect from servers. */
	for (i = n - 1; i >= 0; i--) {
		UA_Client_disconnect(client[i]);
		UA_Client_delete(client[i]);
	}

	return 0;
}
