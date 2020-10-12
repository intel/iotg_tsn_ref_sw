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
#include <argp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include <signal.h>
#include <linux/ptp_clock.h>
#include <poll.h>
#include <fcntl.h>
#include <pthread.h>

#define BUFFER_SIZE 256
#define CLIENT_COUNT 2
#define DEFAULT_LISTENER_OUTFILE "tsq-listener-data.txt"
#define NULL_OUTFILE "NULL"

#define MODE_LISTENER 1
#define MODE_TALKER 2

int halt_sig;
FILE *glob_fp;
int glob_sockfd;
int glob_ptpfd;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void error(char *msg, ...)
{
	va_list argptr;

	if(glob_sockfd)
		close(glob_sockfd);

	if(glob_fp)
		fclose(glob_fp);

	if(glob_ptpfd)
		close(glob_ptpfd);

	va_start(argptr, msg);
	vfprintf(stderr, msg, argptr);
	va_end(argptr);
	exit(1);
}

/* Read shared signal variable */
int get_signal(void)
{
	int temp = 0, ret = 0;
	ret = pthread_mutex_lock(&lock);
	if (ret)
		fprintf(stderr, "[TSQ] Failed to lock halt_sig.\n");

	temp = halt_sig;

	ret = pthread_mutex_unlock(&lock);
	if (ret)
		fprintf(stderr, "[TSQ] Failed to unlock halt_sig.\n");
	return temp;
}

/* Signal handler */
void sigint_handler(int signum)
{
	int ret = 0;

	fprintf(stdout, "[TSQ] Thread exiting.\n");

	ret = pthread_mutex_lock(&lock);
	if (ret)
		fprintf(stderr, "[TSQ] Failed to lock halt_sig.\n");

	halt_sig = signum;

	ret = pthread_mutex_unlock(&lock);
	if (ret)
		fprintf(stderr, "[TSQ] Failed to unlock halt_sig.\n");

}

typedef struct payload {
	int uid;
	int seq;
	long long secs;
	long nsecs;
} payload;

/* User input options */
struct opt {
	char *args[1];
	int verbose;
	int mode;
	char *server_ip;
	int port;
	/* Listener */
	char *output_file;
	/* Talker */
	int uid;
	char *device;
	int timeout;
};

static struct argp_option options[] = {
	/* Shared */

	{"talker",  'T', 0,       0, "Talker mode (read AUXTS)"},
	{"listener",'L', 0,       0, "Listener mode (listen for 2 talker and compare)"},

	{"verbose", 'v', 0,       0, "Produce verbose output"},
	{"ip",      'i', "ADDR",  0, "Server IP address (eg. 192.1.2.3)"},
	{"port",    'p', "PORT",  0, "Port number\n"
				     "	Def: 5678 | Min: 999 | Max: 9999"},

	/* Listener-specific */
	{"output",  'o', "FILE",  0, "Save output to FILE (eg. temp.txt)"},

	/* Talker-specific */
	{"device",  'd', "FILE",  0, "PTP device to read (eg. /dev/ptp1)"},
	{"uid",     'u', "COUNT", 0, "Unique Talker ID"
				     "	Def: 1234 | Min: 999 | Max: 9999"},
	{"timeout", 't', "MSEC",  0, "Polling timeout in ms"
				     "	Def: 1100ms | Min: 1ms | Max: 2000ms"},

	{ 0 }
};

static error_t parser(int key, char *arg, struct argp_state *state)
{
	/* Get the input argument from argp_parse, which we */
	/* know is a pointer to our user_opt structure. */
	struct opt *user_opt = state->input;

	switch (key) {
	case 'v':
		user_opt->verbose = 1;
		break;
	case 'T':
		user_opt->mode = MODE_TALKER;
		break;
	case 'L':
		user_opt->mode = MODE_LISTENER;
		break;
	case 'i':
		user_opt->server_ip = arg;
		break;
	case 'p':
		user_opt->port = atoi(arg);
		if (user_opt->port < 999 || user_opt->port > 9999)
			error("Invalid port number. Check --help");
		break;
	case 'o':
		user_opt->output_file = arg;
		break;
	case 'u':
		user_opt->uid = atoi(arg);
		if (user_opt->uid < 999 || user_opt->uid > 9999)
			error("Invalid UID. Check --help");
		break;
	case 'd':
		user_opt->device = arg;
		break;
	case 't':
		user_opt->timeout = atoi(arg);
		if (user_opt->timeout < 1 || user_opt->timeout > 2000)
			error("Invalid timeout. Check --help");
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static char usage[] = "-i <IP_ADDR> -p <PORT> -d /dev/ptp<X> -u <UID> {-T|-L}";

static char summary[] = "Time Sync Quality Measurement application";

static struct argp argp = { options, parser, usage, summary };

/**
 *  @brief Validate payload structure received
 *
 *  @param pl	pointer to payload received
 *  @param cli_ids
 *  @return validation boolean result
 *
 */
bool validate_payload (payload *pl, int *cli_ids, int client_num)
{
	bool ret = false;

	if ((pl->uid != 0) && (pl->secs != 0) && (pl->nsecs != 0)) {
		for (int i = 0 ; i < client_num ; i++) {
			if (cli_ids[i] == pl->uid) {
				ret = true;
				break;
			}
		}
	}
	return ret;
}

/**
 *  @brief Check if all the data is received/ready for each client
 *
 *  @param pl	pointer to data ready flags table
 *  @param size	size of data ready flags table
 *  @return true if data is now all available
 */
bool data_ready(bool *flags, int size)
{
	for (int i = 0 ; i < size ; i++) {
		if (*flags == false)
			return false;
		flags++;
	}
	return true;
}

/**
 *  @brief Reset data ready flags table
 *
 *  @param pl	pointer to data ready flags table
 *  @param size	size of data ready flags table
 */
void reset_data_ready(bool *flags, int size)
{
	for (int i = 0; i < size; i++) {
		*flags = false;
		flags++;
	}
}

/* Listener - wait for 2 talkers to connect and receive. Compare both talker
 * timestamps to get the delta and transmit out.
 * TODO: Expand it to support 4 talkers at the same time?
 */
void listener(struct opt *user_opt) {

	char *server_ip;
	struct sockaddr_in serv;
	socklen_t len;
	int rounds = 0;
	int connfd = 0;
	int n = 0;
	int i = 0;
	int verbose;
	int max_sd = 0;
	int activity = 0;
	char recv_buff[BUFFER_SIZE];
	long long secs_error;
	long nsecs_error;
	int cli_ids[CLIENT_COUNT];
	int cli_conns[CLIENT_COUNT] = {};
	payload cli_data[CLIENT_COUNT];
	payload temp_data, keep_data;
	fd_set readfds;
	bool pl_valid_flag[CLIENT_COUNT] = {false, false};
	bool skip_slot = false;
	int keep_data_slot = 0;

	memset(&keep_data, 0, sizeof(payload));

	if(user_opt == NULL)
		error("[TSQ-L] User option is NULL");

	verbose = user_opt->verbose;

	server_ip = user_opt->server_ip;

	glob_fp = fopen(user_opt->output_file, "w");
	if (glob_fp == NULL)
		error("[TSQ-L] Error opening file: %s\n", user_opt->output_file);

	printf("[TSQ-L] Saving output in %s\n", user_opt->output_file);

	// Create listener socket
	glob_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (glob_sockfd < 0)
		error("[TSQ-L] Error opening listening socket\n");

	// connection settings
	memset(&serv, '0', sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = inet_addr(server_ip);
	if (user_opt->port != 0)
		serv.sin_port = htons(user_opt->port);
	else
		serv.sin_port = 0;

	// bind the socket and start listening
	if (bind(glob_sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
		error("[TSQ-L] Error binding listening socket\n");

	listen(glob_sockfd, 2);

	len = sizeof(serv);
	if (getsockname(glob_sockfd, (struct sockaddr *)&serv, &len) == -1)
		error("[TSQ-L] getsockname() failed");
	else
		if (verbose)
			printf("[TSQ-L] Started listening on %s:%d\n",
			server_ip, ntohs(serv.sin_port));

	// Waiting for CLIENT_COUNT connections
	i = 0;
	while (i < CLIENT_COUNT && get_signal() == 0) {

		connfd = accept(glob_sockfd, (struct sockaddr *)NULL, NULL);

		if (connfd < 0) {
			printf("[TSQ-L] Error accepting connection. Waiting for the next one\n");
			continue;
		}

		if (verbose)
			printf("[TSQ-L] Accept a connection. glob_sockfd[%d]:%d\n", i, connfd);

		bzero(recv_buff, BUFFER_SIZE);
		n = read(connfd, recv_buff, sizeof(recv_buff) - 1);

		if (n < 0) {
			//close the connection in case failed read
			close(connfd);
			error("[TSQ-L] ERROR reading UID from socket\n");
		}

		cli_ids[i] = atoi(recv_buff);
		if (verbose)
			printf("[TSQ-L] Connected with client_id:%d\n", cli_ids[i]);
		cli_conns[i] = connfd;
		i++;
	}

	// clients connected. Send command to start broadcast
	for (int i = 0; i < CLIENT_COUNT; i++) {

		connfd = cli_conns[i];
		bzero(recv_buff, BUFFER_SIZE);
		snprintf(recv_buff, sizeof(recv_buff), "start\n");
		n = write(connfd, recv_buff, sizeof(recv_buff));
		if (n < 0)
			error("[TSQ-L] Error writing to client socket.\n");
	}
	memset(&cli_data, 0, sizeof(cli_data));

	while (get_signal() == 0) {

		// write to file everytime
		fflush(glob_fp);

		FD_ZERO(&readfds);
		max_sd = 0;

		for (int i = 0; i < CLIENT_COUNT; i++) {
			FD_SET(cli_conns[i], &readfds);
			if (cli_conns[i] > max_sd)
				max_sd = cli_conns[i];
		}

		/*
		 * wait for an activity on one of the sockets , timeout is NULL,
		 * so wait indefinitely
		 */
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

		if ((activity < 0) && (errno != EINTR))
			error("[TSQ-L] Select error\n");

		for (int i = 0; i < CLIENT_COUNT; i++) {

			bzero(recv_buff, BUFFER_SIZE);
			memset(&temp_data, '0', sizeof(temp_data));
			connfd = cli_conns[i];

			if (FD_ISSET(connfd, &readfds)) {
				if (get_signal() != 0)
					error("[TSQ-L] client socket closed. TSQ will end now.\n");

				n = read(connfd, recv_buff, sizeof(recv_buff) - 1);

				if (n < 0)
					error("[TSQ-L] ERROR reading socket. Exiting.\n");
				/*
				 * If the slot is marked for keeping for sample alignment
				 * we will recopy the previous value and not use the one read from the socket.
				 */
				if ((skip_slot == true) && (keep_data_slot == i)) {
					memcpy(&cli_data[i], &keep_data, sizeof(payload));
					pl_valid_flag[i] = true;

					/*
					 * if (verbose)
					 *	printf("[TSQ-L] KEEP SLOT data from %d : Secs : %lld, Nsecs : %ld , seq : %d\n",
					 *	cli_data[i].uid, cli_data[i].secs, cli_data[i].nsecs , cli_data[i].seq);
					 */

				} else {
					if (n > 0) {  // copy the new value
						memcpy(&temp_data, recv_buff, sizeof(payload));
						if (validate_payload (&temp_data, cli_ids, CLIENT_COUNT)) {
							pl_valid_flag[i] = true;
							memcpy(&cli_data[i], &temp_data, sizeof(payload));
							/*if (verbose)
								printf("[TSQ-L] Msg from %d : Secs : %lld, Nsecs : %ld , seq : %d\n",
								cli_data[i].uid, cli_data[i].secs, cli_data[i].nsecs, cli_data[i].seq); */
						}
					}
				}
			}
		}

		if (data_ready(pl_valid_flag, CLIENT_COUNT)) {
			secs_error = cli_data[0].secs - cli_data[1].secs;
			nsecs_error = cli_data[0].nsecs - cli_data[1].nsecs;

			if (verbose)
				printf("[TSQ-L] Msg from %d, %d: Secs off: %lld, Nsecs off: %ld\n",
				cli_data[0].uid, cli_data[1].uid, secs_error, nsecs_error);
			/*
			 * 1. When secs_error is equals to 1, t0 sample is 'newer' compared to t1.
			 * This means the t0 sample is not align to t1 sample, we shd keep t0, discard current t1, and reread t1.
			 * 2. When secs_error is equals to -1, t1 sample is now 'newer' compare to t0.
			 * The t1 sample is not align to t0 sample, we shd keep t1, discard current t0, and reread t0.
			 * 3. When secs_error is 0 , the samples are aligned. The nsecs is then valid.
			 * 4. When secs_error are not ( 0 | 1 |-1) , the samples are misaligned , and we wait for alignment in future samples.
			 * In this case, we discard nsecs_errors.
			 */

			switch (secs_error) {

			case 1:
				skip_slot = true;
				keep_data_slot = 0;
				memcpy(&keep_data, &cli_data[0], sizeof(payload));
				break;

			case -1:
				skip_slot = true;
				keep_data_slot = 1;
				memcpy(&keep_data, &cli_data[1], sizeof(payload));
				break;

			case 0:
				skip_slot = false;
				memset(&keep_data, '0', sizeof(payload));
				fprintf(glob_fp, "%d %lld %ld\n", rounds, secs_error, nsecs_error);
				rounds++;
				break;

			default:
				skip_slot = false;
				memset(&keep_data, '0', sizeof(payload));
				printf("[TSQ-L] Waiting for samples alignment...\n");
				break;
			}

			reset_data_ready(pl_valid_flag, CLIENT_COUNT);
			memset(&cli_data, 0, sizeof(cli_data));

		}
	}

	close(glob_sockfd);
	fclose(glob_fp);
}

void talker(struct opt *user_opt){
	char *server_ip;
	int verbose;
	int uid;
	int port;
	struct sockaddr_in serv;
	char send_buff[BUFFER_SIZE];
	int n;
	payload data;
	int seq = 0;
	char *device;
	int timeout_ms;
	struct pollfd pfd;
	struct ptp_extts_event e;
	int ready;
	int ret;

	if(user_opt == NULL)
		error("[TSQ-T] User option is NULL");

	server_ip = user_opt->server_ip;
	port = user_opt->port;
	device = user_opt->device;
	verbose = user_opt->verbose;
	uid = user_opt->uid;
	glob_sockfd = 0;

	if (verbose)
		printf("[TSQ-T] Assigned uid %d\n", uid);

	/* Set up the socket for transmission */
	memset(&serv, '0', sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = inet_addr(server_ip);
	serv.sin_port = htons(port);

	glob_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (glob_sockfd < 0)
		error("[TSQ-T] Could not create socket\n");

	if (connect(glob_sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
		error("[TSQ-T] Connect Failed\n");

	if (verbose)
		printf("[TSQ-T] Connection established with %s\n", server_ip);

	bzero(send_buff, BUFFER_SIZE);

	// send uid
	snprintf(send_buff, sizeof(send_buff), "%d\n", uid);
	ret = write(glob_sockfd, send_buff, sizeof(send_buff));
	if (ret < 0)
		error("[TSQ-T] ERROR writing UID to socket\n");

	if (verbose)
		printf("[TSQ-T] Sent %s to tsq-listener\n", send_buff);

	bzero(send_buff, BUFFER_SIZE);
	n = read(glob_sockfd, send_buff, sizeof(send_buff) - 1);

	if (n < 0)
		error("[TSQ-T] ERROR reading command from socket\n");

	if (strcmp(send_buff, "start\n") != 0)
		error("[TSQ-T] Connection aborted\n");

	if (verbose)
		printf("[TSQ-T] Reading from %s\n", device);

	glob_ptpfd = open(device, O_RDWR);
	if (glob_ptpfd < 0) 
		error("[TSQ-T] ERROR to open ptp device %s\n", device);
	if (verbose)
		printf("[TSQ-T] PTP device : %s is now opened\n", device);

	pfd.fd = glob_ptpfd;
	pfd.events = PTP_PF_EXTTS;
	pfd.revents = 0;

	timeout_ms = user_opt-> timeout;
	if (verbose)
		printf("[TSQ-T] Setting timeout to %dms\n", timeout_ms);

	while (get_signal() == 0) {
		ready = poll(&pfd, 1, timeout_ms);
		if (ready < 0)
			error("[TSQ-T] Failed to poll\n");
		e.t.sec = 0;
		e.t.nsec = 0;

		while (ready-- > 0) {
			n = read(glob_ptpfd, &e, sizeof(e));
			if (n != sizeof(e)) {
				error("[TSQ-T] read returns %lu bytes, expecting %lu bytes\n",
					n, sizeof(e));
			}
		}
		data.uid = uid;
		data.seq = seq;
		data.secs = e.t.sec;
		data.nsecs = e.t.nsec;

		bzero(send_buff, BUFFER_SIZE);
		memcpy(send_buff, &data, sizeof(data));

		/*
		 * If secs / nsecs turns out to be zero, it wont be sent over,
		 * because the values read are not valid.
		 */
		if ( (data.secs != 0) && (data.nsecs != 0) ) {
			if (verbose)
				printf("[TSQ-T:%d] Sending %d: %lld#%ld\n",
					uid, seq, data.secs, data.nsecs);

			ret = write(glob_sockfd, send_buff, sizeof(send_buff));
			if (ret < 0)
				error("[TSQ-T] Erorr writing to socket.\n");

			seq++;
		} else {
			if (verbose)
				printf ("[TSQ-T] sec:%lld nsec:%ld seq: %d\n",
					 data.secs, data.nsecs, seq);
		}
	}

	close(glob_sockfd);
}

int main(int argc, char *argv[])
{
	struct opt user_opt;

	/* Defaults */
	user_opt.mode = 0;
	user_opt.verbose = 0;
	user_opt.device = "\0";
	user_opt.timeout = 1100; // ms
	user_opt.uid = 1234;
	user_opt.port = 5678;
	user_opt.output_file = DEFAULT_LISTENER_OUTFILE;
	glob_fp = NULL;
	glob_ptpfd = -1;
	halt_sig = 0;

	argp_parse(&argp, argc, argv, 0, 0, &user_opt);

	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGABRT, sigint_handler);

	switch(user_opt.mode){
	case MODE_TALKER:
		/* IP, PORT, UID, device */
		talker(&user_opt);
		break;
	case MODE_LISTENER:
		/* IP (itself), PORT*/
		listener(&user_opt);
		break;
	default:
		error("Invalid mode selected\n");
		break;
	}

	return 0;
}
