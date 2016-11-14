
#include <ddsbench.h>
#include <../idl/ddsbench.h>
#include <lite.h>

#define NS_IN_ONE_SEC 1000000000
#define BYTES_PER_SEC_TO_MEGABITS_PER_SEC 125000
#define MAX_SAMPLES 100

typedef struct HandleEntry
{
  dds_instance_handle_t handle;
  unsigned long long count;
  struct HandleEntry * next;
} HandleEntry;

typedef struct HandleMap
{
  HandleEntry *entries;
} HandleMap;

static unsigned long cycles = 0;
static dds_instance_handle_t ph = 0;

static HandleMap * imap;
static HandleEntry * current = NULL;
static unsigned long long outOfOrder = 0;
static unsigned long long total_bytes = 0;
static unsigned long long total_samples = 0;

static dds_time_t startTime = 0;
static dds_time_t time_now = 0;

static unsigned long payloadSize = 0;
static double deltaTime = 0;

static ThroughputModule_DataType data [MAX_SAMPLES];
static void * samples[MAX_SAMPLES];

static dds_condition_t gCond;

/*
 * This struct contains all of the entities used in the publisher and subscriber.
 */

static HandleMap * HandleMap__alloc (void)
{
  HandleMap * map = malloc (sizeof (*map));
  memset (map, 0, sizeof (*map));
  return map;
}

static void HandleMap__free (HandleMap *map)
{
  HandleEntry * entry;

  while (map->entries)
  {
    entry = map->entries;
    map->entries = entry->next;
    free (entry);
  }
  free (map);
}

static HandleEntry* store_handle (HandleMap *map, dds_instance_handle_t key)
{
  HandleEntry * entry = malloc (sizeof (*entry));
  memset (entry, 0, sizeof (*entry));

  entry->handle = key;
  entry->next = map->entries;
  map->entries = entry;

  return entry;
}

static HandleEntry* retrieve_handle (HandleMap *map, dds_instance_handle_t key)
{
  HandleEntry * entry = map->entries;

  while (entry)
  {
    if (entry->handle == key)
    {
      break;
    }
    entry = entry->next;
  }
  return entry;
}

static void data_available_handler (dds_entity_t reader)
{
  static dds_time_t prev_time = 0;
  dds_time_t deltaTv;
  static bool first_batch = true;
  int samples_received;
  dds_sample_info_t info [MAX_SAMPLES];
  int i;

  if (startTime == 0)
  {
    startTime = dds_time ();
  }

  /* Take samples and iterate through them */

  samples_received = dds_take (reader, samples, MAX_SAMPLES, info, 0);
  DDS_ERR_CHECK (samples_received, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  for (i = 0; !dds_condition_triggered (terminated) && i < samples_received; i++)
  {
    if (info[i].valid_data)
    {
      ph = info[i].publication_handle;
      current = retrieve_handle (imap, ph);
      ThroughputModule_DataType * this_sample = &data[i];

      if (current == NULL)
      {
        current = store_handle (imap, ph);
        current->count = this_sample->count;
      }

      if (this_sample->count != current->count)
      {
        /* Temporarily disable outOfOrder- this doesn't work with multiple
         * subscribers because of the global administration. Need listener
         * userdata to be able to accurately keep track of this */
        /* outOfOrder++; */
      } else {
      }
      current->count = this_sample->count + 1;

      /* Add the sample payload size to the total received */

      payloadSize = this_sample->payload._length;
      total_bytes += payloadSize + 8;
      total_samples++;
    }
  }

  /* Check that at least one second has passed since the last output */
  time_now = dds_time ();
  if (time_now > (prev_time + DDS_SECS (1)))
  {
    if (!first_batch)
    {
      deltaTv = time_now - prev_time;
      deltaTime = (double) deltaTv / DDS_NSECS_IN_SEC;
      prev_time = time_now;

      if (gCond)
      {
        dds_guard_trigger (gCond);
      }
    }
    else
    {
      prev_time = time_now;
      first_batch = false;
    }
  }
}

int tsub(ddsbench_threadArg *arg)
{
  int status;
  unsigned long i;
  int result = EXIT_SUCCESS;
  unsigned long long maxCycles = 0;
  unsigned long pollingDelay = 0;
  char *partitionName;

  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t subscriber;
  dds_entity_t reader;
  dds_waitset_t waitSet;
  dds_readerlistener_t rd_listener;

  unsigned long long prev_samples = 0;
  unsigned long long prev_bytes = 0;
  dds_time_t deltaTv;

  status = dds_init (0, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  char threadName[16];
  sprintf(threadName, "tsub_%d", arg->id);
  status = dds_thread_init (threadName);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  maxCycles = 0; /* The number of times to output statistics before terminating */
  pollingDelay = arg->ctx->pollingdelay; /* The number of ms to wait between reads (0 = event based) */
  partitionName = "throughput"; /* The name of the partition */

  /* Initialise entities */

  {
    const char *subParts[1];
    dds_qos_t *subQos = dds_qos_create ();
    dds_qos_t *drQos = dds_qos_create ();
    uint32_t maxSamples = 400;
    dds_attach_t wsresults[1];
    size_t wsresultsize = 1U;
    dds_duration_t infinite = DDS_INFINITY;

    /* A Participant is created for the default domain. */

    status = dds_participant_create (&participant, DDS_DOMAIN_DEFAULT, NULL, NULL);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* A Topic is created for our sample type on the domain participant. */

    topic = dds_topic_find(participant, arg->topicName);
    if (!topic) {
      status = dds_topic_create (participant, &topic, &ThroughputModule_DataType_desc, arg->topicName, NULL, NULL);
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    }

    /* A Subscriber is created on the domain participant. */

    subParts[0] = partitionName;
    dds_qset_partition (subQos, 1, subParts);
    status = dds_subscriber_create (participant, &subscriber, subQos, NULL);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    dds_qos_delete (subQos);

    /* A Reader is created on the Subscriber & Topic with a modified Qos. */

    dds_qset_reliability (drQos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
    dds_qset_history (drQos, DDS_HISTORY_KEEP_ALL, 0);
    dds_qset_resource_limits (drQos, maxSamples, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);

    memset (&rd_listener, 0, sizeof (rd_listener));
    rd_listener.on_data_available = data_available_handler;

    status = dds_reader_create (subscriber, &reader, topic, drQos, &rd_listener);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    dds_qos_delete (drQos);

    /* A Read Condition is created which is triggered when data is available to read */

    waitSet = dds_waitset_create ();

    gCond = dds_guardcondition_create ();
    status = dds_waitset_attach (waitSet, gCond, gCond);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    terminated = dds_guardcondition_create ();
    status = dds_waitset_attach (waitSet, terminated, terminated);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* Read samples until the maxCycles has been reached (0 = infinite) */

    imap = HandleMap__alloc ();

    memset (data, 0, sizeof (data));
    for (int i = 0; i < MAX_SAMPLES; i++)
    {
      samples[i] = &data[i];
    }

    printf ("Waiting for samples...\n");

    while (!dds_condition_triggered (terminated) && (maxCycles == 0 || cycles < maxCycles))
    {
      status = dds_waitset_wait (waitSet, wsresults, wsresultsize, infinite);
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

      if ((status > 0 ) && (dds_condition_triggered (gCond)))
      {
        dds_guard_reset (gCond);
      }

      printf
      (
        "Payload size: %lu | Total received: %llu samples, %llu bytes | Out of order: %llu samples "
        "Transfer rate: %.2lf samples/s, %.2lf Mbit/s\n",
        payloadSize, total_samples, total_bytes, outOfOrder,
        (total_samples - prev_samples) / deltaTime,
        ((total_bytes - prev_bytes) / BYTES_PER_SEC_TO_MEGABITS_PER_SEC) / deltaTime
      );
      cycles++;

      /* Update the previous values for next iteration */

      prev_bytes = total_bytes;
      prev_samples = total_samples;
    }

    /* Disable callbacks */

    dds_status_set_enabled (reader, 0);

    /* Output totals and averages */

    deltaTv = time_now - startTime;
    deltaTime = deltaTv / DDS_NSECS_IN_SEC;
    printf ("\nTotal received: %llu samples, %llu bytes\n", total_samples, total_bytes);
    printf ("Out of order: %llu samples\n", outOfOrder);
    printf ("Average transfer rate: %.2lf samples/s, ", total_samples / deltaTime);
    printf ("%.2lf Mbit/s\n", (total_bytes / BYTES_PER_SEC_TO_MEGABITS_PER_SEC) / deltaTime);

    HandleMap__free (imap);
  }

  /* Clean up */

  for (i = 0; i < MAX_SAMPLES; i++)
  {
    ThroughputModule_DataType_free (&data[i], DDS_FREE_CONTENTS);
  }

  status = dds_waitset_detach (waitSet, terminated);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  status = dds_waitset_detach (waitSet, gCond);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_condition_delete (terminated);
  dds_condition_delete (gCond);
  status = dds_waitset_delete (waitSet);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_entity_delete (participant);
  dds_fini ();

  return result;
}

int tpub(ddsbench_threadArg *arg) {
  unsigned long i;
  int status;
  int result = EXIT_SUCCESS;
  uint32_t payloadSize = 8192;
  unsigned int burstInterval = 0;
  unsigned int timeOut = 0;
  uint32_t maxSamples = 100;
  int burstSize = 1;
  bool timedOut = false;
  char * partitionName;
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t publisher;
  dds_entity_t writer;
  dds_time_t pubStart;
  dds_time_t now;
  dds_time_t deltaTv;
  ThroughputModule_DataType sample;
  dds_publication_matched_status_t pms;
  const char *pubParts[1];
  dds_qos_t *pubQos;
  dds_qos_t *dwQos;

  status = dds_init (0, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  char threadName[16];
  sprintf(threadName, "tpub_%d", arg->id);
  status = dds_thread_init (threadName);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  payloadSize = arg->ctx->payload; /* The size of the payload in bytes */
  burstInterval = arg->ctx->burstinterval; /* The time interval between each burst in ms */
  burstSize = arg->ctx->burstsize; /* The number of samples to send each burst */
  timeOut = 0; /* The number of seconds the publisher should run for (0 = infinite) */
  partitionName = "throughput"; /* The name of the partition */

  printf ("payloadSize: %i bytes burstInterval: %u ms burstSize: %d timeOut: %u seconds partitionName: %s\n",
    payloadSize, burstInterval, burstSize, timeOut, partitionName);

  /* A domain participant is created for the default domain. */

  status = dds_participant_create (&participant, DDS_DOMAIN_DEFAULT, NULL, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  terminated = dds_guardcondition_create ();

  /* A topic is created for our sample type on the domain participant. */

  topic  = dds_topic_find(participant, arg->topicName);
  if (!topic) {
    status = dds_topic_create (participant, &topic, &ThroughputModule_DataType_desc, arg->topicName, NULL, NULL);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  }

  /* A publisher is created on the domain participant. */

  pubQos = dds_qos_create ();
  pubParts[0] = partitionName;
  dds_qset_partition (pubQos, 1, pubParts);
  status = dds_publisher_create (participant, &publisher, pubQos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (pubQos);

  /* A DataWriter is created on the publisher. */

  dwQos = dds_qos_create ();
  dds_qset_reliability (dwQos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (dwQos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_resource_limits (dwQos, maxSamples, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
  status = dds_writer_create (publisher, &writer, topic, dwQos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (dwQos);

  /* Enable write batching */

  dds_write_set_batch (true);

  /* Fill the sample payload with data */

  sample.count = 0;
  sample.payload._buffer = dds_alloc (payloadSize);
  sample.payload._length = payloadSize;
  sample.payload._release = true;
  for (i = 0; i < payloadSize; i++)
  {
    sample.payload._buffer[i] = 'a';
  }

  /* Wait until have a reader */

  pubStart = dds_time ();
  do
  {
    dds_sleepfor (DDS_MSECS (100));
    status = dds_get_publication_matched_status (writer, &pms);
    if (timeOut)
    {
      now = dds_time ();
      deltaTv = now - pubStart;
      if ((deltaTv) > DDS_SECS (timeOut))
      {
        timedOut = true;
      }
    }
  }
  while (pms.total_count == 0 && !timedOut);

  /* Register the sample instance and write samples repeatedly or until time out */
  {
    dds_time_t burstStart;
    int burstCount = 0;

    printf ("Writing samples...\n");
    burstStart = pubStart;

    while (!dds_condition_triggered (terminated) && !timedOut)
    {
      /* Write data until burst size has been reached */

      if (burstCount < burstSize)
      {
	status = dds_write (writer, &sample);
        if (dds_err_no (status) == DDS_RETCODE_TIMEOUT)
        {
          timedOut = true;
        }
        else
        {
	  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
	  sample.count++;
	  burstCount++;
        }
      }
      else if (burstInterval)
      {
        /* Sleep until burst interval has passed */

	dds_time_t time = dds_time ();
	dds_time_t deltaTv = time - burstStart;
	dds_time_t deltaTime = deltaTv;
	if (deltaTime < DDS_MSECS (burstInterval))
	{
          dds_write_flush (writer);
	  dds_sleepfor (DDS_MSECS (burstInterval) - deltaTime);
	}
	deltaTv = burstInterval - deltaTime;
	burstStart = dds_time ();
	burstCount = 0;
      }
      else
      {
	burstCount = 0;
      }

      if (timeOut)
      {
	now = dds_time ();
	deltaTv = now - pubStart;
	if ((deltaTv) > DDS_SECS (timeOut))
        {
	  timedOut = true;
	}
      }
    }
    dds_write_flush (writer);

    if (dds_condition_triggered (terminated))
    {
      printf ("Terminated, %llu samples written.\n", (long long) sample.count);
    }
    else
    {
      printf ("Timed out, %llu samples written.\n", (long long) sample.count);
    }
  }

  /* Cleanup */

  status = dds_instance_dispose (writer, &sample);
  if (dds_err_no (status) != DDS_RETCODE_TIMEOUT)
  {
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  }

  dds_free (sample.payload._buffer);

  dds_condition_delete (terminated);
  dds_entity_delete (participant);
  dds_fini ();

  return result;
}
