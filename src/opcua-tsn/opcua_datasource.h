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
#ifndef _OPCUA_DATASOURCE_H_
#define _OPCUA_DATASOURCE_H_

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>

#include "opcua_utils.h"

#define ERROR_DUPLICATE 3131313131313131313LL
#define ERROR_MSGQ_COPY 4343434343434343434LL
#define ERROR_NOTHING_TO_FORWARD 5656565656565656565LL
#define MSGQ_TYPE 1

/* Specially for round-trip Sub forward to PubReturn */
struct msgq_buf {
    long msg_type;
    UA_UInt64 rx_sequence;
    UA_UInt64 txTime;
    UA_UInt64 rxTime;
};

UA_StatusCode
pubReturnGetDataToTransmit(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, UA_Boolean sourceTimeStamp,
                     const UA_NumericRange *range, UA_DataValue *data);

UA_StatusCode
pubGetDataToTransmit(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, UA_Boolean sourceTimeStamp,
                     const UA_NumericRange *range, UA_DataValue *value);

UA_StatusCode
subReturnStoreDataReceived(UA_Server *server, const UA_NodeId *sessionId,
                           void *sessionContext, const UA_NodeId *nodeId,
                           void *nodeContext, const UA_NumericRange *range,
                           const UA_DataValue *data);

UA_StatusCode
subStoreDataReceived(UA_Server *server, const UA_NodeId *sessionId,
                     void *sessionContext, const UA_NodeId *nodeId,
                     void *nodeContext, const UA_NumericRange *range,
                     const UA_DataValue *data);

UA_StatusCode
dummyDSWrite(UA_Server *server, const UA_NodeId *sessionId,
             void *sessionContext, const UA_NodeId *nodeId,
             void *nodeContext, const UA_NumericRange *range,
             const UA_DataValue *value);

UA_StatusCode
dummyDSRead(UA_Server *server, const UA_NodeId *sessionId,
            void *sessionContext, const UA_NodeId *nodeId,
            void *nodeContext, UA_Boolean sourceTimeStamp,
            const UA_NumericRange *range, UA_DataValue *value);
#endif
