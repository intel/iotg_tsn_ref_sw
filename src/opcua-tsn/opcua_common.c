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

#include "opcua_common.h"
#include "json_helper.h"

#define PKT_COUNT_MAX 1000000
#define CYCLETIME_NS_MAX 5000000

#define HUNDRED_USEC_NSEC 100000
#define ONE_MSEC_NSEC 1000000
#define TEN_MSEC_NSEC 10000000
#define FIVE_DIGIT_MAX 99999

/* TODO: check if we can reuse this
 * Set threads to use RT scheduling so that we pre-empt all other
 * less important tasks
 * Set CPU affinity to ensure we're not migrated to another processor
 * to improve performance
 */
int setRtPriority(pthread_t thread, int priority, uint32_t cpu)
{
    catch_err(priority < 0, "Priority must be greater than 0");

    cpu_set_t cpuset;
    int policy;
    struct sched_param sp;
    int ret = pthread_getschedparam(thread, &policy, &sp);
    catch_err(ret != 0, "Error getting thread scheduler");

    sp.sched_priority = priority;
    ret = pthread_setschedparam(thread, SCHED_FIFO, &sp);
    catch_err(ret != 0, "Failed to set thread scheduler");

    if (cpu == 0)
        return 0;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    catch_err(ret != 0, "Failed to set thread CPU affinity");
    return 0;

error:
    perror("Failed to set RT proirity");
    return -1;
}

void free_resources(struct ServerData *sdata)
{
    int i = 0;

    for (i = 0; i < sdata->pubCount; i++) {
        if (sdata->pubData[i].url)
            free(sdata->pubData[i].url);
    }

    if (sdata->pubData)
        free(sdata->pubData);

    for (i = 0; i < sdata->subCount; i++) {
        if (sdata->subData[i].url)
            free(sdata->subData[i].url);

        if (sdata->subData[i].subscriberOutputFileName)
            free(sdata->subData[i].subscriberOutputFileName);

        if (sdata->subData[i].fpSubscriberOutput)
            fclose(sdata->subData[i].fpSubscriberOutput);
    }

    if (sdata->subData)
        free(sdata->subData);

    if(sdata->subInterface)
        free(sdata->subInterface);

    if(sdata->subInterface)
        free(sdata->pubInterface);

    free(sdata);
}

struct ServerData *parseJson(struct json_object *json)
{
    struct ServerData *s = malloc(sizeof(struct ServerData));
    catch_err(s == NULL, "ServerData - Out of memory");

    log("Parsing OPCUA server");
    s->pubInterface = getString(json, "publisher_interface");
    s->subInterface = getString(json, "subscriber_interface");

    s->useXDP = getBool(json, "use_xdp");

    s->pollingDurationNs = getInt(json, "polling_duration_ns");
    catch_err(s->pollingDurationNs < 0 || s->pollingDurationNs > TEN_MSEC_NSEC,
              "Invalid polling_duration_ns");

    s->packetCount = getInt64(json, "packet_count");
    catch_err(s->packetCount < 1 || s->packetCount > PKT_COUNT_MAX,
              "Invalid packet_count");

    s->cycleTimeNs = getInt64(json, "cycle_time_ns");
    catch_err(s->cycleTimeNs < HUNDRED_USEC_NSEC || s->cycleTimeNs > CYCLETIME_NS_MAX,
              "Invalid cycle_time_ns");

    debug("Found: pub if %s, sub if %s, cycleTimeNs %ld pollNs %d, packet_count %ld",
            s->pubInterface, s->subInterface, s->cycleTimeNs,
            s->pollingDurationNs, s->packetCount);

    struct PublisherData *pubData = NULL;
    struct SubscriberData *subData = NULL;

    struct json_object *pubs = getValue(json, "publishers");
    int pubCount = countChildrenEntries(pubs);
    pubData = calloc(pubCount, sizeof(struct PublisherData));
    if (pubData == NULL) {
        perror("PubData - Out of memory");
        goto error;
    }

    struct json_object *subs = getValue(json, "subscribers");
    int subCount = countChildrenEntries(subs);
    subData = calloc(subCount, sizeof(struct SubscriberData));
    if (subData == NULL) {
        free(pubData);
        perror("PubData - Out of memory");
        goto error;
    }

    s->pubCount = pubCount;
    s->subCount = subCount;
    s->pubData = pubData;
    s->subData = subData;
    log("Identified: %d publishers, %d subscribers", s->pubCount, s->subCount);

    struct json_object_iter iter;
    int cpuAff = 0;
    int i = 0;

    json_object_object_foreachC(pubs, iter) {
        struct json_object *pubJson = iter.val;
        struct PublisherData *pd = &pubData[i];

        pd->earlyOffsetNs = getInt(pubJson, "early_offset_ns");
        catch_err(pd->earlyOffsetNs < 0 || pd->earlyOffsetNs > ONE_MSEC_NSEC,
                  "Invalid early_offset_ns");

        pd->publishOffsetNs = getInt(pubJson, "publish_offset_ns");
        catch_err(pd->publishOffsetNs < 0 || pd->publishOffsetNs > TEN_MSEC_NSEC,
                  "Invalid publish_offset_ns");

        pd->socketPriority = getInt(pubJson, "socket_prio");
        catch_err(pd->socketPriority < 0 || pd->socketPriority > 7,
                  "Invalid socket_prio");

        if (s->useXDP) {
            pd->xdpQueue = getInt(pubJson, "xdp_queue");
            catch_err(pd->xdpQueue < 0 || pd->xdpQueue > 3, "Invalid xdp_queue");
        } else {
            pd->xdpQueue = -1;
        }

        pd->url = getString(pubJson, "url");

        pd->id = getInt(pubJson, "pub_id");
        catch_err(pd->id < 0  || pd->id > FIVE_DIGIT_MAX, "Invalid pub_id");

        pd->dataSetWriterId = getInt(pubJson, "dataset_writer_id");
        catch_err(pd->dataSetWriterId < 0,
                  "Negative dataset_writer_id is not valid");

        pd->writerGroupId = getInt(pubJson, "writer_group_id");
        catch_err(pd->writerGroupId < 0,
                  "Negative writerGroupId is not valid");

        pd->twoWayData = getBool(pubJson, "two_way_data");

        cpuAff = getInt(pubJson, "cpu_affinity");
        catch_err(cpuAff < 0 || cpuAff > 3, "Invalid cpu_affinity");
        pd->cpuAffinity = cpuAff;

        log("Publisher: %s %s CPU%ld",
            pd->url, s->useXDP ? "AF_XDP" : "AF_PACKET", pd->cpuAffinity);

        i++;
    }

    int j = 0;

    json_object_object_foreachC(subs, iter) {
        struct json_object *subJson = iter.val;
        struct SubscriberData *sd = &subData[j];

        sd->offsetNs = getInt(subJson, "offset_ns");
        catch_err(sd->offsetNs < 0 || sd->offsetNs > TEN_MSEC_NSEC,
                  "Invalid offset_ns");

        if (s->useXDP) {
            sd->xdpQueue = getInt(subJson, "xdp_queue");
            catch_err(sd->xdpQueue < 0 || sd->xdpQueue > 3, "Invalid xdp_queue");
        } else {
            sd->xdpQueue = -1;
        }

        sd->url = getString(subJson, "url");

        sd->id = getInt(subJson, "sub_id");
        catch_err(sd->id < 0 || sd->id > FIVE_DIGIT_MAX, "Invalid sub_id");

        sd->subscribedPubId = getInt(subJson, "subscribed_pub_id");
        catch_err(sd->subscribedPubId < 0 || sd->subscribedPubId > FIVE_DIGIT_MAX,
                  "Invalid subscribed_pub_id");

        sd->subscribedDSWriterId = getInt(subJson, "subscribed_dataset_writer_id");
        catch_err(sd->subscribedDSWriterId < 0 || sd->subscribedDSWriterId > FIVE_DIGIT_MAX,
                  "Invalid subscribed_dataset_writer_id");

        sd->subscribedWGId = getInt(subJson, "subscribed_writer_group_id");
        catch_err(sd->subscribedWGId < 0 || sd->subscribedWGId > FIVE_DIGIT_MAX,
                  "Invalid subscribed_writer_group_id");

        sd->twoWayData = getBool(subJson, "two_way_data");

        cpuAff = getInt(subJson, "cpu_affinity");
        catch_err(cpuAff < 0 || cpuAff > 3, "Invalid cpu_affinity");
        sd->cpuAffinity = cpuAff;

        sd->subscriberOutputFileName = getString(subJson, "subscriber_output_file");
        sd->fpSubscriberOutput = fopen(sd->subscriberOutputFileName, "w" );
        if (sd->fpSubscriberOutput)
            fflush(sd->fpSubscriberOutput);

        log("Subscriber: %s %s CPU%ld",
            sd->url, s->useXDP ? "AF_XDP" : "AF_PACKET", sd->cpuAffinity);

        j++;
    }

    return s;

error:
    if (s)
        free_resources(s);

    return NULL;
}

struct ServerData *parseArgs(char **argv)
{
    struct json_object *js = NULL, *opcuaJson;
    struct ServerData *sdata;

    char *jsonPath = argv[1];
    catch_err(jsonPath == NULL, "Path to JSON not specified (usage: ./opcua-server some_valid.json");

    debug("Reading: %s", jsonPath);

    js = json_object_from_file(jsonPath);
    catch_err(js == NULL, "Failed to extract json objects");

    opcuaJson = getValue(js, "opcua_server");
    sdata = parseJson(opcuaJson);
    catch_err(sdata == NULL, "Failed to parse JSON");

    /* Free json since we shouldn't be using it outside of parsing */
    json_object_put(js);

    return sdata;

error:
    if (js)
        json_object_put(js);

    return NULL;
}

UA_UInt64 as_nanoseconds(struct timespec *ts)
{
    return (uint64_t)(ts->tv_sec) * (uint64_t)1E9L + (uint64_t)(ts->tv_nsec);
}
