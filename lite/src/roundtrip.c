#include <ddsbench.h>
#include <../idl/ddsbench.h>
#include <lite.h>

#define TIME_STATS_SIZE_INCREMENT 50000
#define MAX_SAMPLES 100
#define US_IN_ONE_SEC 1000000LL

typedef struct ExampleTimeStats
{
  unsigned long *values;
  unsigned long valuesSize;
  unsigned long valuesMax;
  double average;
  unsigned long min;
  unsigned long max;
  unsigned long count;
} ExampleTimeStats;

static void exampleInitTimeStats (ExampleTimeStats *stats)
{
  stats->values = (unsigned long *)malloc (TIME_STATS_SIZE_INCREMENT * sizeof (unsigned long));
  stats->valuesSize = 0;
  stats->valuesMax = TIME_STATS_SIZE_INCREMENT;
  stats->average = 0;
  stats->min = 0;
  stats->max = 0;
  stats->count = 0;
}

static void exampleResetTimeStats (ExampleTimeStats *stats)
{
  memset (stats->values, 0, stats->valuesMax * sizeof (unsigned long));
  stats->valuesSize = 0;
  stats->average = 0;
  stats->min = 0;
  stats->max = 0;
  stats->count = 0;
}

static void exampleDeleteTimeStats (ExampleTimeStats *stats)
{
  free (stats->values);
}

static ExampleTimeStats *exampleAddTimingToTimeStats (ExampleTimeStats *stats, unsigned long timing)
{
  if (stats->valuesSize > stats->valuesMax)
  {
    unsigned long * temp = (unsigned long*)realloc (stats->values, (stats->valuesMax + TIME_STATS_SIZE_INCREMENT) * sizeof (unsigned long));
    stats->values = temp;
    stats->valuesMax += TIME_STATS_SIZE_INCREMENT;
  }
  if (stats->valuesSize < stats->valuesMax)
  {
    stats->values[stats->valuesSize++] = timing;
  }
  stats->average = (stats->count * stats->average + timing) / (stats->count + 1);
  stats->min = (stats->count == 0 || timing < stats->min) ? timing : stats->min;
  stats->max = (stats->count == 0 || timing > stats->max) ? timing : stats->max;
  stats->count++;

  return stats;
}

static int exampleCompareul (const void* a, const void* b)
{
  unsigned long ul_a = *((unsigned long*)a);
  unsigned long ul_b = *((unsigned long*)b);

  if (ul_a < ul_b) return -1;
  if (ul_a > ul_b) return 1;
  return 0;
}

static double exampleGetMedianFromTimeStats (ExampleTimeStats *stats)
{
  double median = 0.0;

  qsort (stats->values, stats->valuesSize, sizeof (unsigned long), exampleCompareul);

  if (stats->valuesSize % 2 == 0)
  {
    median = (double)(stats->values[stats->valuesSize / 2 - 1] + stats->values[stats->valuesSize / 2]) / 2;
  }
  else
  {
    median = (double)stats->values[stats->valuesSize / 2];
  }

  return median;
}

int lsub (ddsbench_threadArg *arg)
{
  dds_entity_t participant;
  dds_entity_t writer;
  dds_entity_t reader;
  dds_entity_t topic;
  dds_entity_t publisher;
  dds_entity_t subscriber;
  dds_waitset_t waitSet;

  const char *pubPartitions[] = { "ping" };
  const char *subPartitions[] = { "pong" };
  dds_qos_t *pubQos;
  dds_qos_t *dwQos;
  dds_qos_t *drQos;
  dds_qos_t *subQos;

  ExampleTimeStats roundTrip;
  ExampleTimeStats writeAccess;
  ExampleTimeStats readAccess;
  ExampleTimeStats roundTripOverall;
  ExampleTimeStats writeAccessOverall;
  ExampleTimeStats readAccessOverall;

  unsigned long payloadSize = 0;
  unsigned long long numSamples = 0;
  dds_time_t timeOut = 0;
  dds_time_t startTime;
  dds_time_t time;
  dds_time_t preWriteTime;
  dds_time_t postWriteTime;
  dds_time_t preTakeTime;
  dds_time_t postTakeTime;
  dds_time_t difference = 0;
  dds_time_t elapsed = 0;

  RoundTripModule_DataType pub_data;
  RoundTripModule_DataType sub_data[MAX_SAMPLES];
  void *samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];

  dds_attach_t wsresults[1];
  size_t wsresultsize = 1U;
  dds_time_t waitTimeout = DDS_SECS (1);
  unsigned long i;
  int status;
  bool warmUp = true;
  dds_condition_t readCond;

  exampleInitTimeStats (&roundTrip);
  exampleInitTimeStats (&writeAccess);
  exampleInitTimeStats (&readAccess);
  exampleInitTimeStats (&roundTripOverall);
  exampleInitTimeStats (&writeAccessOverall);
  exampleInitTimeStats (&readAccessOverall);

  memset (&sub_data, 0, sizeof (sub_data));
  memset (&pub_data, 0, sizeof (pub_data));

  for (i = 0; i < MAX_SAMPLES; i++)
  {
    samples[i] = &sub_data[i];
  }

  status = dds_init(0, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  status = dds_thread_init ("lsub");
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  status = dds_participant_create (&participant, DDS_DOMAIN_DEFAULT, NULL, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  /* A DDS_Topic is created for our sample type on the domain participant. */
  topic = dds_topic_find(participant, arg->topicName);
  if (!topic) {
      status = dds_topic_create
        (participant, &topic, &RoundTripModule_DataType_desc, arg->topicName, NULL, NULL);
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  }

  /* A DDS_Publisher is created on the domain participant. */
  pubQos = dds_qos_create ();
  dds_qset_partition (pubQos, 1, pubPartitions);

  status = dds_publisher_create (participant, &publisher, pubQos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (pubQos);

  /* A DDS_DataWriter is created on the Publisher & Topic with a modified Qos. */
  dwQos = dds_qos_create ();
  dds_qset_reliability (dwQos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_writer_data_lifecycle (dwQos, false);
  status = dds_writer_create (publisher, &writer, topic, dwQos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (dwQos);

  /* A DDS_Subscriber is created on the domain participant. */
  subQos = dds_qos_create ();
  dds_qset_partition (subQos, 1, subPartitions);

  status = dds_subscriber_create (participant, &subscriber, subQos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (subQos);
  /* A DDS_DataReader is created on the Subscriber & Topic with a modified QoS. */
  drQos = dds_qos_create ();
  dds_qset_reliability (drQos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  status = dds_reader_create (subscriber, &reader, topic, drQos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (drQos);

  terminated = dds_guardcondition_create ();
  waitSet = dds_waitset_create ();
  readCond = dds_readcondition_create (reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);

  status = dds_waitset_attach (waitSet, readCond, reader);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  status = dds_waitset_attach (waitSet, terminated, terminated);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  setvbuf(stdout, NULL, _IONBF, 0);

  payloadSize = arg->ctx->payload;
  numSamples = 0;
  timeOut = 0;

  pub_data.payload._length = payloadSize;
  pub_data.payload._buffer = payloadSize ? dds_alloc (payloadSize) : NULL;
  pub_data.payload._release = true;
  pub_data.payload._maximum = 0;
  for (i = 0; i < payloadSize; i++)
  {
    pub_data.payload._buffer[i] = 'a';
  }

  startTime = dds_time ();
  printf ("# Waiting for startup jitter to stabilise\n");
  while (!dds_condition_triggered (terminated) && difference < DDS_SECS(5))
  {
    status = dds_write (writer, &pub_data);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    if (status > 0) /* data */
    {
      status = dds_take (reader, samples, MAX_SAMPLES, info, 0);
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    }

    time = dds_time ();
    difference = time - startTime;
  }
  if (!dds_condition_triggered (terminated))
  {
    warmUp = false;
    printf("# Warm up complete.\n\n");

    printf("# Round trip measurements (in us)\n");
    printf("#             Round trip time [us]         Write-access time [us]       Read-access time [us]\n");
    printf("# Seconds     Count   median      min      Count   median      min      Count   median      min\n");

  }

  startTime = dds_time ();
  for (i = 0; !dds_condition_triggered (terminated) && (!numSamples || i < numSamples); i++)
  {
    /* Write a sample that pong can send back */
    preWriteTime = dds_time ();
    status = dds_write (writer, &pub_data);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    postWriteTime = dds_time ();

    /* Wait for response from pong */
    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    if (status != 0)
    {
      /* Take sample and check that it is valid */
      preTakeTime = dds_time ();
      status = dds_take (reader, samples, MAX_SAMPLES, info, 0);
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
      postTakeTime = dds_time ();

      if (!dds_condition_triggered (terminated))
      {
        if (status != 1)
        {
          fprintf (stdout, "%s%d%s", "ERROR: Ping received ", status,
                  " samples but was expecting 1. Are multiple pong applications running?\n");

          return (0);
        }
        else if (!info[0].valid_data)
        {
          printf ("ERROR: Ping received an invalid sample. Has pong terminated already?\n");
          return (0);
        }
      }

      /* Update stats */
      difference = (postWriteTime - preWriteTime)/DDS_NSECS_IN_USEC;
      writeAccess = *exampleAddTimingToTimeStats (&writeAccess, difference);
      writeAccessOverall = *exampleAddTimingToTimeStats (&writeAccessOverall, difference);

      difference = (postTakeTime - preTakeTime)/DDS_NSECS_IN_USEC;
      readAccess = *exampleAddTimingToTimeStats (&readAccess, difference);
      readAccessOverall = *exampleAddTimingToTimeStats (&readAccessOverall, difference);

      difference = (postTakeTime - preWriteTime)/DDS_NSECS_IN_USEC;
      roundTrip = *exampleAddTimingToTimeStats (&roundTrip, difference);
      roundTripOverall = *exampleAddTimingToTimeStats (&roundTripOverall, difference);

      /* Print stats each second */
      difference = (postTakeTime - startTime)/DDS_NSECS_IN_USEC;
      if (difference > US_IN_ONE_SEC || (i && i == numSamples))
      {
        printf
        (
          "%9" PRIi64 " %9lu %8.0f %8lu %10lu %8.0f %8lu %10lu %8.0f %8lu\n",
          elapsed + 1,
          roundTrip.count,
          exampleGetMedianFromTimeStats (&roundTrip),
          roundTrip.min,
          writeAccess.count,
          exampleGetMedianFromTimeStats (&writeAccess),
          writeAccess.min,
          readAccess.count,
          exampleGetMedianFromTimeStats (&readAccess),
          readAccess.min
        );

        exampleResetTimeStats (&roundTrip);
        exampleResetTimeStats (&writeAccess);
        exampleResetTimeStats (&readAccess);
        startTime = dds_time ();
        elapsed++;
      }
    }
    else
    {
      elapsed += waitTimeout / DDS_NSECS_IN_SEC;
    }
    if (timeOut && elapsed == timeOut)
    {
      dds_guard_trigger (terminated);
    }
  }

  if (!warmUp)
  {
    printf
    (
      "\n%9s %9lu %8.0f %8lu %10lu %8.0f %8lu %10lu %8.0f %8lu\n",
      "# Overall",
      roundTripOverall.count,
      exampleGetMedianFromTimeStats (&roundTripOverall),
      roundTripOverall.min,
      writeAccessOverall.count,
      exampleGetMedianFromTimeStats (&writeAccessOverall),
      writeAccessOverall.min,
      readAccessOverall.count,
      exampleGetMedianFromTimeStats (&readAccessOverall),
      readAccessOverall.min
    );
  }

  /* Disable callbacks */

  dds_status_set_enabled (reader, 0);

  /* Clean up */

  exampleDeleteTimeStats (&roundTrip);
  exampleDeleteTimeStats (&writeAccess);
  exampleDeleteTimeStats (&readAccess);
  exampleDeleteTimeStats (&roundTripOverall);
  exampleDeleteTimeStats (&writeAccessOverall);
  exampleDeleteTimeStats (&readAccessOverall);

  status = dds_waitset_detach (waitSet, readCond);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  status = dds_waitset_detach (waitSet, terminated);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_condition_delete (readCond);
  dds_condition_delete (terminated);
  status = dds_waitset_delete (waitSet);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  for (i = 0; i < MAX_SAMPLES; i++)
  {
    RoundTripModule_DataType_free (&sub_data[i], DDS_FREE_CONTENTS);
  }
  RoundTripModule_DataType_free (&pub_data, DDS_FREE_CONTENTS);

  return 0;
}

int lpub (ddsbench_threadArg *arg)
{
  dds_duration_t waitTimeout = DDS_INFINITY;
  unsigned int i;
  int status, samplecount;
  dds_attach_t wsresults[1];
  size_t wsresultsize = 1U;
  dds_entity_t participant;
  dds_entity_t reader;
  dds_entity_t writer;
  dds_entity_t topic;
  dds_entity_t publisher;
  dds_entity_t subscriber;
  dds_waitset_t waitSet;

  RoundTripModule_DataType data[MAX_SAMPLES];
  void * samples[MAX_SAMPLES];
  dds_sample_info_t info[MAX_SAMPLES];
  const char *pubPartitions[] = { "pong" };
  const char *subPartitions[] = { "ping" };
  dds_qos_t *qos;
  dds_condition_t readCond;

  /* Initialize sample data */

  memset (data, 0, sizeof (data));
  for (i = 0; i < MAX_SAMPLES; i++)
  {
    samples[i] = &data[i];
  }

  status = dds_init(0, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  status = dds_thread_init ("lpub");
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  status = dds_participant_create (&participant, DDS_DOMAIN_DEFAULT, NULL, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  /* A DDS Topic is created for our sample type on the domain participant. */

  topic = dds_topic_find(participant, arg->topicName);
  if (!topic) {
      status = dds_topic_create
        (participant, &topic, &RoundTripModule_DataType_desc, arg->topicName, NULL, NULL);
      DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  }

  /* A DDS Publisher is created on the domain participant. */

  qos = dds_qos_create ();
  dds_qset_partition (qos, 1, pubPartitions);
  status = dds_publisher_create (participant, &publisher, qos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  /* A DDS DataWriter is created on the Publisher & Topic with a modififed Qos. */

  qos = dds_qos_create ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  dds_qset_writer_data_lifecycle (qos, false);
  status = dds_writer_create (publisher, &writer, topic, qos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  /* A DDS Subscriber is created on the domain participant. */

  qos = dds_qos_create ();
  dds_qset_partition (qos, 1, subPartitions);
  status = dds_subscriber_create (participant, &subscriber, qos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  /* A DDS DataReader is created on the Subscriber & Topic with a modified QoS. */

  qos = dds_qos_create ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
  status = dds_reader_create (subscriber, &reader, topic, qos, NULL);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_qos_delete (qos);

  terminated = dds_guardcondition_create ();
  waitSet = dds_waitset_create ();
  readCond = dds_readcondition_create (reader, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE);
  status = dds_waitset_attach (waitSet, readCond, reader);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  status = dds_waitset_attach (waitSet, terminated, terminated);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  printf ("Waiting for samples from ping to send back...\n");
  fflush (stdout);

  while (!dds_condition_triggered (terminated))
  {
    /* Wait for a sample from ping */

    status = dds_waitset_wait (waitSet, wsresults, wsresultsize, waitTimeout);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    /* Take samples */
    samplecount = dds_take (reader, samples, MAX_SAMPLES, info, 0);
    DDS_ERR_CHECK (samplecount, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
    for (i = 0; !dds_condition_triggered (terminated) && i < samplecount; i++)
    {
      /* If writer has been disposed terminate pong */

      if (info[i].instance_state == DDS_IST_NOT_ALIVE_DISPOSED)
      {
        printf ("Received termination request. Terminating.\n");
        dds_guard_trigger (terminated);
        break;
      }
      else if (info[i].valid_data)
      {
        /* If sample is valid, send it back to ping */

        RoundTripModule_DataType * valid_sample = &data[i];
        status = dds_write (writer, valid_sample);
        DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
      }
    }
  }

  /* Clean up */
  status = dds_waitset_detach (waitSet, readCond);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  status = dds_waitset_detach (waitSet, terminated);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);
  dds_condition_delete (readCond);
  dds_condition_delete (terminated);
  status = dds_waitset_delete (waitSet);
  DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

  for (i = 0; i < MAX_SAMPLES; i++)
  {
    RoundTripModule_DataType_free (&data[i], DDS_FREE_CONTENTS);
  }

  return 0;
}
