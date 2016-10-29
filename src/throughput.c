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

#include <config.h>
#include <ddsbench.h>
#include <example_utilities.h>
#include <example_error_sac.h>
#include <qos.h>
#include <throughput.h>

#ifdef GENERATING_EXAMPLE_DOXYGEN
GENERATING_EXAMPLE_DOXYGEN /* workaround doxygen bug */
#endif

/**
 * @addtogroup examplesdcpsThroughputsac The Standalone C DCPS API Throughput example
 *
 * The Throughput example measures data throughput in bytes per second. The publisher
 * allows you to specify a payload size in bytes as well as allowing you to specify
 * whether to send data in bursts. The publisher will continue to send data forever
 * unless a time out is specified. The subscriber will receive data and output the
 * total amount received and the data rate in bytes per second. It will also indicate
 * if any samples were received out of order. A maximum number of cycles can be
 * specified and once this has been reached the subscriber will terminate and output
 * totals and averages.
 * @ingroup examplesdcpssac
 */
/** @{*/
/** @dir */
/** @file */

/**
 * This struct serves as a container holding initialised entities used by publisher
 */
typedef struct PubEntities {
    /** The DomainParticipantFactory used by the publisher */
    DDS_DomainParticipantFactory domainParticipantFactory;
    /** The DomainParticipant used by the publisher */
    DDS_DomainParticipant participant;
    /** The TypeSupport for the sample */
    ddsbench_ThroughputTypeSupport typeSupport;
    /** The Topic used by the publisher */
    DDS_Topic topic;
    /** The Publisher used by the publisher */
    DDS_Publisher publisher;
    /** The DataWriter used by the publisher */
    ddsbench_ThroughputDataWriter writer;
} PubEntities;

/**
 * This struct serves as a container holding initialised entities used by publisher
 */
typedef struct SubEntities {
    /** The TypeSupport for the sample */
    ddsbench_ThroughputTypeSupport typeSupport;
    /** The Topic used by the subscriber */
    DDS_Topic topic;
    /** The Topic used when a filter is specified */
    DDS_ContentFilteredTopic filter;
    /** The Subscriber used by the subscriber */
    DDS_Subscriber subscriber;
    /** The DataReader used by the subscriber */
    ddsbench_ThroughputDataReader reader;
    /** The WaitSet used by the subscriber */
    DDS_WaitSet waitSet;
    /** The StatusCondition used by the subscriber,
     * triggered when data is available to read */
    DDS_StatusCondition dataAvailable;
} SubEntities;

typedef struct HandleEntry {
    DDS_InstanceHandle_t handle;
    DDS_unsigned_long_long count;
} HandleEntry;

typedef struct HandleMap {
    unsigned long size;
    unsigned long max_size;
    HandleEntry *entries;
} HandleMap;

static HandleMap* HandleMap__alloc()
{
    HandleMap *map = malloc(sizeof(*map));
    CHECK_ALLOC_MACRO(map);
    map->size = 0;
    map->max_size = 10;
    map->entries = malloc(sizeof(HandleEntry) * map->max_size);
    CHECK_ALLOC_MACRO(map->entries);

    return map;
}

static void HandleMap__free(HandleMap *map)
{
    free(map->entries);
    free(map);
}

static HandleEntry* store_handle(HandleMap *map, DDS_InstanceHandle_t key)
{
    if (map->size == map->max_size) {
        map->entries = realloc(map->entries, sizeof(HandleEntry) * (map->max_size + 10));
        CHECK_ALLOC_MACRO(map->entries);
        map->max_size += 10;
    }

    map->entries[map->size].handle = key;
    map->entries[map->size].count = 0;
    map->size++;

    return &(map->entries[map->size - 1]);
}

static void remove_handle(HandleMap *map, DDS_InstanceHandle_t key)
{
    HandleEntry *entry = NULL;
    unsigned long i;

    for (i = 0; i < map->size; i++) {
        entry = &(map->entries[i]);
        if (entry->handle == key) {
            map->size --;
            if (i != map->size) {
                entry->handle = map->entries[map->size].handle;
                entry->count = map->entries[map->size].count;
                entry->count = 0;
            }
            break;
        }
    }
}

static HandleEntry* retrieve_handle(HandleMap *map, DDS_InstanceHandle_t key)
{
    HandleEntry *entry = NULL;
    unsigned long i;

    for (i = 0; i < map->size; i++) {
        entry = &(map->entries[i]);
        if (entry->handle == key) {
            break;
        } else {
            entry = NULL;
        }
    }

    return entry;
}

static void copy_handles(HandleMap *from, HandleMap *to)
{
    unsigned long i;
    HandleEntry *fromHandle, *toHandle;

    for (i=0; i < from->size; i++) {
        fromHandle = &(from->entries[i]);
        toHandle = retrieve_handle(to, fromHandle->handle);
        if (!toHandle) {
            toHandle = store_handle(to, fromHandle->handle);
        }
        toHandle->count = fromHandle->count;
    }
}

#define BYTES_PER_SEC_TO_MEGABITS_PER_SEC 125000
#define BYTES_IN_MEGABYTE 1000000

/**
 * This function performs the publisher role in this example.
 * @return 0 if a sample is successfully written, 1 otherwise.
 */
int publisher(ddsbench_threadArg *arg)
{
    int result = EXIT_SUCCESS;
    unsigned long payloadSize = 4096;
    unsigned long burstInterval = 0;
    unsigned long timeOut = 0;
    int burstSize = 1;
    char *partitionName = "Throughput example";
    PubEntities *e = malloc(sizeof(*e));
    ddsbench_Throughput sample;
    DDS_ReturnCode_t status;

    sample.payload._buffer = NULL;

    payloadSize = ddsbench_payload; /* The size of the payload in bytes */
    burstInterval = 0; /* The time interval between each burst in ms */
    burstSize = 1; /* The number of samples to send each burst */
    timeOut = 0;
    partitionName = "throughput"; /* The name of the partition */

    /** Initialise entities */
    {
        DDS_PublisherQos *pubQos;
        DDS_DataWriterQos *dwQos;
        DDS_string typename;

        /** The sample type is created and registered */
        e->typeSupport = ddsbench_ThroughputTypeSupport__alloc();
        typename = ddsbench_ThroughputTypeSupport_get_type_name(e->typeSupport);
        status = ddsbench_ThroughputTypeSupport_register_type(e->typeSupport, ddsbench_dp, typename);
        CHECK_STATUS_MACRO(status);

        /** A DDS_Topic is created for our sample type on the domain participant. */
        DDS_TopicQos *topicQos = ddsbench_getQos(ddsbench_qos);
        e->topic = DDS_DomainParticipant_create_topic(
            ddsbench_dp, arg->topicName, typename, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
        CHECK_HANDLE_MACRO(e->topic);
        DDS_free(typename);

        /** A DDS_Publisher is created on the domain participant. */
        pubQos = DDS_PublisherQos__alloc();
        status = DDS_DomainParticipant_get_default_publisher_qos(ddsbench_dp, pubQos);
        CHECK_STATUS_MACRO(status);
        pubQos->partition.name._release = TRUE;
        pubQos->partition.name._buffer = DDS_StringSeq_allocbuf(1);
        pubQos->partition.name._length = 1;
        pubQos->partition.name._maximum = 1;
        pubQos->partition.name._buffer[0] = DDS_string_dup(partitionName);
        e->publisher = DDS_DomainParticipant_create_publisher(ddsbench_dp, pubQos, NULL, 0);
        CHECK_HANDLE_MACRO(e->publisher);
        DDS_free(pubQos);

        /** A DDS_DataWriter is created on the Publisher & Topic with a modififed Qos. */
        dwQos = DDS_DataWriterQos__alloc();
        status = DDS_Publisher_get_default_datawriter_qos(e->publisher, dwQos);
        CHECK_STATUS_MACRO(status);
        status = DDS_Publisher_copy_from_topic_qos(e->publisher, dwQos, topicQos);
        CHECK_STATUS_MACRO(status);
        dwQos->history.kind = DDS_KEEP_ALL_HISTORY_QOS;
        dwQos->resource_limits.max_samples = 100;
        e->writer = DDS_Publisher_create_datawriter(e->publisher, e->topic, dwQos, NULL, 0);
        CHECK_HANDLE_MACRO(e->writer);
        DDS_free(dwQos);
    }

    /** Fill the sample payload with data */
    {
        unsigned long i;

        sample.id = arg->id;
        sample.count = 0;
        sample.payload._buffer = DDS_sequence_octet_allocbuf(payloadSize);
        sample.payload._length = payloadSize;
        sample.payload._maximum = payloadSize;
        for (i = 0; i < payloadSize; i++) {
            sample.payload._buffer[i] = 'a';
        }
    }

    /* Register the sample instance and write samples repeatedly or until time out */
    {
        DDS_InstanceHandle_t handle;
        struct timeval pubStart, burstStart;
        int burstCount = 0;
        int timedOut = FALSE;

        handle = ddsbench_ThroughputDataWriter_register_instance(e->writer, &sample);
        pubStart = exampleGetTime();
        burstStart = exampleGetTime();

        unsigned long long i;
        for (i = 0; !DDS_GuardCondition_get_trigger_value(terminated) && !timedOut; i++) {
            sample.filter = i % 10;

            /** Write data until burst size has been reached */
            if (burstCount < burstSize) {
                do {
                    status = ddsbench_ThroughputDataWriter_write(e->writer, &sample, handle);
                    if (status == DDS_RETCODE_TIMEOUT) {
                        printf("pub %d: timeout, retrying in 100msec\n", arg->id);
                        exampleSleepMilliseconds(100);
                    }
                } while (status == DDS_RETCODE_TIMEOUT);
                CHECK_STATUS_MACRO(status);
                sample.count++;
                burstCount++;
            }
            /** Sleep until burst interval has passed */
            else if(burstInterval) {
                struct timeval time = exampleGetTime();
                struct timeval deltaTv = exampleSubtractTimevalFromTimeval(&time, &burstStart);
                unsigned long deltaTime = exampleTimevalToMicroseconds(&deltaTv) / US_IN_ONE_MS;
                if (deltaTime < burstInterval) {
                    exampleSleepMilliseconds(burstInterval - deltaTime);
                }
                burstStart = exampleGetTime();
                burstCount = 0;
            }
            else {
                burstCount = 0;
            }

            if (timeOut) {
                struct timeval now = exampleGetTime();
                struct timeval deltaTv = exampleSubtractTimevalFromTimeval(&now, &pubStart);
                if ((exampleTimevalToMicroseconds(&deltaTv) / US_IN_ONE_SEC) > timeOut) {
                    timedOut = TRUE;
                }
            }
        }

        if (DDS_GuardCondition_get_trigger_value(terminated)) {
            printf("pub %d: Terminated, %llu samples written.\n", arg->id, sample.count);
        } else {
            printf("pub %d: Timed out, %llu samples written.\n", arg->id, sample.count);
        }
    }

    /** Cleanup entities */
    DDS_free(e->typeSupport);
    DDS_free(sample.payload._buffer);
    DDS_free(terminated);
    free(e);

    return result;
}

/**
 * This function calculates the number of samples received
 *
 * @param count1 the map tracking sample count values
 * @param count2 the map tracking sample count start or previous values
 * @param prevCount if set to true, count2's value should be set to count1 after adding to total
 * @return the number of samples received
 */
unsigned long long samplesReceived(HandleMap *count1, HandleMap *count2, int prevCount)
{
    unsigned long long total = 0;
    unsigned long i;
    HandleEntry *pubCount1, *pubCount2;

    for (i = 0; i < count1->size; i++) {
        pubCount1 = &(count1->entries[i]);
        pubCount2 = retrieve_handle(count2, pubCount1->handle);
        if (!pubCount2) {
            pubCount2 = store_handle(count2, pubCount1->handle);
        }

        total += pubCount1->count - pubCount2->count;
        if (prevCount) {
            pubCount2->count = pubCount1->count;
        }
    }
    return total;
}

int subscriber(ddsbench_threadArg *arg)
{
    int result = EXIT_SUCCESS;
    unsigned long long maxCycles = 0;
    unsigned long pollingDelay = 0;
    char *partitionName = "Throughput example";
    SubEntities *e = malloc(sizeof(*e));
    DDS_ReturnCode_t status;

    maxCycles = 10; /* The number of times to output statistics before terminating */
    pollingDelay = 0; /* The number of ms to wait between reads (0 = event based) */
    partitionName = "throughput"; /* The name of the partition */

    /** Initialise entities */
    {
        DDS_SubscriberQos *subQos;
        DDS_DataReaderQos *drQos;
        DDS_string typename;

        /** The sample type is created and registered */
        e->typeSupport = ddsbench_ThroughputTypeSupport__alloc();
        typename = ddsbench_ThroughputTypeSupport_get_type_name(e->typeSupport);
        status = ddsbench_ThroughputTypeSupport_register_type(
            e->typeSupport, ddsbench_dp, typename);
        CHECK_STATUS_MACRO(status);

        /** A DDS_Topic is created for our sample type on the domain participant. */
        DDS_TopicQos *topicQos = ddsbench_getQos(ddsbench_qos);
        e->topic = DDS_DomainParticipant_create_topic(
            ddsbench_dp, arg->topicName, typename, DDS_TOPIC_QOS_DEFAULT, NULL, 0);
        CHECK_HANDLE_MACRO(e->topic);
        DDS_free(typename);

        /** Create a ContentFilteredTopic if a filter is specified. */
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
        } else {
            e->filter = NULL;
        }

        /** A DDS_Subscriber is created on the domain participant. */
        subQos = DDS_SubscriberQos__alloc();
        status = DDS_DomainParticipant_get_default_subscriber_qos(ddsbench_dp, subQos);
        CHECK_STATUS_MACRO(status);
        subQos->partition.name._buffer = DDS_StringSeq_allocbuf(1);
        subQos->partition.name._length = 1;
        subQos->partition.name._maximum = 1;
        subQos->partition.name._release = TRUE;
        subQos->partition.name._buffer[0] = DDS_string_dup(partitionName);
        e->subscriber = DDS_DomainParticipant_create_subscriber(ddsbench_dp, subQos, NULL, 0);
        CHECK_HANDLE_MACRO(e->subscriber);
        DDS_free(subQos);

        /** A DDS_DataReader is created on the Publisher & Topic with a modififed Qos. */
        drQos = DDS_DataReaderQos__alloc();
        status = DDS_Subscriber_get_default_datareader_qos(e->subscriber, drQos);
        CHECK_STATUS_MACRO(status);
        status = DDS_Subscriber_copy_from_topic_qos(e->subscriber, drQos, topicQos);
        CHECK_STATUS_MACRO(status);
        drQos->history.kind = DDS_KEEP_ALL_HISTORY_QOS;
        drQos->resource_limits.max_samples = 400;
        if (!e->filter) {
            e->reader = DDS_Subscriber_create_datareader(e->subscriber, e->topic, drQos, NULL, 0);
        } else {
            e->reader = DDS_Subscriber_create_datareader(e->subscriber, e->filter, drQos, NULL, 0);
        }
        CHECK_HANDLE_MACRO(e->reader);
        DDS_free(drQos);

        /** A DDS_StatusCondition is created which is triggered when data is available to read */
        e->dataAvailable = ddsbench_ThroughputDataReader_get_statuscondition(e->reader);
        CHECK_HANDLE_MACRO(e->dataAvailable);
        status = DDS_StatusCondition_set_enabled_statuses(e->dataAvailable, DDS_DATA_AVAILABLE_STATUS);
        CHECK_STATUS_MACRO(status);

        /** A DDS_WaitSet is created and the data available status condition is attached */
        e->waitSet = DDS_WaitSet__alloc();
        status = DDS_WaitSet_attach_condition(e->waitSet, e->dataAvailable);
        CHECK_STATUS_MACRO(status);

        status = DDS_WaitSet_attach_condition(e->waitSet, terminated);
        CHECK_STATUS_MACRO(status);
    }

    /* Read samples until the maxCycles has been reached (0 = infinite) */
    {
        unsigned long long cycles = 0;

        DDS_unsigned_long i;
        DDS_InstanceHandle_t ph;

        DDS_Duration_t infinite = DDS_DURATION_INFINITE;
        HandleMap *count = HandleMap__alloc();
        HandleMap *startCount = HandleMap__alloc();
        HandleMap *prevCount = HandleMap__alloc();
        HandleEntry *pubCount = NULL;
        HandleEntry *pubStartCount = NULL;
        unsigned long long outOfOrder = 0;
        unsigned long long received = 0;
        unsigned long long prevReceived = 0;
        unsigned long long deltaReceived = 0;

        struct timeval time = { 0, 0} ;
        struct timeval startTime = { 0, 0 };
        struct timeval prevTime = { 0, 0 };
        struct timeval deltaTv;

        DDS_ConditionSeq *conditions = DDS_ConditionSeq__alloc();
        DDS_sequence_ddsbench_Throughput *samples = DDS_sequence_ddsbench_Throughput__alloc();
        DDS_SampleInfoSeq *info = DDS_SampleInfoSeq__alloc();
        unsigned long payloadSize = 0;
        double deltaTime = 0;

        if (arg->id == ddsbench_subid) {
            printf("\n");
            printf("Throughput measurements\n");
            printf("          Total Received        Missing   Transfer rate              Publishers\n");
            printf("        %9s %11s %9s %9s %16s %7s\n", "samples", "bytes", "samples", "samples", "bytes", "count");
        }

        while (!DDS_GuardCondition_get_trigger_value(terminated)) {
            /** If polling delay is set */
            if (pollingDelay) {
                /** Sleep before polling again */
                exampleSleepMilliseconds(pollingDelay);
            } else {
                /** Wait for samples */
                status = DDS_WaitSet_wait(e->waitSet, conditions, &infinite);
                CHECK_STATUS_MACRO(status);
            }

            /** Take samples and iterate through them */
            status = ddsbench_ThroughputDataReader_take(e->reader, samples, info, DDS_LENGTH_UNLIMITED,
                DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
            CHECK_STATUS_MACRO(status);
            for (i = 0; !DDS_GuardCondition_get_trigger_value(terminated) && i < samples->_length; i++) {
                ph = info->_buffer[i].publication_handle;
                if (info->_buffer[i].instance_state != DDS_ALIVE_INSTANCE_STATE){
                    printf("sub %2d: lost publisher %d\n", arg->id, samples->_buffer[i].id);
                    remove_handle(count, ph);
                } else if (info->_buffer[i].valid_data) {
                    /** Check that the sample is the next one expected */
                    pubCount = retrieve_handle(count, ph);
                    pubStartCount = retrieve_handle(startCount, ph);
                    if (!pubCount && !pubStartCount) {
                        pubCount = store_handle(count, ph);
                        pubStartCount = store_handle(startCount, ph);
                        pubCount->count = samples->_buffer[i].count;
                        pubStartCount->count = samples->_buffer[i].count;
                    }
                    if (samples->_buffer[i].count != pubCount->count) {
                        outOfOrder++;
                    }
                    pubCount->count = samples->_buffer[i].count + 1;

                    /** Add the sample payload size to the total received */
                    payloadSize = samples->_buffer[i].payload._length;
                    received += payloadSize + 8;
                }
            }

            /** Check that at lease on second has passed since the last output */
            time = exampleGetTime();
            if (exampleTimevalToMicroseconds(&time) > (exampleTimevalToMicroseconds(&prevTime) + US_IN_ONE_SEC)) {
                /** If not the first iteration */
                if (exampleTimevalToMicroseconds(&prevTime)) {
                    /**
                     * Calculate the samples and bytes received and the time passed since the
                     * last iteration and output
                     */
                    deltaReceived = received - prevReceived;
                    deltaTv = exampleSubtractTimevalFromTimeval(&time, &prevTime);
                    deltaTime = (double)exampleTimevalToMicroseconds(&deltaTv) / US_IN_ONE_SEC;

                    printf("sub %2d: %8.2lfK %9.2lfMB %9llu %8.2lfK %9.2lf Mbit/s %7lu\n",
                        arg->id,
                        (double)samplesReceived(count, startCount, FALSE) / (double)1000,
                        (double)received / (double)BYTES_IN_MEGABYTE,
                        outOfOrder,
                        (samplesReceived(count, prevCount, TRUE) / deltaTime) / 1000,
                        ((double)deltaReceived / (double)BYTES_PER_SEC_TO_MEGABITS_PER_SEC) / (double)deltaTime,
                        count->size);
                    fflush (stdout);
                    cycles++;
                } else {
                    /* Copy entries from startCount map to prevCount map */
                    copy_handles(startCount, prevCount);

                    /** Set the start time if it is the first iteration */
                    startTime = time;
                }
                /** Update the previous values for next iteration */
                copy_handles(count, prevCount);
                prevReceived = received;
                prevTime = time;
            }

            status = ddsbench_ThroughputDataReader_return_loan(e->reader, samples, info);
            CHECK_STATUS_MACRO(status);
        }

        /** Output totals and averages */
        deltaTv = exampleSubtractTimevalFromTimeval(&time, &startTime);
        deltaTime = (double)exampleTimevalToMicroseconds(&deltaTv) / US_IN_ONE_SEC;
        printf("\nTotal received: %llu samples, %llu bytes\n",
            samplesReceived(count, startCount, FALSE), received);
        printf("Out of order: %llu samples\n",
            outOfOrder);
        printf("Average transfer rate: %.2lf samples/s, %.2lf Mbit/s\n",
            samplesReceived(count, startCount, FALSE) / deltaTime,
            ((double)received / BYTES_PER_SEC_TO_MEGABITS_PER_SEC) / deltaTime);

        DDS_free(conditions);
        DDS_free(samples);
        DDS_free(info);
        HandleMap__free(count);
        HandleMap__free(startCount);
        HandleMap__free(prevCount);
    }

    /** Cleanup entities */
    DDS_free(e->waitSet);
    DDS_free(e->typeSupport);
    free(e);

    return result;
}

void* ddsbench_throughputSubscriberThread(void *arg) {
    subscriber(arg);
}

void* ddsbench_throughputPublisherThread(void *arg) {
    publisher(arg);
}
