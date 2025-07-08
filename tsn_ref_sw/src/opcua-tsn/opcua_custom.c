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

#include <pthread.h>
#include <time.h>
#include <sched.h>

#include "opcua_custom.h"
#include "opcua_common.h"

#define CLOCKID                               CLOCK_TAI
#define ONESEC_IN_NSEC                        (1000 * 1000 * 1000)
#define PUB_THREAD_PRIORITY                   91
#define SUB_THREAD_PRIORITY                   90

extern struct threadParams g_thread[MAX_OPCUA_THREAD];
extern UA_UInt16 g_threadRun;
extern UA_Boolean g_running, g_roundtrip_pubReturn;
extern UA_NodeId g_writerGroupIdent;
extern struct ServerData *g_sData;
UA_UInt16 g_indexPub;
UA_UInt16 g_indexSub;

static void normalize(struct timespec *timeSpecValue)
{
    /* In nsec is bigger than sec, we increment the sec
     * and realign the nsec value
     */
    while (timeSpecValue->tv_nsec > (ONESEC_IN_NSEC - 1)) {
        timeSpecValue->tv_sec  += 1;
        timeSpecValue->tv_nsec -= ONESEC_IN_NSEC;
    }

    /* If the nsec is in negative, we shd go back to the
     * previous second, and realign the nanosec
     */
    while (timeSpecValue->tv_nsec < 0) {
        timeSpecValue->tv_sec  -= 1;
        timeSpecValue->tv_nsec += ONESEC_IN_NSEC;
    }
}

static pthread_t threadCreation(UA_Int16 threadPriority, size_t coreAffinity,
                                void *(*thread) (void *), char *applicationName,
                                void *serverConfig)
{
    struct sched_param schedParam;
    pthread_t threadID;
    cpu_set_t cpuset;
    UA_Int32 ret = 0;

    /* Set scheduler and thread priority */
    threadID = pthread_self();
    schedParam.sched_priority = threadPriority;

    ret = pthread_setschedparam(threadID, SCHED_FIFO, &schedParam);
    if (ret != 0) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "pthread_setschedparam: failed\n");
        exit(1);
    }

    /*
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,\
                "\npthread_setschedparam:%s Thread priority is %d \n", \
                applicationName, schedParam.sched_priority);
    */

    /* Set thread CPU affinity */
    CPU_ZERO(&cpuset);
    CPU_SET(coreAffinity, &cpuset);

    ret = pthread_setaffinity_np(threadID, sizeof(cpu_set_t), &cpuset);
    if (ret) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                     "pthread_setaffinity_np ret: %s\n", strerror(ret));
        exit(1);
    }

    /* TODO: check if this should be run earlier. Also check setRTpriority() */
    ret = pthread_create(&threadID, NULL, thread, serverConfig);
    if (ret != 0) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    ":%s Cannot create thread\n", applicationName);
    }

    if (CPU_ISSET(coreAffinity, &cpuset)) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                    "%s CPU CORE: %ld\n", applicationName, coreAffinity);
    }

   return threadID;
}

/* Custom publisher thread */
void *pub_thread(void *arg)
{
    struct timespec   nextnanosleeptime;
    struct timespec   temp_t;
    struct timespec   delay;
    UA_UInt64         tx_timestamp;
    UA_ServerCallback pubCallback;
    UA_Server         *server;
    UA_WriterGroup    *currentWriterGroup;
    UA_Int32          blank_counter = 0;
    UA_Int32          earlyOffsetNs = 0;
    UA_Int32          publishOffsetNs = 0;
    UA_Int32          publishDelaySec = 0;
    UA_UInt32         ind = 0;
    UA_UInt64         cycleTimeNs = 0;
    struct PublisherData *pData;

    /* Initialise value for nextnanosleeptime timespec */
    tx_timestamp = 0;
    nextnanosleeptime.tv_nsec           = 0;
    nextnanosleeptime.tv_sec            = 0;
    threadParams *threadArgumentsPublisher = (threadParams *)arg;
    server             = threadArgumentsPublisher->server;
    pubCallback        = threadArgumentsPublisher->callback;
    currentWriterGroup = (UA_WriterGroup *)threadArgumentsPublisher->data;
    pData              = (struct PublisherData *)g_sData->pubData;
    ind                = threadArgumentsPublisher->data_index;
    earlyOffsetNs      = pData[ind].earlyOffsetNs;
    publishOffsetNs    = pData[ind].publishOffsetNs;
    publishDelaySec    = pData[ind].publishDelaySec;
    cycleTimeNs        = g_sData->cycleTimeNs;
    delay.tv_sec       = 0;
    delay.tv_nsec      = 10;

    /* Define Ethernet ETF transport settings */
    UA_EthernetETFWriterGroupTransportDataType ethernetETFtransportSettings;
    memset(&ethernetETFtransportSettings, 0,
           sizeof(UA_EthernetETFWriterGroupTransportDataType));

    ethernetETFtransportSettings.txtime_enabled    = UA_TRUE;
#ifndef WITH_XDPTBS
    if (g_sData->useXDP) {
        ethernetETFtransportSettings.txtime_enabled    = UA_FALSE;
    }
#endif
    ethernetETFtransportSettings.transmission_time = 0;

    /* Encapsulate ETF config in transportSettings */
    UA_ExtensionObject transportSettings;
    memset(&transportSettings, 0, sizeof(UA_ExtensionObject));

    /* TODO: transportSettings encoding and type to be defined */
    transportSettings.content.decoded.data       = &ethernetETFtransportSettings;
    currentWriterGroup->config.transportSettings = transportSettings;

    /* Get current time and compute the next nanosleeptime to the nearest 5th second */
    clock_gettime(CLOCKID, &temp_t);
    tx_timestamp = ((temp_t.tv_sec + 9) / 10) * 10;
    /* Add delay for publisher (subscriber starts earlier) */
    tx_timestamp += publishDelaySec;

    tx_timestamp *= ONESEC_IN_NSEC;
    /* Add publish offset to tx time*/
    tx_timestamp += publishOffsetNs;

    /* First packet tx_timestamp */
    ethernetETFtransportSettings.transmission_time = tx_timestamp;

    while (g_running) {
        /* Calculate publisher wake up time using earlyOffsetNs
         * Publisher wakes up earlier to be able to catch
         * the ETF transmission time
         */
        nextnanosleeptime.tv_nsec = (tx_timestamp - earlyOffsetNs) % ONESEC_IN_NSEC;
        nextnanosleeptime.tv_sec = (tx_timestamp - earlyOffsetNs) / ONESEC_IN_NSEC;
        clock_nanosleep(CLOCKID, TIMER_ABSTIME, &nextnanosleeptime, NULL);

        /* Specifically for round-trip pubReturn ONLY. This blank_counter
         * condition only gets triggered if B-side starts earlier than A-side's
         * thread - due to user delay in starting A-side or timer misalignment.
         *
         * This check pauses pubReturn until sub has cleared this bit which
         * indicates valid data is available for transmission.
         * Regardless of this bit, the timestamp will keep rolling.
         *
         * Without this check, pubReturn might send blanks/invalid packets
         * and stall subReturn. We do implement several checks to help with
         * processing invalids, this is just a supplementary safeguard.
         *
         * We can't do this check elsewhere since once we've started pubCallback,
         * a packet WILL get sent, valid data or not. Preventing pubCallback is
         * the best option.
         */
        if (!g_roundtrip_pubReturn)
            blank_counter++; //Do nothing
        else {
            pubCallback(server, currentWriterGroup);
            /* There is a problem of increased delay in 5.10 and above kernel if there is
             * no sleep after pubCallback.
             * Suspicion of unyielding process in pub/sub API in open62541-iotg.
             */
            clock_nanosleep(CLOCKID, 0, &delay, NULL);
        }
        tx_timestamp += cycleTimeNs;
        ethernetETFtransportSettings.transmission_time = tx_timestamp;
    }
    debug("Blank counter: %d\n", blank_counter);

    UA_free(threadArgumentsPublisher);
    return (void *)NULL;
}

/* Custom subscriber thread */
void *sub_thread(void *arg)
{
    UA_Server         *server;
    UA_ReaderGroup    *currentReaderGroup;
    UA_ServerCallback subCallback;
    struct timespec   nextnanosleeptimeSub;
    struct timespec   delay;
    UA_UInt64         cycleTimeNs = 0;
    UA_UInt32         offsetNs = 0;
    UA_UInt32         ind = 0;
    struct SubscriberData *sData;

    threadParams *threadArgumentsSubscriber = (threadParams *)arg;
    server             = threadArgumentsSubscriber->server;
    subCallback        = threadArgumentsSubscriber->callback;
    currentReaderGroup = (UA_ReaderGroup *)threadArgumentsSubscriber->data;
    cycleTimeNs        = g_sData->cycleTimeNs;
    sData              = (struct SubscriberData *)g_sData->subData;
    ind                = threadArgumentsSubscriber->data_index;
    offsetNs           = sData[ind].offsetNs;
    delay.tv_sec       = 0;
    delay.tv_nsec      = 10;

    /* Get current time and compute the next nanosleeptime to the nearest 5th second */
    clock_gettime(CLOCKID, &nextnanosleeptimeSub);

    nextnanosleeptimeSub.tv_sec  = ((nextnanosleeptimeSub.tv_sec + 9) / 10) * 10;
    /* Add 3 secs delay for subscriber to start */
    nextnanosleeptimeSub.tv_sec += 3;
    nextnanosleeptimeSub.tv_nsec = offsetNs;
    normalize(&nextnanosleeptimeSub);

    while (g_running) {
        clock_nanosleep(CLOCKID, TIMER_ABSTIME, &nextnanosleeptimeSub, NULL);
        subCallback(server, currentReaderGroup);
        /* There is a problem of increased delay in 5.10 and above kernel if there is
        * no sleep after subCallback.
        * Suspicion of unyielding process in pub/sub API in open62541-iotg.
        */
        clock_nanosleep(CLOCKID, 0, &delay, NULL);
        nextnanosleeptimeSub.tv_nsec += cycleTimeNs;
        normalize(&nextnanosleeptimeSub);
    }

    UA_free(threadArgumentsSubscriber);
    return (void *)NULL;
}

/* The following 3 functions are originally/normally declared in ua_pubsub.h
 * But we want a customized cyclic interrupt as well as our own threads
 * so we use custom threads to do it. The library will call these functions
 * when it needs to register a callback, where our threads will be used instead
 * of ua_timer's
 */

UA_StatusCode
UA_PubSubManager_addRepeatedCallback(UA_Server *server, UA_ServerCallback callback,
                                     void *data, UA_Double interval_ms,
                                     UA_UInt64 *callbackId)
{

    /* Initialize arguments required for the thread to run */
    threadParams *params = (threadParams *) UA_malloc(sizeof(threadParams));
    if (!params)
        return UA_STATUSCODE_BADINTERNALERROR;

    /* Pass the value required for the threads */
    params->server      = server;
    params->data        = data;
    params->callback    = callback;
    params->interval_ms = interval_ms;
    params->callbackId  = callbackId;

    /* Check the writer group identifier and create the thread accordingly */
    UA_WriterGroup *tmpWriter = (UA_WriterGroup *) data;

    if (UA_NodeId_equal(&tmpWriter->identifier, &g_writerGroupIdent)) {
        char threadNamePub[10] = "Publisher";
        params->data_index = g_indexPub;
        g_thread[g_threadRun].id = threadCreation(PUB_THREAD_PRIORITY,
                                                  g_sData->pubData[g_indexPub].cpuAffinity,
                                                  pub_thread,
                                                  threadNamePub, params);
        g_indexPub++;
    }
    else {
        char threadNameSub[11] = "Subscriber";
        params->data_index = g_indexSub;
        g_thread[g_threadRun].id = threadCreation(SUB_THREAD_PRIORITY,
                                                  g_sData->subData[g_indexSub].cpuAffinity,
                                                  sub_thread,
                                                  threadNameSub, params);
        g_indexSub++;
    }

    g_threadRun++;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_PubSubManager_changeRepeatedCallbackInterval(UA_Server *server,
                                                UA_UInt64 callbackId,
                                                UA_Double interval_ms)
{
    (void) server;
    (void) callbackId;
    (void) interval_ms;
    /* Callback interval need not be modified as it is thread based.
     * The thread uses nanosleep for calculating cycle time and modification in
     * nanosleep value changes cycle time
     */
    return UA_STATUSCODE_GOOD;
}

void
UA_PubSubManager_removeRepeatedPubSubCallback(UA_Server *server,
                                              UA_UInt64 callbackId)
{
    (void) server;
    (void) callbackId;

    /* TODO move pthread_join here? */
}
