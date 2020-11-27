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
#ifndef _OPCUA_COMMON_H_
#define _OPCUA_COMMON_H_

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "opcua_utils.h"

typedef UA_StatusCode (DSCallbackRead)(UA_Server *server,
        const UA_NodeId *sessionId, void *sessionContext,
        const UA_NodeId *nodeId, void *nodeContext, UA_Boolean sourceTimeStamp,
        const UA_NumericRange *range, UA_DataValue *value);

typedef UA_StatusCode (DSCallbackWrite)(UA_Server *server,
        const UA_NodeId *sessionId, void *sessionContext,
        const UA_NodeId *nodeId, void *nodeContext,
        const UA_NumericRange *range, const UA_DataValue *value);

// TODO: Some of these params are no longer required.
struct PublisherData {
    int socketPriority;
    int xdpQueue;
    int32_t earlyOffsetNs;
    int32_t publishOffsetNs;
    char *url;
    int32_t id;
    int32_t dataSetWriterId;
    int32_t writerGroupId;
    int32_t prev_sequence_num;
    bool twoWayData;
    size_t cpuAffinity;
    DSCallbackRead *readFunc;
    DSCallbackWrite *writeFunc;
};

struct SubscriberData {
    int xdpQueue;
    int32_t offsetNs;
    char *url;
    int32_t id;
    int32_t subscribedPubId;
    int32_t subscribedDSWriterId;
    int32_t subscribedWGId;
    FILE *fpSubscriberOutput;
    char *subscriberOutputFileName;
    char *temp_targetVars;
    char *temp_dataSetMetaData;
    bool twoWayData;
    size_t cpuAffinity;
    DSCallbackRead *readFunc;
    DSCallbackWrite *writeFunc;
};

struct ServerData {
    char *pubInterface;
    char *subInterface;
    int64_t cycleTimeNs;
    int32_t pollingDurationNs;
    int cpu;
    bool useXDP;
    bool useXDP_ZC;
    bool useXDP_SKB;

    int pubCount;
    int subCount;
    struct PublisherData *pubData;
    struct SubscriberData *subData;

    UA_UInt64 startTime;
    UA_String transportProfile;
    UA_UInt64 packetCount;
};

void free_resources(struct ServerData *sdata);
struct ServerData *parseArgs(char **argv);
int setRtPriority(pthread_t thread, int priority, uint32_t cpu);
UA_UInt64 as_nanoseconds(struct timespec *ts);

#endif
