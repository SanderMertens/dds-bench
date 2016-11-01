/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2016 PrismTech
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE
 *
 *   for full copyright notice and license terms.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <config.h>
#include <ddsbench.h>
#include <example_utilities.h>
#include <example_error_sac.h>
#include <roundtrip.h>
#include <qos.h>

#ifdef GENERATING_EXAMPLE_DOXYGEN
GENERATING_EXAMPLE_DOXYGEN /* workaround doxygen bug */
#endif

/**
 * @addtogroup examplesdcpsRoundTripsac The Standalone C DCPS API RoundTrip example
 *
 * The RoundTrip example consists of a Ping and a Pong application. Ping sends sample
 * to Pong by writing to the Ping partition which Pong subscribes to. Pong them sends them
 * back to Ping by writing on the Pong partition which Ping subscribes to. Ping measure
 * the amount of time taken to write and read each sample as well as the total round trip
 * time to send a sample to Pong and receive it back.
 * @ingroup examplesdcpssac
 */
/** @{*/
/** @dir */
/** @file */

/**
 * This struct serves as a container holding initialised entities used by ping and pong.
 */
typedef struct
{
    /** The Topic used by ping and pong */
    DDS_Topic topic;
    /** The Topic used when a filter is specified */
    DDS_ContentFilteredTopic filter;
    /** The Publisher used by ping and pong */
    DDS_Publisher publisher;
    /** The DataWriter used by ping and pong */
    ddsbench_LatencyDataWriter writer;
    /** The Subscriber used by ping and pong */
    DDS_Subscriber subscriber;
    /** The DataReader used by ping and pong */
    ddsbench_LatencyDataReader reader;
    /** The WaitSet used by ping and pong */
    DDS_WaitSet waitSet;
    /** The StatusCondition used by ping and pong,
     * triggered when data is available to read */
    DDS_StatusCondition dataAvailable;

    /** The sample used to send data */
    ddsbench_Latency *data;
    /** The condition sequence used to store conditions returned by the WaitSet */
    DDS_ConditionSeq *conditions;
    /** The sequence used to hold samples received by the DataReader */
    DDS_sequence_ddsbench_Latency *samples;
    /** The sequence used to hold information about the samples received by the DataReader */
    DDS_SampleInfoSeq *info;
    /** The PublicationMatcheStatus */
    DDS_PublicationMatchedStatus *pms;

    ExampleTimeStats roundTrip;
    ExampleTimeStats writeAccess;
    ExampleTimeStats readAccess;
    ExampleTimeStats roundTripOverall;
    ExampleTimeStats writeAccessOverall;
    ExampleTimeStats readAccessOverall;
} Entities;

/**
* This function initialises the entities used by ping and pong, or exit
* on any failure.
*/
void initialise(Entities *e, const char *topicName, const char *pubPartition, const char *subPartition)
{
    DDS_PublisherQos *pubQos;
    DDS_DataWriterQos *dwQos;
    DDS_DataReaderQos *drQos;
    DDS_SubscriberQos *subQos;
    ddsbench_LatencyTypeSupport typeSupport;
    DDS_string typeSupportName;
    DDS_TopicQos *topicQos;
    DDS_ReturnCode_t status;

    /** The sample type is created and registered */
    typeSupport = ddsbench_LatencyTypeSupport__alloc();
    CHECK_HANDLE_MACRO(typeSupport);
    typeSupportName = ddsbench_LatencyTypeSupport_get_type_name(typeSupport);
    CHECK_HANDLE_MACRO(typeSupportName);
    status = ddsbench_LatencyTypeSupport_register_type(
        typeSupport, ddsbench_dp, typeSupportName);
    CHECK_STATUS_MACRO(status);

    /** A DDS_Topic is created for our sample type on the domain participant. */
    topicQos = ddsbench_getQos(ddsbench_qos);
    e->topic = DDS_DomainParticipant_create_topic(
        ddsbench_dp, topicName, typeSupportName,
        DDS_TOPIC_QOS_DEFAULT, 0, DDS_STATUS_MASK_NONE);
    CHECK_HANDLE_MACRO(e->topic);
    DDS_free(typeSupport);
    DDS_free(typeSupportName);

    /** A DDS_ContentFilteredTopic is created if a filter is specified. */
    if (ddsbench_filter) {
        DDS_StringSeq *parameterList = DDS_StringSeq__alloc();
        CHECK_HANDLE_MACRO(parameterList);
        e->filter = DDS_DomainParticipant_create_contentfilteredtopic(
            ddsbench_dp,
            ddsbench_filtername,
            e->topic,
            ddsbench_filter,
            parameterList);
        CHECK_HANDLE_MACRO(e->filter);
        DDS_free(parameterList);
    }

    /** A DDS_Publisher is created on the domain participant. */
    pubQos = DDS_PublisherQos__alloc();
    CHECK_HANDLE_MACRO(pubQos);
    status = DDS_DomainParticipant_get_default_publisher_qos(ddsbench_dp, pubQos);
    CHECK_STATUS_MACRO(status);
    pubQos->partition.name._length = 1;
    pubQos->partition.name._maximum = 1;
    pubQos->partition.name._release = TRUE;
    pubQos->partition.name._buffer = DDS_StringSeq_allocbuf(1);
    pubQos->partition.name._buffer[0] = DDS_string_alloc((DDS_unsigned_long)strlen(pubPartition) + 1);
    strcpy(pubQos->partition.name._buffer[0], pubPartition);
    e->publisher = DDS_DomainParticipant_create_publisher(ddsbench_dp, pubQos, 0, DDS_STATUS_MASK_NONE);
    CHECK_HANDLE_MACRO(e->publisher);
    DDS_free(pubQos);

    /** A DDS_DataWriter is created on the Publisher & Topic with a modififed Qos. */
    dwQos = DDS_DataWriterQos__alloc();
    CHECK_HANDLE_MACRO(dwQos);
    status = DDS_Publisher_get_default_datawriter_qos(e->publisher, dwQos);
    CHECK_STATUS_MACRO(status);
    status = DDS_Publisher_copy_from_topic_qos(e->publisher, dwQos, topicQos);
    CHECK_STATUS_MACRO(status);
    dwQos->writer_data_lifecycle.autodispose_unregistered_instances = FALSE;
    e->writer = DDS_Publisher_create_datawriter(e->publisher, e->topic, dwQos, 0, DDS_STATUS_MASK_NONE);
    CHECK_HANDLE_MACRO(e->writer);
    DDS_free(dwQos);

    /** A DDS_Subscriber is created on the domain participant. */
    subQos = DDS_SubscriberQos__alloc();
    CHECK_HANDLE_MACRO(subQos);
    status = DDS_DomainParticipant_get_default_subscriber_qos(ddsbench_dp, subQos);
    CHECK_STATUS_MACRO(status);
    subQos->partition.name._length = 1;
    subQos->partition.name._maximum = 1;
    subQos->partition.name._release = TRUE;
    subQos->partition.name._buffer = DDS_StringSeq_allocbuf(1);
    subQos->partition.name._buffer[0] = DDS_string_alloc((DDS_unsigned_long)strlen(subPartition) + 1);
    strcpy(subQos->partition.name._buffer[0], subPartition);
    e->subscriber = DDS_DomainParticipant_create_subscriber(ddsbench_dp, subQos, 0, DDS_STATUS_MASK_NONE);
    CHECK_HANDLE_MACRO(e->subscriber);
    DDS_free(subQos);

    /** A DDS_DataReader is created on the Subscriber & Topic with a modified QoS. */
    drQos = DDS_DataReaderQos__alloc();
    CHECK_HANDLE_MACRO(drQos);
    status = DDS_Subscriber_get_default_datareader_qos(e->subscriber, drQos);
    CHECK_STATUS_MACRO(status);
    status = DDS_Subscriber_copy_from_topic_qos(e->subscriber, drQos, topicQos);
    CHECK_STATUS_MACRO(status);
    if (!ddsbench_filter) {
        e->reader = DDS_Subscriber_create_datareader(
            e->subscriber, e->topic, drQos, 0, DDS_STATUS_MASK_NONE);
    } else {
        e->reader = DDS_Subscriber_create_datareader(
            e->subscriber, e->filter, drQos, 0, DDS_STATUS_MASK_NONE);
    }
    CHECK_HANDLE_MACRO(e->reader);
    DDS_free(drQos);

    DDS_free(topicQos);

    /** A DDS_StatusCondition is created which is triggered when data is available to read */
    e->dataAvailable = DDS_DataReader_get_statuscondition(e->reader);
    CHECK_HANDLE_MACRO(e->dataAvailable);
    status = DDS_StatusCondition_set_enabled_statuses(e->dataAvailable, DDS_DATA_AVAILABLE_STATUS);
    CHECK_STATUS_MACRO(status);

    /** A DDS_WaitSet is created and the data available status condition is attached */
    e->waitSet = DDS_WaitSet__alloc();
    CHECK_HANDLE_MACRO(e->waitSet);
    status = DDS_WaitSet_attach_condition(e->waitSet, e->dataAvailable);
    CHECK_STATUS_MACRO(status);

    status = DDS_WaitSet_attach_condition(e->waitSet, terminated);
    CHECK_STATUS_MACRO(status);

    /** Initialise data structures */
    e->data = ddsbench_Latency__alloc();
    CHECK_HANDLE_MACRO(e->data);

    e->conditions = DDS_ConditionSeq__alloc();
    CHECK_HANDLE_MACRO(e->conditions);

    e->samples = DDS_sequence_ddsbench_Latency__alloc();
    CHECK_HANDLE_MACRO(e->samples);

    e->info = DDS_SampleInfoSeq__alloc();
    CHECK_HANDLE_MACRO(e->info);

    e->pms = (DDS_PublicationMatchedStatus*)malloc(sizeof(DDS_PublicationMatchedStatus));
    CHECK_HANDLE_MACRO(e->pms);

    /** Initialise ExampleTimeStats used to track timing */
    e->roundTrip = exampleInitTimeStats();
    e->writeAccess = exampleInitTimeStats();
    e->readAccess = exampleInitTimeStats();
    e->roundTripOverall = exampleInitTimeStats();
    e->writeAccessOverall = exampleInitTimeStats();
    e->readAccessOverall = exampleInitTimeStats();
}

/**
* This function cleans up after the application has finished running
*/
void cleanup(Entities *e)
{
    DDS_ReturnCode_t status;
    DDS_free(e->data->payload._buffer);
    DDS_free(e->data);
    DDS_free(e->conditions);
    status = ddsbench_LatencyDataReader_return_loan(e->reader, e->samples, e->info);
    CHECK_STATUS_MACRO(status);
    DDS_free(e->samples);
    DDS_free(e->info);
    free(e->pms);
    status = DDS_WaitSet_detach_condition(e->waitSet, e->dataAvailable);
    CHECK_STATUS_MACRO(status);
    status = DDS_WaitSet_detach_condition(e->waitSet, terminated);
    CHECK_STATUS_MACRO(status);
    DDS_free(e->waitSet);

    exampleDeleteTimeStats(&e->roundTrip);
    exampleDeleteTimeStats(&e->writeAccess);
    exampleDeleteTimeStats(&e->readAccess);
    exampleDeleteTimeStats(&e->roundTripOverall);
    exampleDeleteTimeStats(&e->writeAccessOverall);
    exampleDeleteTimeStats(&e->readAccessOverall);

    exampleSleepMilliseconds(1000);
}

/**
 * This function performs the Ping role in this example.
 * @return 0 if a sample is successfully written, 1 otherwise.
 */
int ping(ddsbench_threadArg *arg)
{
    unsigned long payloadSize = 0;
    unsigned long long numSamples = 0;
    unsigned long timeOut = 0;
    DDS_boolean pongRunning;
    struct timeval startTime;
    struct timeval time;
    struct timeval preWriteTime;
    struct timeval postWriteTime;
    struct timeval preTakeTime;
    struct timeval postTakeTime;
    struct timeval difference = {0, 0};
    DDS_Duration_t waitTimeout = {1, 0};
    unsigned long long i;
    unsigned long elapsed = 0;
    DDS_boolean invalid = FALSE;
    DDS_boolean warmUp = TRUE;
    DDS_ReturnCode_t status;

    /** Initialise entities */
    Entities e;
    char pingPartition[32], pongPartition[32];
    sprintf(pingPartition, "ping_%d", arg->id);
    sprintf(pongPartition, "pong_%d", arg->id);
    initialise(&e, arg->topicName, pingPartition, pongPartition);

    setbuf(stdout, NULL);

    payloadSize = ddsbench_payload;

    e.data->payload._length = payloadSize;
    e.data->payload._maximum = payloadSize;
    e.data->payload._buffer = DDS_sequence_octet_allocbuf(payloadSize);
    for(i = 0; i < payloadSize; i++)
    {
        e.data->payload._buffer[i] = 'a';
    }

    startTime = exampleGetTime();
    printf("sub %d: warming up to stabilise performance...\n", arg->id);
    while(!DDS_GuardCondition_get_trigger_value(terminated) && exampleTimevalToMicroseconds(&difference) / US_IN_ONE_SEC < 5)
    {
        status = ddsbench_LatencyDataWriter_write(e.writer, e.data, DDS_HANDLE_NIL);
        CHECK_STATUS_MACRO(status);
        status = DDS_WaitSet_wait(e.waitSet, e.conditions, &waitTimeout);
        if(status != DDS_RETCODE_TIMEOUT)
        {
            CHECK_STATUS_MACRO(status);
            status = ddsbench_LatencyDataReader_take(e.reader, e.samples, e.info, DDS_LENGTH_UNLIMITED,
                                        DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
            CHECK_STATUS_MACRO(status);
            status = ddsbench_LatencyDataReader_return_loan(e.reader, e.samples, e.info);
            CHECK_STATUS_MACRO(status);
        }

        time = exampleGetTime();
        difference = exampleSubtractTimevalFromTimeval(&time, &startTime);
    }
    if(!DDS_GuardCondition_get_trigger_value(terminated))
    {
        warmUp = FALSE;
        printf("sub %d: Warm up complete.\n", arg->id);

        if (arg->id == ddsbench_subid) {
            printf("\n");
            printf("          Round trip measurements (in us)\n");
            printf("                      Round trip time [us]         Write-access time [us]       Read-access time [us]\n");
            printf("          Seconds     Count   median      min      Count   median      min      Count   median      min\n");
        }
    }

    startTime = exampleGetTime();
    for(i = 0; !DDS_GuardCondition_get_trigger_value(terminated); i++)
    {
        /** Write a sample that pong can send back */
        e.data->filter = i % 10;
        preWriteTime = exampleGetTime();
        status = ddsbench_LatencyDataWriter_write(e.writer, e.data, DDS_HANDLE_NIL);
        postWriteTime = exampleGetTime();
        CHECK_STATUS_MACRO(status);

        /** Wait for response from pong */
        status = DDS_WaitSet_wait(e.waitSet, e.conditions, &waitTimeout);
        if(status != DDS_RETCODE_TIMEOUT)
        {
            CHECK_STATUS_MACRO(status);

            /** Take sample and check that it is valid */
            preTakeTime = exampleGetTime();
            status = ddsbench_LatencyDataReader_take(e.reader, e.samples, e.info, DDS_LENGTH_UNLIMITED,
                                        DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
            postTakeTime = exampleGetTime();
            CHECK_STATUS_MACRO(status);

            if(!DDS_GuardCondition_get_trigger_value(terminated))
            {
                if(e.samples->_length != 1)
                {
                    fprintf(stdout, "sub %d: %s%d%s", arg->id, "ERROR: Ping received ", e.samples->_length,
                                " samples but was expecting 1. Are multiple pong applications running?\n");

                    cleanup(&e);
                    exit(0);
                }
                else if(!e.info->_buffer[0].valid_data)
                {
                    printf("sub %d: ERROR: Ping received an invalid sample. Has pong terminated already?\n", arg->id);

                    cleanup(&e);
                    exit(0);
                }
                else if (e.info->_buffer[0].instance_state != DDS_ALIVE_INSTANCE_STATE)
                {
                    printf("sub %d: ERROR: Ping received a not alive sample. Has pong terminated already?\n", arg->id);

                    cleanup(&e);
                    exit(0);
                }
            }
            status = ddsbench_LatencyDataReader_return_loan(e.reader, e.samples, e.info);
            CHECK_STATUS_MACRO(status);

            /** Update stats */
            difference = exampleSubtractTimevalFromTimeval(&postWriteTime, &preWriteTime);
            e.writeAccess = *exampleAddMicrosecondsToTimeStats(
                &e.writeAccess, exampleTimevalToMicroseconds(&difference));

            difference = exampleSubtractTimevalFromTimeval(&postTakeTime, &preTakeTime);
            e.readAccess = *exampleAddMicrosecondsToTimeStats(
                &e.readAccess, exampleTimevalToMicroseconds(&difference));

            difference = exampleSubtractTimevalFromTimeval(&postTakeTime, &preWriteTime);
            e.roundTrip = *exampleAddMicrosecondsToTimeStats(
                &e.roundTrip, exampleTimevalToMicroseconds(&difference));

            difference = exampleSubtractTimevalFromTimeval(&postWriteTime, &preWriteTime);
            e.writeAccessOverall = *exampleAddMicrosecondsToTimeStats(
                &e.writeAccessOverall, exampleTimevalToMicroseconds(&difference));

            difference = exampleSubtractTimevalFromTimeval(&postTakeTime, &preTakeTime);
            e.readAccessOverall = *exampleAddMicrosecondsToTimeStats(
                &e.readAccessOverall, exampleTimevalToMicroseconds(&difference));

            difference = exampleSubtractTimevalFromTimeval(&postTakeTime, &preWriteTime);
            e.roundTripOverall = *exampleAddMicrosecondsToTimeStats(
                &e.roundTripOverall, exampleTimevalToMicroseconds(&difference));

            /** Print stats each second */
            difference = exampleSubtractTimevalFromTimeval(&postTakeTime, &startTime);
            if(exampleTimevalToMicroseconds(&difference) > US_IN_ONE_SEC || (i && i == numSamples))
            {
                printf ("sub %3d: %8lu %9lu %8.0f %8lu %10lu %8.0f %8lu %10lu %8.0f %8lu\n",
                    arg->id,
                    elapsed + 1,
                    e.roundTrip.count,
                    exampleGetMedianFromTimeStats(&e.roundTrip),
                    e.roundTrip.min,
                    e.writeAccess.count,
                    exampleGetMedianFromTimeStats(&e.writeAccess),
                    e.writeAccess.min,
                    e.readAccess.count,
                    exampleGetMedianFromTimeStats(&e.readAccess),
                    e.readAccess.min);

                /** Reset stats for next run */
                exampleResetTimeStats(&e.roundTrip);
                exampleResetTimeStats(&e.writeAccess);
                exampleResetTimeStats(&e.readAccess);

                /** Set values for next run */
                startTime = exampleGetTime();
                elapsed += 1;
            }
        }
        else
        {
            elapsed += waitTimeout.sec;
        }
        if(timeOut && elapsed == timeOut)
        {
            status = DDS_GuardCondition_set_trigger_value(terminated, TRUE);
            CHECK_STATUS_MACRO(status);
        }
    }

    if(!warmUp)
    {
        /** Print overall stats */
        printf ("\nddsbench: sub %d: %9s %9lu %8.0f %8lu %10lu %8.0f %8lu %10lu %8.0f %8lu\n",
                    arg->id,
                    "Overall",
                    e.roundTripOverall.count,
                    exampleGetMedianFromTimeStats(&e.roundTripOverall),
                    e.roundTripOverall.min,
                    e.writeAccessOverall.count,
                    exampleGetMedianFromTimeStats(&e.writeAccessOverall),
                    e.writeAccessOverall.min,
                    e.readAccessOverall.count,
                    exampleGetMedianFromTimeStats(&e.readAccessOverall),
                    e.readAccessOverall.min);
    }

    cleanup(&e);
    return 0;
}

/**
 * Runs the Pong role in this example.
 * @return 0 if a sample is successfully read, 1 otherwise.
 */
int pong(ddsbench_threadArg *arg)
{
    DDS_ReturnCode_t status;
    DDS_Duration_t waitTimeout = DDS_DURATION_INFINITE;
    unsigned int i;

    /** Initialise entities */
    Entities e;
    char pingPartition[32], pongPartition[32];
    sprintf(pingPartition, "ping_%d", arg->id);
    sprintf(pongPartition, "pong_%d", arg->id);
    initialise(&e, arg->topicName, pongPartition, pingPartition);

    printf("pub %d: Waiting for samples from ping to send back...\n", arg->id);
    fflush(stdout);

    while(!DDS_GuardCondition_get_trigger_value(terminated))
    {
        /** Wait for a sample from ping */
        status = DDS_WaitSet_wait(e.waitSet, e.conditions, &waitTimeout);
        if (status != DDS_RETCODE_TIMEOUT)
        {
            CHECK_STATUS_MACRO(status);

            /** Take samples */
            status = ddsbench_LatencyDataReader_take(
                e.reader, e.samples, e.info, DDS_LENGTH_UNLIMITED, DDS_ANY_SAMPLE_STATE,
                DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
            CHECK_STATUS_MACRO(status);
            for (i = 0; !DDS_GuardCondition_get_trigger_value(terminated) && i < e.samples->_length; i++)
            {
                /** If writer has been disposed terminate pong */
                if(e.info->_buffer[i].instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
                {
                    printf("pub %d: Received termination request. Terminating.\n", arg->id);
                    status = DDS_GuardCondition_set_trigger_value(terminated, TRUE);
                    CHECK_STATUS_MACRO(status);
                    break;
                }
                /** If sample is valid, send it back to ping */
                else if(e.info->_buffer[i].valid_data)
                {
                    status = ddsbench_LatencyDataWriter_write(e.writer, &e.samples->_buffer[i],
                                                                    DDS_HANDLE_NIL);
                    CHECK_STATUS_MACRO(status);
                }
            }
            status = ddsbench_LatencyDataReader_return_loan(e.reader, e.samples, e.info);
            CHECK_STATUS_MACRO(status);
        }
    }

    cleanup(&e);
    return 0;
}

void* ddsbench_latencySubscriberThread(void *arg) {
    ping(arg);
}

void* ddsbench_latencyPublisherThread(void *arg) {
    pong(arg);
}
