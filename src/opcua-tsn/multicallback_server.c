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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/pubsub_ethernet_etf.h>
#include <open62541/plugin/pubsub_ethernet_xdp.h>
#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "opcua_common.h"
#include "opcua_publish.h"
#include "opcua_subscribe.h"
#include "opcua_custom.h"
#include "opcua_datasource.h"

#define VERBOSE 0
#define MSG_KEY_1 3838

/* Globals */
struct threadParams g_thread[MAX_OPCUA_THREAD];
UA_UInt16 g_threadRun;
UA_Boolean g_running = true;
UA_Boolean g_roundtrip_pubReturn = true;
UA_NodeId g_writerGroupIdent;
struct ServerData *g_sData;
int verbose = VERBOSE;

static void stopHandler(int sign)
{
    (void) sign;
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    g_running = false;
}

int configureServer(struct ServerData *sdata)
{
    struct timespec ts;

    int ret = clock_gettime(CLOCK_TAI, &ts);
    catch_err(ret == -1, "Failed to get current time");

    /* Initialize server start time to +3 secs from now */
    sdata->startTime = (UA_UInt64) ((ts.tv_sec + 3) * 1E9L);

    sdata->transportProfile =
        UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-eth-uadp");

    sdata->msqid = -1;

    for (int i = 0; i < sdata->pubCount; i++) {
        struct PublisherData *pub = &sdata->pubData[i];

        pub->writeFunc = &dummyDSWrite;

        if( pub->twoWayData == false) {
            pub->readFunc = &pubGetDataToTransmit;
        } else {
            pub->readFunc = &pubReturnGetDataToTransmit;

            /* Don't start pubReturn until valid data available */
            g_roundtrip_pubReturn = false;

            /* Setup message queue IPC */
            sdata->msqid = msgget((key_t)MSG_KEY_1, 0644 | IPC_CREAT);
            if (sdata->msqid < 0)
                log_error("msgget() failed with error\n");

            if (msgctl(sdata->msqid, IPC_STAT, &sdata->mqbuf1) < 0)
                log_error("msgctl(IPC_STAT) failed\n");

            memset(&sdata->mqbuf1, 0, sizeof(sdata->mqbuf1));

            sdata->mqbuf1.msg_qbytes = sizeof(sdata->mqbuf1) * 24;

            if (msgctl(sdata->msqid, IPC_SET, &sdata->mqbuf1) < 0)
                log_error("msgctl(IPC_SET) failed\n");

        }
    }

    for (int i = 0; i < sdata->subCount; i++) {
        struct SubscriberData *sub = &sdata->subData[i];
        sub->readFunc = &dummyDSRead;
        if ( sub->twoWayData == false)
            sub->writeFunc = &subStoreDataReceived;
        else
            sub->writeFunc = &subReturnStoreDataReceived;
    }

    return 0;

error:
    return -1;
}

static UA_Server *setupOpcuaServer(struct ServerData *sdata)
{
    UA_NodeId connId = {};
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    struct PublisherData *pub = NULL;
    struct SubscriberData *sub = NULL;
    int ret = 0;

    UA_ServerConfig_setMinimal(config, getpid(), NULL);

    /* Setup transport layers through UDPMP and Ethernet */
    config->pubsubTransportLayersSize = 0;
    config->pubsubTransportLayers =
        (UA_PubSubTransportLayer *) UA_calloc(2, sizeof(UA_PubSubTransportLayer));
    catch_err(config->pubsubTransportLayers == NULL, "Out of memory");

    if (sdata->useXDP)
        config->pubsubTransportLayers[0] = UA_PubSubTransportLayerEthernetXDP();
    else
        config->pubsubTransportLayers[0] = UA_PubSubTransportLayerEthernetETF();

    config->pubsubTransportLayersSize++;

    for (int i = 0; i < sdata->pubCount; i++) {
        pub = &sdata->pubData[i];
        addPubSubConnection(server, &connId, sdata, pub);
        ret = createPublisher(server, sdata, pub, &connId);
        catch_err(ret == -1, "createPublisher() returned -1");
    }

    for (int i = 0; i < sdata->subCount; i++) {
        sub = &sdata->subData[i];

        /* Skip adding a new socket (PubSubConnection)
         * using XDP and already have a publisher on the same interface's queue.
         * This is because XDP sockets are bi-directional, so both
         * TX/RX queue is bound together to a single socket.
         * Uni direction sockets are available in 5.5+ kernels only and
         * requires a different bpf program.
         */
        if (sdata->pubCount && sdata->useXDP && sub->xdpQueue == pub->xdpQueue &&
                strcmp(sdata->pubInterface, sdata->subInterface) == 0) {
            log("Round trip AF_XDP same queue, same interface, sharing layer");
        } else if (sdata->pubCount && sdata->useXDP) {
            log("Round trip AF_XDP different queue use new layer");
            config->pubsubTransportLayers[1] = UA_PubSubTransportLayerEthernetXDP();
            config->pubsubTransportLayersSize++;
            addSubConnection(server, &connId, sdata, sub);
        } else if (sdata->pubCount) {
            log("Round trip AF_PACKET use new layer");
            config->pubsubTransportLayers[1] = UA_PubSubTransportLayerEthernetETF();
            config->pubsubTransportLayersSize++;
            addSubConnection(server, &connId, sdata, sub);
        } else {
            log("Single trip AF_XDP or AF_PACKET addSubConnection only");
            addSubConnection(server, &connId, sdata, sub);
        }

        ret = createSubscriber(server, sdata, sub, &connId);
        catch_err(ret == -1, "createSubscriber() returned -1");
    }

    catch_err(ret < 0, "createSubscriber() returned < 0");

    return server;

error:
    UA_Server_delete(server);
    return NULL;
}


void copy_file(char *src_file, char *dst_file, bool clear_src)
{
	char ch;
	FILE *src, *dst;

	/* Open source file for reading */
	src = fopen(src_file, "r");
	if (src == NULL)
		return;

	/* Open destination file for writing in append mode */
	dst = fopen(dst_file, "w");
	if (dst == NULL) {
		fclose(src);
		return;
	}

	/* Copy content from source file to destination file */
	while ((ch = fgetc(src)) != EOF)
		fputc(ch, dst);

	fclose(src);
	fclose(dst);

	/* Clear source file */
	if (clear_src)
		fclose(fopen(src_file, "w"));

	return;
}


void ts_log_start()
{
	copy_file("/var/log/ptp4l.log", "/var/log/total_ptp4l.log", 1);
	copy_file("/var/log/phc2sys.log", "/var/log/total_phc2sys.log", 1);
}


void ts_log_stop()
{
	copy_file("/var/log/ptp4l.log", "/var/log/captured_ptp4l.log", 0);
	copy_file("/var/log/phc2sys.log", "/var/log/captured_phc2sys.log", 0);
}


int main(int argc, char **argv)
{
    (void) argc;

    int i, ret = 0;
    UA_UInt16 threadCount = 0;
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    struct ServerData *sdata = parseArgs(argv);
    catch_err(sdata == NULL, "parseArgs() returned NULL");

    ret = configureServer(sdata);
    catch_err(ret == -1, "configureServer() returned -1");

    threadCount = sdata->pubCount + sdata->subCount;
    catch_err(threadCount > MAX_OPCUA_THREAD, "Max total opcua pub+sub is exceeded !!. Exiting.");
    g_threadRun = 0;

    g_sData = sdata;

    /* Pub/sub threads will be started by the WG/RG addRepeatedCallback() */
    UA_Server *server = setupOpcuaServer(sdata);
    catch_err(server == NULL, "setupOpcuaServer() returned NULL");

    UA_StatusCode servrun = UA_Server_run(server, &g_running);

    /* Pub/sub threads are started by the server. Wait till they finish. */
    for (i = 0; i < g_threadRun; i++) {
        ret = pthread_join(g_thread[i].id, NULL);
        if (ret != 0)
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                        "\nPthread Join Failed:%ld\n", g_thread[i].id);
    }

    ts_log_stop();

    /* Clear msgq */
    if (g_sData->msqid >= 0){
        if (msgctl(g_sData->msqid, IPC_RMID, 0) < 0)
            log_error("Failed to remove IPC");
        else
            log("Successfully removed IPC");
    }

    /* Teardown and free any malloc-ed memory (incl those by strndup) */
    UA_Server_delete(server);

    free_resources(sdata);

    return servrun == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;

error:
    exit(-1);
}
