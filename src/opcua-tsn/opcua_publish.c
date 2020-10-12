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
#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/pubsub_ethernet_etf.h>
#include <open62541/plugin/pubsub_ethernet_xdp.h>
#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include <time.h>

#include "opcua_common.h"
#include "opcua_publish.h"

#define PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS    //TODO define this elsewhere
#define KEYFRAME_COUNT     10
#define PUB_MODE_ZERO_COPY

extern UA_NodeId g_writerGroupIdent;

void addPubSubConnection(UA_Server *server, UA_NodeId *connId,
                         struct ServerData *sdata, struct PublisherData *pdata)
{

    /* Details about the connection configuration and handling are located
     * in the pubsub connection tutorial
     */
    UA_PubSubConnectionConfig connectionConfig;

    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UADP Connection");
    connectionConfig.transportProfileUri = sdata->transportProfile;
    connectionConfig.enabled = UA_TRUE;

    int socketPrio = pdata->socketPriority;
    connectionConfig.etfConfiguration.socketPriority = socketPrio > 0 ?
                                                       socketPrio : -1;
    connectionConfig.etfConfiguration.sotxtimeEnabled = UA_TRUE;

    if (sdata->useXDP) {
#ifdef PUB_MODE_ZERO_COPY
        connectionConfig.xdp_queue = pdata->xdpQueue;
        connectionConfig.xdp_flags |= XDP_FLAGS_DRV_MODE;
        connectionConfig.xdp_bind_flags |= XDP_ZEROCOPY;
#else
        connectionConfig.xdp_queue = pdata->xdpQueue;
        connectionConfig.xdp_flags |= XDP_FLAGS_SKB_MODE;
        connectionConfig.xdp_bind_flags |= XDP_COPY;
#endif
    }

    UA_NetworkAddressUrlDataType networkAddressUrl = {
                         UA_STRING(sdata->pubInterface), UA_STRING(pdata->url)};

    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);

    connectionConfig.publisherId.numeric = (UA_UInt16)pdata->id;
    UA_Server_addPubSubConnection(server, &connectionConfig, connId);
}

/**
 * **PublishedDataSet handling**
 *
 * The PublishedDataSet (PDS) and PubSubConnection are the toplevel entities and
 * can exist alone. The PDS contains the collection of the published fields. All
 * other PubSub elements are directly or indirectly linked with the PDS or
 * connection.
 */
static void addPublishedDataSet(UA_Server *server, UA_NodeId *publishedDataSetIdent)
{
    /* The PublishedDataSetConfig contains all necessary public
     * informations for the creation of a new PublishedDataSet
     */
    UA_PublishedDataSetConfig publishedDataSetConfig;

    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    /* Create new PublishedDataSet based on the PublishedDataSetConfig.
     */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig, publishedDataSetIdent);
}

/**
 * **DataSetField handling**
 *
 * The DataSetField (DSF) is part of the PDS and describes exactly one published
 * field.
 */
static void addDataSetField(UA_Server *server, UA_NodeId *publishedDataSetIdent,
        struct ServerData *sdata, struct PublisherData *pdata)
{
    (void) sdata;

    /* Add a variable datasource node. */
    /* Set the variable attributes. */
    UA_VariableAttributes attr = UA_VariableAttributes_default;

    attr.displayName = UA_LOCALIZEDTEXT("en_US", "Payload datasource");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    /* Define where the variable shall be added with which browse name */
    UA_NodeId new_node_id = UA_NODEID_STRING(1, "payload-datasource");
    UA_NodeId parent_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parent_ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variable_type = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
    UA_QualifiedName browse_name = UA_QUALIFIEDNAME(1, "Shared mem DS");

    UA_DataSource data_source;

    data_source.read = pdata->readFunc;
    data_source.write = pdata->writeFunc;

    /* Add the variable with data source. */
    UA_Server_addDataSourceVariableNode(server, new_node_id,
                                        parent_node_id,
                                        parent_ref_node_id,
                                        browse_name,
                                        variable_type, attr,
                                        data_source,
                                        pdata, NULL);

    /* Add a field to the previous created PublishedDataSet */
    UA_NodeId dataSetFieldIdent;
    UA_DataSetFieldConfig dataSetFieldConfig;
    memset(&dataSetFieldConfig, 0, sizeof(UA_DataSetFieldConfig));
    dataSetFieldConfig.dataSetFieldType = UA_PUBSUB_DATASETFIELD_VARIABLE;
    dataSetFieldConfig.field.variable.fieldNameAlias = UA_STRING("payload");
    dataSetFieldConfig.field.variable.promotedField = UA_FALSE;
    dataSetFieldConfig.field.variable.publishParameters.publishedVariable =
                                UA_NODEID_STRING(1, "payload-datasource");
    dataSetFieldConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, *publishedDataSetIdent,
                              &dataSetFieldConfig, &dataSetFieldIdent);
}

/**
 * **WriterGroup handling**
 *
 * The WriterGroup (WG) is part of the connection and contains the primary
 * configuration parameters for the message creation.
 */
static void addWriterGroup(UA_Server *server, UA_NodeId *connectionIdent,
        UA_NodeId *writerGroupIdent, struct ServerData *sdata,
        struct PublisherData *pdata)
{
    double intervalMs = (double)sdata->cycleTimeNs / 1e6f;

    /* Now we create a new WriterGroupConfig and add the group to the existing
     * PubSubConnection.
     */
    UA_WriterGroupConfig writerGroupConfig;

    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    // This is in miliseconds, not nanoseconds
    writerGroupConfig.publishingInterval = intervalMs;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = pdata->writerGroupId;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
#if defined PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS
    //TODO: enable rtlevel to increase performance?
    // writerGroupConfig.rtLevel            = UA_PUBSUB_RT_FIXED_SIZE;
#endif
    writerGroupConfig.messageSettings.encoding             = UA_EXTENSIONOBJECT_DECODED;
    writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];

    /* The configuration flags for the messages are encapsulated inside the
     * message- and transport settings extension objects. These extension
     * objects are defined by the standard. e.g.
     * UadpWriterGroupMessageDataType
     */
    UA_UadpWriterGroupMessageDataType *writerGroupMessage  = UA_UadpWriterGroupMessageDataType_new();
    /* Change message settings of writerGroup to send PublisherId,
     * WriterGroupId in GroupHeader and DataSetWriterId in PayloadHeader
     * of NetworkMessage
     */
    writerGroupMessage->networkMessageContentMask          = (UA_UadpNetworkMessageContentMask)(UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                                             (UA_UadpNetworkMessageContentMask)UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                                             (UA_UadpNetworkMessageContentMask)UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                                             (UA_UadpNetworkMessageContentMask)UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);
    writerGroupConfig.messageSettings.content.decoded.data = writerGroupMessage;

    UA_Server_addWriterGroup(server, *connectionIdent, &writerGroupConfig, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, *writerGroupIdent);
    UA_UadpWriterGroupMessageDataType_delete(writerGroupMessage);
}

/**
 * **DataSetWriter handling**
 *
 * A DataSetWriter (DSW) is the glue between the WG and the PDS. The DSW is
 * linked to exactly one PDS and contains additional informations for the
 * message generation.
 */
static void addDataSetWriter(UA_Server *server, UA_NodeId *writerGroupIdent,
                             UA_NodeId *publishedDataSetIdent, struct PublisherData *pdata)
{
    /* We need now a DataSetWriter within the WriterGroup. This means we must
     * create a new DataSetWriterConfig and add call the addWriterGroup function.
     */
    UA_NodeId dataSetWriterIdent;
    UA_DataSetWriterConfig dataSetWriterConfig;

    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = (UA_UInt16)pdata->dataSetWriterId;
    dataSetWriterConfig.keyFrameCount = KEYFRAME_COUNT;
    UA_Server_addDataSetWriter(server, *writerGroupIdent, *publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

int createPublisher(UA_Server *serv, struct ServerData *sdata,
                    struct PublisherData *pdata, UA_NodeId *connectionIdent)
{
    UA_NodeId publishedDataSetIdent;
    addPublishedDataSet(serv, &publishedDataSetIdent);
    addDataSetField(serv, &publishedDataSetIdent, sdata, pdata);
    addWriterGroup(serv, connectionIdent, &g_writerGroupIdent, sdata, pdata);
    addDataSetWriter(serv, &g_writerGroupIdent, &publishedDataSetIdent, pdata);
    UA_Server_freezeWriterGroupConfiguration(serv, g_writerGroupIdent);
    return 0;
}
