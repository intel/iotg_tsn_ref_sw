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
#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <json-c/json.h>
#include <assert.h>
#include <errno.h>

#include "opcua_common.h"
#include "opcua_datasource.h"
#include "json_helper.h"

extern struct ServerData *g_sData;
extern UA_Boolean g_running, g_roundtrip_pubReturn;
/* PubReturn only */
static UA_Int64 tx_sequence = -2; //TODO
static UA_UInt64 prev_rx_sequence = 999;

UA_StatusCode
pubGetDataToTransmit(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, UA_Boolean sourceTimeStamp,
                     const UA_NumericRange *range, UA_DataValue *data)
{
    (void) server;
    (void) sessionId;
    (void) sessionContext;
    (void) nodeId;
    (void) nodeContext;
    (void) sourceTimeStamp;
    (void) range;

    assert(nodeContext != NULL);

    UA_UInt64 currentTime = 0;
    struct timespec current_time_timespec;
    UA_UInt64 packetCount = g_sData->packetCount;

    clock_gettime(CLOCK_TAI, &current_time_timespec);

    tx_sequence++;
    currentTime = as_nanoseconds(&current_time_timespec);
    UA_UInt64 d[2] = {tx_sequence, currentTime};

    debug("[PUB] tx_sequence : %ld, time : %ld\n", tx_sequence, currentTime);

    UA_StatusCode retval = UA_Variant_setArrayCopy(&data->value, &d[0], 2,
                                                   &UA_TYPES[UA_TYPES_UINT64]);

    if (retval != UA_STATUSCODE_GOOD)
            debug("[PUB]Error in transmitting data source\n");

    data->hasValue = true;
    data->value.storageType = UA_VARIANT_DATA_NODELETE;

    if (tx_sequence == (UA_Int64)packetCount) {
        g_running = UA_FALSE;
        debug("\n[PUB] SendCurrentTime: tx_sequence=packet_count=%ld reached. Exiting...\n", tx_sequence);
    }

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
subStoreDataReceived(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, const UA_NumericRange *range,
                     const UA_DataValue *data)
{
    (void) server;
    (void) sessionId;
    (void) sessionContext;
    (void) nodeId;
    (void) nodeContext;
    (void) range;

    assert(nodeContext != NULL);

    struct SubscriberData *sdata = (struct SubscriberData *)nodeContext;
    FILE   *fpSubscriber = sdata->fpSubscriberOutput;
    UA_UInt64 packetCount = g_sData->packetCount;
    UA_Int32 ret = 0;

    struct msgq_buf msgqB = {.rx_sequence = 999, .txTime = 999 , .rxTime = 999 };

    if (data == NULL) {
        debug("[SUB] Rx Data is null\n");
        goto  ret;
    }

    /* Validate incoming data is byte array */
    UA_Variant v = data->value;

    if (v.arrayLength == (UA_UInt64) -1 && v.data == NULL) {
        debug("[SUB] Rx empty variant\n");
        goto ret;
    }

    /* Check if valid data is there */
    if (v.arrayLength == (UA_UInt64) -1 || v.arrayLength > 0) {
        g_roundtrip_pubReturn = true;

        UA_UInt64 *ptr_data = (UA_UInt64 *)data->value.data;
        UA_UInt64 RXhwTS = 0; /* TODO: hard code as per now */
        struct timespec current_time_timespec;
        clock_gettime(CLOCK_TAI, &current_time_timespec);

        msgqB.msg_type = MSGQ_TYPE;
        msgqB.rx_sequence = ptr_data[0];
        msgqB.txTime = ptr_data[1];
        msgqB.rxTime = as_nanoseconds(&current_time_timespec);

        UA_Int64 latency = (UA_Int64)(msgqB.rxTime - msgqB.txTime);

        if (fpSubscriber != NULL) {
            fprintf(fpSubscriber, "%ld\t%ld\t%d\t%ld\t%ld\t%ld\n",
                    latency, msgqB.rx_sequence, sdata->id, msgqB.txTime, RXhwTS, msgqB.rxTime);
        }

        if (g_sData->msqid >= 0) {
            ret = msgsnd(g_sData->msqid, (void *)&msgqB, sizeof(struct msgq_buf), IPC_NOWAIT);
            if (ret < 0) {
                if (errno == EAGAIN) {
                    debug("msgsnd: queue is full\n");
                } else {
                    debug("msgsnd: errno:%d\n", errno);
                }
            }
        }

        debug("[SUB] RX: seq:%ld rx_t(ns):%ld local_t(ns):%ld diff_t(ns): %ld\n",
              (long)msgqB.rx_sequence, (long)msgqB.txTime, (long)msgqB.rxTime, latency);
    }

    if (msgqB.rx_sequence == packetCount) {
        if (fpSubscriber != NULL) {
            fflush(fpSubscriber);
            fclose(fpSubscriber);
        }
        g_running = UA_FALSE;
        debug("[SUB] RX: rx_sequence = packet_count = %ld\n", msgqB.rx_sequence);
    }

ret:
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
pubReturnGetDataToTransmit(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, UA_Boolean sourceTimeStamp,
                     const UA_NumericRange *range, UA_DataValue *data)
{
    (void) server;
    (void) sessionId;
    (void) sessionContext;
    (void) nodeId;
    (void) nodeContext;
    (void) sourceTimeStamp;
    (void) range;

    assert(nodeContext != NULL);
    UA_UInt64 currentTime = 0;
    struct timespec current_time_timespec;
    UA_UInt64 packetCount = g_sData->packetCount;
    UA_UInt64 curr_rx_sequence = 0;
    UA_UInt64 curr_rxTime = 0;
    UA_UInt64 curr_txTime = 0;
    UA_Int32 ret = 0;

    struct msgq_buf msgqC = {.rx_sequence = 888, .txTime = 888 , .rxTime = 888 };

    /* Give a window of 150us */
    for (int i = 0; i < 15; i++) {
        ret = msgrcv(g_sData->msqid, (void *)&msgqC, sizeof(struct msgq_buf),
                     MSGQ_TYPE, IPC_NOWAIT);
        if (ret < 0) {
            if (errno == EAGAIN) {
                debug("msgrcv: No message available\n");
            } else {
                debug("msgrcv: errno:%d\n", errno);
            }
        }
         else {
             break;
         }
         usleep(10);
     }

    curr_rx_sequence = msgqC.rx_sequence;
    curr_rxTime = msgqC.rxTime;
    curr_txTime = msgqC.txTime;

    /* Validate forwarded data. We will only pass if */
    if (curr_rx_sequence < 10 && curr_rxTime == 999 && curr_txTime == 999) {
        log("[PUBR] #%ld Early invalid packet. Pausing pubReturn.", tx_sequence);
        g_roundtrip_pubReturn = false;

    } else if(curr_rx_sequence < 10 && curr_rxTime == 888 && curr_txTime == 888) {
        log("[PUBR] #%ld Early msgq missed. Pausing pubReturn.", tx_sequence);
        g_roundtrip_pubReturn = false;

    } else if(curr_rx_sequence == 999 && curr_rxTime == 999 && curr_txTime == 999) {
        log("[PUBR] #%ld forwarding error logged.\n", tx_sequence);
        curr_rx_sequence = ERROR_NOTHING_TO_FORWARD;
        curr_rxTime = ERROR_NOTHING_TO_FORWARD;
        curr_txTime = ERROR_NOTHING_TO_FORWARD;

    } else if(curr_rx_sequence == 888 && curr_rxTime == 888 && curr_txTime == 888) {
        log("[PUBR] #%ld msgq copy error logged.", tx_sequence);
        curr_rx_sequence = ERROR_MSGQ_COPY;
        curr_rxTime = ERROR_MSGQ_COPY;
        curr_txTime = ERROR_MSGQ_COPY;

    } else if (curr_rx_sequence == prev_rx_sequence && curr_rx_sequence > 10 && curr_rxTime != 999) {
        log("[PUBR] #%ld duplicate error logged.", tx_sequence);
        curr_rx_sequence = ERROR_DUPLICATE;
        curr_rxTime = ERROR_DUPLICATE;
        curr_txTime = ERROR_DUPLICATE;

    } else {
        tx_sequence++; /* all good, bump tx counter */
    }

    /* Prep data for forwarding */
    clock_gettime(CLOCK_TAI, &current_time_timespec);
    currentTime = as_nanoseconds(&current_time_timespec);

    UA_UInt64 d[5] = {curr_rx_sequence, curr_txTime, curr_rxTime, tx_sequence, currentTime};

    debug("[PUBR] rx_seqA:%ld, txtimePubA:%ld, rxTimeSubB:%ld, tx_seqB:%ld, txtimePubB:%ld\n",
          curr_rx_sequence, curr_txTime, curr_rxTime, tx_sequence, currentTime);

    UA_StatusCode retval = UA_Variant_setArrayCopy(&data->value, &d[0], 5,
                                                   &UA_TYPES[UA_TYPES_UINT64]);
    if (retval != UA_STATUSCODE_GOOD)
            debug("[PUBR] Error in transmitting data source\n");

    data->hasValue = true;
    data->value.storageType = UA_VARIANT_DATA_NODELETE;

    /* Termination condition */
    if (tx_sequence >= (UA_Int64)packetCount) {
        g_running = UA_FALSE;
        debug("\n[PUBR] SendCurrentTime: tx_sequence=packet_count=%ld reached. Exiting...\n",
              tx_sequence);
    }

    if (curr_rx_sequence != ERROR_DUPLICATE &&
        curr_rx_sequence != ERROR_MSGQ_COPY &&
        curr_rx_sequence != ERROR_NOTHING_TO_FORWARD)
        prev_rx_sequence = curr_rx_sequence;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
subReturnStoreDataReceived(UA_Server *server, const UA_NodeId *sessionId,
                           void *sessionContext, const UA_NodeId *nodeId,
                           void *nodeContext, const UA_NumericRange *range,
                           const UA_DataValue *data)
{
    (void) server;
    (void) sessionId;
    (void) sessionContext;
    (void) nodeId;
    (void) nodeContext;
    (void) range;
    assert(nodeContext != NULL);

    struct SubscriberData *sdata = (struct SubscriberData *)nodeContext;
    FILE   *fpSubscriber = sdata->fpSubscriberOutput;
    UA_UInt64 packetCount = g_sData->packetCount;
    UA_UInt64 seqA = 0;
    UA_UInt64 seqB = 0;

    if (data == NULL) {
        debug("[SUBR] Rx data is NULL\n");
        goto  ret;
    }

    /* Validate incoming data is byte array */
    UA_Variant v = data->value;

    if (v.arrayLength == (UA_UInt64) -1 && v.data == NULL) {
        debug("[SUBR] Rx empty variant\n");
        goto ret;
    }

    /* Check if valid data is there */
    if (v.arrayLength == (UA_UInt64) -1 || v.arrayLength > 0) {
        /* Data received ; {SeqA, tx-PubA, rx-SubB, SeqB, tx-PubB}; */
        UA_UInt64 *ptr_data = (UA_UInt64 *)data->value.data;
        UA_UInt64 RXhwTS = 0; /* TODO: hard code as per now */
        UA_UInt64 txPubA = ptr_data[1];
        UA_UInt64 rxSubB = ptr_data[2];
        UA_UInt64 txPubB = ptr_data[4];
        UA_UInt64 rxSubA = 0;
        UA_Int64  a2bLatency = 0;
        UA_Int64  b2aLatency = 0;
        UA_Int64  processingLatency = 0;
        UA_Int64  returnLatency = 0;
        struct timespec current_time_timespec;
        seqA   = ptr_data[0];
        seqB   = ptr_data[3];

        clock_gettime(CLOCK_TAI, &current_time_timespec);
        rxSubA = as_nanoseconds(&current_time_timespec);
        a2bLatency = (UA_Int64)(rxSubB - txPubA);
        b2aLatency = (UA_Int64)(rxSubA - txPubB);
        processingLatency = (UA_Int64)(txPubB - rxSubB);
        returnLatency = (UA_Int64)(rxSubA-txPubA);

        if (fpSubscriber != NULL) {
            if (seqA == ERROR_DUPLICATE) {
                fprintf(fpSubscriber, "ERROR_DUPLICATE_SKIPPED\n");
            } else if (seqA == ERROR_MSGQ_COPY) {
                fprintf(fpSubscriber, "ERROR_MSGQ_COPY\n");
            } else if (seqA == ERROR_NOTHING_TO_FORWARD) {
                fprintf(fpSubscriber, "ERROR_NOTHING_TO_FORWARD\n");
            } else {
                fprintf(fpSubscriber,
                        "%ld\t%ld\t%d\t%ld\t%ld\t%ld\t%ld\t%ld\t%ld\t%d\t%ld\t%ld\t%ld\t%ld\n",
                        a2bLatency, seqA, sdata->id, txPubA, RXhwTS, rxSubB, processingLatency,
                        b2aLatency, seqB, sdata->id, txPubB, RXhwTS, rxSubA, returnLatency);
            }
        }
    } else {
        debug("[SUBR] Rx invalid variant\n");
    }

    if (seqA != ERROR_DUPLICATE && seqA != ERROR_MSGQ_COPY
        && seqA != ERROR_NOTHING_TO_FORWARD && seqA >= packetCount) {
        if (fpSubscriber != NULL) {
            fflush(fpSubscriber);
            fclose(fpSubscriber);
        }
        g_running = UA_FALSE;
        debug("\n[SUBR][readCurrentTime]: rx_sequence=packet_count=%ld reached. Exit.\n ",
              seqA);
    }
ret:
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
dummyDSRead(UA_Server *server, const UA_NodeId *sessionId,
            void *sessionContext, const UA_NodeId *nodeId,
            void *nodeContext, UA_Boolean sourceTimeStamp,
            const UA_NumericRange *range, UA_DataValue *value)
{
    (void) server;
    (void) sessionId;
    (void) sessionContext;
    (void) nodeId;
    (void) nodeContext;
    (void) sourceTimeStamp;
    (void) range;
    (void) value;

    debug("WARN: dummyDSRead() shouldn't be called");
    return UA_STATUSCODE_GOOD;
}


UA_StatusCode
dummyDSWrite(UA_Server *server, const UA_NodeId *sessionId,
             void *sessionContext, const UA_NodeId *nodeId,
             void *nodeContext, const UA_NumericRange *range,
             const UA_DataValue *value)
{
    (void) server;
    (void) sessionId;
    (void) sessionContext;
    (void) nodeId;
    (void) nodeContext;
    (void) range;
    (void) value;

    debug("WARN: dummyDSWrite() shouldn't be called");
    return UA_STATUSCODE_GOOD;
}
