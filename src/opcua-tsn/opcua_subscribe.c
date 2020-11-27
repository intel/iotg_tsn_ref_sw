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
#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include <time.h>

#include "opcua_common.h"

/* Add new connection to the server */
void addSubConnection(UA_Server *server, UA_NodeId *connId,
                      struct ServerData *sdata, struct SubscriberData *sub)
{
    /* Configuration creation for the connection */
    UA_PubSubConnectionConfig connectionConfig;

    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.name = UA_STRING("UDPMC Connection 1");
    connectionConfig.transportProfileUri = sdata->transportProfile;
    connectionConfig.enabled = UA_TRUE;

    UA_NetworkAddressUrlDataType networkAddressUrl = {
        UA_STRING(sdata->subInterface),
        UA_STRING(sub->url)
    };

    if (sdata->useXDP) {
        connectionConfig.xdp_queue = sub->xdpQueue;

        if (sdata->useXDP_SKB && !sdata->useXDP_ZC) {
            connectionConfig.xdp_flags |= XDP_FLAGS_SKB_MODE;
            connectionConfig.xdp_bind_flags |= XDP_COPY;
        } else if (!sdata->useXDP_SKB && !sdata->useXDP_ZC) {
            connectionConfig.xdp_flags |= XDP_FLAGS_DRV_MODE;
            connectionConfig.xdp_bind_flags |= XDP_COPY;
        } else {
            connectionConfig.xdp_flags |= XDP_FLAGS_DRV_MODE;
            connectionConfig.xdp_bind_flags |= XDP_ZEROCOPY;
        }
    }

    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    if (sub->id == 0)
        connectionConfig.publisherId.numeric =  UA_UInt32_random();
    else
        connectionConfig.publisherId.numeric = sub->id;
    UA_Server_addPubSubConnection(server, &connectionConfig, connId);
}

/* Add ReaderGroup to the created connection */
static void addReaderGroup(UA_Server *server, UA_NodeId *connId,
                           UA_NodeId *readerGroupId)
{
    UA_ReaderGroupConfig readerGroupConfig;

    memset(&readerGroupConfig, 0, sizeof(UA_ReaderGroupConfig));
    readerGroupConfig.name = UA_STRING("ReaderGroup1");

    UA_Server_addReaderGroup(server, *connId, &readerGroupConfig,
                             readerGroupId);
}

/* Add DataSetReader to the ReaderGroup */
static void addDataSetReader(UA_Server *server, UA_NodeId *readerGroupIdentifier,
                             UA_NodeId *datasetReaderIdentifier,
                             struct SubscriberData *sub)
{
    UA_DataSetReaderConfig readerConfig;
    memset(&readerConfig, 0, sizeof(UA_DataSetReaderConfig));
    readerConfig.name                 = UA_STRING("DataSet Reader 1");
    readerConfig.dataSetWriterId      = (UA_UInt16)sub->subscribedDSWriterId;
    UA_UInt16 publisherIdentifier     = (UA_UInt16)sub->subscribedPubId;
    readerConfig.publisherId.type     = &UA_TYPES[UA_TYPES_UINT16];
    readerConfig.publisherId.data     = &publisherIdentifier;
    readerConfig.writerGroupId        = (UA_UInt16)sub->subscribedWGId;

    /* Setting up Meta data configuration in DataSetReader */
    UA_DataSetMetaDataType *pMetaData = &readerConfig.dataSetMetaData;
    UA_DataSetMetaDataType_init(pMetaData);

    /* Static definition of number of fields size = 1 */
    pMetaData->name       = UA_STRING("DataSet Test");
    pMetaData->fieldsSize = 1;
    pMetaData->fields = (UA_FieldMetaData*)UA_Array_new(pMetaData->fieldsSize,
                         &UA_TYPES[UA_TYPES_FIELDMETADATA]);
    sub->temp_dataSetMetaData = (char *) pMetaData->fields;

    /* Sequence and timestamp is UINT64 DataType */
    UA_FieldMetaData_init(&pMetaData->fields[0]);
    UA_NodeId_copy(&UA_TYPES[UA_TYPES_UINT64].typeId,
                   &pMetaData->fields[0].dataType);
    pMetaData->fields[0].builtInType = UA_TYPES_UINT64;
    pMetaData->fields[0].name = UA_STRING ("Sequence timestamp");
    pMetaData->fields[0].valueRank = -3; /* scalar or 1-D array */

    UA_Server_addDataSetReader(server, *readerGroupIdentifier,
                               &readerConfig, datasetReaderIdentifier);
}

/* Set SubscribedDataSet type to TargetVariables data type
 * Add subscribedvariables to the DataSetReader
 */
static void addSubscribedVariables(UA_Server *server, struct SubscriberData *sub,
                                   UA_NodeId *datasetReaderIdentifier)
{
    /* Add another variable, as a datasource node. */
    /* 1) Set the variable attributes. */
    UA_VariableAttributes attr = UA_VariableAttributes_default;

    attr.displayName = UA_LOCALIZEDTEXT("en_US", "Time stamp datasource");
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    /* 2) Define where the variable shall be added with which browse name */
    UA_NodeId datasource_node_id = UA_NODEID_STRING(1, "time-stamp-datasource");
    UA_NodeId parent_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parent_ref_node_id = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId variable_type = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);
    UA_QualifiedName browse_name = UA_QUALIFIEDNAME(1, "Time stamp datasource");

    UA_DataSource data_source;

    data_source.read = sub->readFunc;
    data_source.write = sub->writeFunc;

    /* 3) Add the variable with data source. */
    UA_Server_addDataSourceVariableNode(server, datasource_node_id,
                                        parent_node_id,
                                        parent_ref_node_id,
                                        browse_name,
                                        variable_type, attr,
                                        data_source,
                                        sub, NULL);

    UA_TargetVariablesDataType targetVars;

    targetVars.targetVariablesSize = 1;
    targetVars.targetVariables     = (UA_FieldTargetDataType *)
                                      UA_calloc(targetVars.targetVariablesSize,
                                      sizeof(UA_FieldTargetDataType));
    sub->temp_targetVars = (char *) targetVars.targetVariables;

    if (&targetVars.targetVariables[0] == NULL) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "failed to calloc targetVariables");
        return;
    }

    UA_FieldTargetDataType_init(&targetVars.targetVariables[0]);
    targetVars.targetVariables[0].attributeId  = UA_ATTRIBUTEID_VALUE;
    targetVars.targetVariables[0].targetNodeId = datasource_node_id;
    UA_Server_DataSetReader_createTargetVariables(server, *datasetReaderIdentifier, &targetVars);

    // TODO: Not sure why this will segfault.
    // Workaround using temp_targetVars & temp_dataSetMetaData to properly free
    // UA_TargetVariablesDataType_deleteMembers(&targetVars);
    // UA_free(ua->readerConfig.dataSetMetaData.fields);
}

int createSubscriber(UA_Server *server, struct ServerData *sdata,
                     struct SubscriberData *sub, UA_NodeId *connId)
{
    (void) sdata;

    UA_NodeId readerGroupIdentifier;
    UA_NodeId dataSetReaderIdentifier;
    addReaderGroup(server, connId, &readerGroupIdentifier);
    addDataSetReader(server, &readerGroupIdentifier, &dataSetReaderIdentifier, sub);
    addSubscribedVariables(server, sub, &dataSetReaderIdentifier);
    return 0;
}

