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
#ifndef _OPCUA_CUSTOM_H_
#define _OPCUA_CUSTOM_H_

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "opcua_utils.h"

/* INTERNAL structs START ***************************************************
 * The following internal structures from src/pubsub/ua_pubsub.h are required
 * for checking the callback type. If the lib changes, then these structs
 * should be updated as well.
 * TODO: find out if there are better ways to check the Repeatedcallback type
 */

#define TAILQ_ENTRY(type)                                                      \
struct {                                                                       \
    struct type *tqe_next;         /* next element */                          \
    struct type **tqe_prev;         /* address of previous next element */     \
}

#define LIST_ENTRY(type)                                                       \
struct {                                                                       \
    struct type *le_next;         /* next element */                           \
    struct type **le_prev;         /* address of previous next element */      \
}

#define LIST_HEAD(name, type)                                                  \
struct name {                                                                  \
    struct type *lh_first;         /* first element */                         \
}

typedef struct UA_ReaderGroup {
    UA_ReaderGroupConfig config;
    UA_NodeId identifier;
    UA_NodeId linkedConnection;
    LIST_ENTRY(UA_ReaderGroup) listEntry;
    LIST_HEAD(UA_ListOfPubSubDataSetReader, UA_DataSetReader) readers;
    /* for simplified information access */
    UA_UInt32 readersCount;
    UA_UInt64 subscribeCallbackId;
    UA_Boolean subscribeCallbackIsRegistered;
} UA_ReaderGroup;

/* Offsets for buffered messages in the PubSub fast path. */
typedef enum {
    UA_PUBSUB_OFFSETTYPE_DATASETMESSAGE_SEQUENCENUMBER,
    UA_PUBSUB_OFFSETTYPE_NETWORKMESSAGE_SEQUENCENUMBER,
    UA_PUBSUB_OFFSETTYPE_TIMESTAMP_PICOSECONDS,
    UA_PUBSUB_OFFSETTYPE_TIMESTAMP,     /* source pointer */
    UA_PUBSUB_OFFSETTYPE_TIMESTAMP_NOW, /* no source */
    UA_PUBSUB_OFFSETTYPE_PAYLOAD_DATAVALUE,
    UA_PUBSUB_OFFSETTYPE_PAYLOAD_VARIANT,
    UA_PUBSUB_OFFSETTYPE_PAYLOAD_RAW
    /* Add more offset types as needed */
} UA_NetworkMessageOffsetType;

typedef struct {
    UA_NetworkMessageOffsetType contentType;
    union {
        union {
            UA_DataValue *value;
            size_t valueBinarySize;
        } value;
        UA_DateTime *timestamp;
    } offsetData;
    size_t offset;
} UA_NetworkMessageOffset;

typedef struct UA_PubSubConnection{
    UA_PubSubConnectionConfig *config;
    /* internal fields */
    UA_PubSubChannel *channel;
    UA_NodeId identifier;
    LIST_HEAD(UA_ListOfWriterGroup, UA_WriterGroup) writerGroups;
    LIST_HEAD(UA_ListOfPubSubReaderGroup, UA_ReaderGroup) readerGroups;
    size_t readerGroupsSize;
    TAILQ_ENTRY(UA_PubSubConnection) listEntry;
    UA_UInt16 configurationFreezeCounter;
} UA_PubSubConnection;

typedef struct {
    UA_ByteString buffer; /* The precomputed message buffer */
    UA_NetworkMessageOffset *offsets; /* Offsets for changes in the message buffer */
    size_t offsetsSize;
} UA_NetworkMessageOffsetBuffer;


typedef struct UA_WriterGroup{
    UA_WriterGroupConfig config;
    /* internal fields */
    LIST_ENTRY(UA_WriterGroup) listEntry;
    UA_NodeId identifier;
    UA_PubSubConnection *linkedConnection;
    LIST_HEAD(UA_ListOfDataSetWriter, UA_DataSetWriter) writers;
    UA_UInt32 writersCount;
    UA_UInt64 publishCallbackId;
    UA_Boolean publishCallbackIsRegistered;
    UA_PubSubState state;
    UA_NetworkMessageOffsetBuffer bufferedMessage;
    UA_UInt16 sequenceNumber; /* Increased after every succressuly sent message */
} UA_WriterGroup;

/* INTERNAL structs END *******************************************************/

typedef struct threadParams {
    UA_Server                    *server;
    void                         *data;
    UA_ServerCallback            callback;
    UA_Duration                  interval_ms;
    UA_UInt64                    *callbackId;
    UA_UInt32                    data_index;
    pthread_t id;
} threadParams;

void *pub_thread(void *arg);
void *sub_thread(void *arg);

UA_StatusCode
UA_PubSubManager_addRepeatedCallback(UA_Server *server,
                                     UA_ServerCallback callback,
                                     void *data,
                                     UA_Double interval_ms,
                                     UA_UInt64 *callbackId);


UA_StatusCode
UA_PubSubManager_changeRepeatedCallbackInterval(UA_Server *server,
                                                UA_UInt64 callbackId,
                                                UA_Double interval_ms);

void
UA_PubSubManager_removeRepeatedPubSubCallback(UA_Server *server,
                                              UA_UInt64 callbackId);
#endif
