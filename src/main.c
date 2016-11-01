
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <config.h>
#include <throughput.h>
#include <example_utilities.h>
#include <example_error_sac.h>
#include <roundtrip.h>

/** ddsbench configuration options */
char *ddsbench_mode = "latency";
char *ddsbench_qos = "vr";
char *ddsbench_filter = NULL;
unsigned int ddsbench_payload = 4;
unsigned int ddsbench_numsub = -1; /* -1 indicates no value specified */
unsigned int ddsbench_numpub = -1; /* -1 indicates no value specified */
unsigned int ddsbench_subid = 0; /* -1 indicates no value specified */
unsigned int ddsbench_pubid = 0; /* -1 indicates no value specified */
unsigned int ddsbench_numtopic = 1;
unsigned int ddsbench_burstsize = 1;
unsigned int ddsbench_burstinterval = 0;
unsigned int ddsbench_pollingdelay = 0;
char ddsbench_topicname[256];
char ddsbench_filtername[256];

DDS_DomainParticipant ddsbench_dp;

/** catch Ctrl-C */
DDS_GuardCondition terminated;

#ifndef _WIN32
struct sigaction oldAction;
#endif
/*
 * Function to handle Ctrl-C presses.
 * @param fdwCtrlType Ctrl signal type
 */
#ifdef _WIN32
static DDS_boolean CtrlHandler(DWORD fdwCtrlType)
{
    DDS_ReturnCode_t status = DDS_GuardCondition_set_trigger_value(terminated, TRUE);
    CHECK_STATUS_MACRO(status);
    return TRUE; //Don't let other handlers handle this key
}
#else
static void CtrlHandler(int fdwCtrlType)
{
    DDS_ReturnCode_t status = DDS_GuardCondition_set_trigger_value(terminated, TRUE);
    CHECK_STATUS_MACRO(status);
}
#endif

/** Error reporting */
#define throw(...) { printf("error: " __VA_ARGS__); goto error; }

static void printUsage(void)
{
    printf(
      "Usage: ddsbench [latency (default)|throughput] [options]\n\n"
      "Options:\n"
      "  --qos v|t|p|b|r       Specify QoS (see QoS codes)\n"
      "  --payload bytes       Specify payload of messages\n"
      "  --numsub count        Specify number of subscribers\n"
      "  --numpub count        Specify number of publishers\n"
      "  --numtopic count      Specify the number of topics to write to\n"
      "  --pubid offset        Specify an offset for the publisher id\n"
      "  --subid offset        Specify an offset for the subscriber id\n"
      "  --filter              Specify filter in OMG-DDS compliant SQL\n"
      "  --help                Display this usage information\n"
      "\n"
      "Throughput only options:\n"
      "  --burstsize           Number of samples to send in a burst (default = 1)\n"
      "  --burstinterval       Number of ms between bursts (default = 0)\n"
      "  --pollingdelay        Delay between polling in ms, 0 is event based (default = 0)\n"
      "\n"
      "Use a combination of the following letters to specify a QoS:\n"
      "  v - volatile\n"
      "  t - transient\n"
      "  p - persistent\n"
      "  b - best effort\n"
      "  r - reliable\n"
      "  The default QoS is 'vr'.\n"
      "\n"
      "When you want to run ddsbench with multiple processes, you can use subid\n"
      "and pubid to specify unique ids for each application. For example, to setup\n"
      "a throuhgput test with 3 publishers in different processes you do:\n"
      " ddsbench throuhgput --numsub 1 &\n"
      " ddsbench throuhgput --numpub 1 --pubid 1 &\n"
      " ddsbench throuhgput --numpub 1 --pubid 2 &\n"
      " ddsbench throuhgput --numpub 1 --pubid 3 &\n"
      "\n"
      "When specifying a filter, you can use the 'filter' field, which is a member\n"
      "in both the types used for latency and throughput benchmarking. The filter\n"
      "field will cycle through the numbers one through 10. To specify a filter that\n"
      "blocks 50%% of the traffic, do:\n"
      " ddsbench throughput --filter \"filter < 5\"\n"
      "\n"
      "When specifying a filter in latency measurements, be sure *not* to block any\n"
      "data, as this will disrupt the measurement. A safe filter that can be used to\n"
      "measure filter overhead is:\n"
      " ddsbench latency --filter \"filter < 10\"\n"
      "\n"
      "If specifying more than one topic, the number of configured publishers and\n"
      "subscribers will be multiplied by the number of topics. For example:\n"
      " ddsbench throughput --numsub 1 --numpub 2 --numtopic 3\n"
      "This command creates 3 subscribers and 6 publishers. Subscribers and\n"
      "publishers from different topics do not communicate with each other.\n"
      "\n"
    );
}

static int parseArguments(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i ++)
    {
        if (argv[i][0] == '-')
        {
            if ((i == (argc - 1)) || !argv[i + 1][0]) throw("missing parameter for %s\n", argv[i]);
            if (!strcmp(argv[i], "--qos")) ddsbench_qos = argv[i + 1], i++;
            else if (!strcmp(argv[i], "--filter")) ddsbench_filter = argv[i + 1], i++;
            else if (!strcmp(argv[i], "--payload")) ddsbench_payload = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--burstsize")) ddsbench_burstsize = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--burstinterval")) ddsbench_burstinterval = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--pollingdelay")) ddsbench_pollingdelay = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--numsub")) ddsbench_numsub = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--numpub")) ddsbench_numpub = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--numtopic")) ddsbench_numtopic = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--subid")) ddsbench_subid = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--pubid")) ddsbench_pubid = atoi(argv[i + 1]), i++;
            else throw("invalid option %s", argv[1]);
        } else
        {
            if (!strcmp(argv[i], "latency") || !strcmp(argv[i], "throughput"))
            {
                ddsbench_mode = argv[i];
            } else
            {
                throw("invalid option %s\n", argv[i]);
            }
        }
    }

    /* If a user does not specify the number of publishers or subscribers, take
     * the default of one publisher and one subscriber. If the user explicitly
     * provides a number of subscribers and/or publishers, the default is zero. */
    if ((ddsbench_numpub == -1) && (ddsbench_numsub == -1)) {
        ddsbench_numpub = 1;
        ddsbench_numsub = 1;
    } else if ((ddsbench_numsub == -1) && (ddsbench_numpub != -1)) {
        ddsbench_numsub = 0;
    } else if ((ddsbench_numpub == -1) && (ddsbench_numsub != -1)) {
        ddsbench_numpub = 0;
    }

    /* If mode is set to latency, number of publishers and subscribers must
     * match exactly. Take whichever one the user provided */
    if (ddsbench_numpub != ddsbench_numsub) {
        if (!strcmp(ddsbench_mode, "latency")) {
            printf(
              "\n"
              "Note: to measure latency, ddsbench uses one publisher and subscriber\n"
              "that perform a roundtrip. This requires the number of subscribers and\n"
              "publishers to match exactly.\n\n");
        }
    }

    if (ddsbench_filter) {
        printf(
          "\n"
          "Note: when specifying a filter, throughput will appear to decrease\n"
          "because the subscriber is receiving less data.\n"
          "When measuring the overhead of a filter on latency you have to specify\n"
          "a filter thatdoes *not* block any data, as this will disrupt the\n"
          "measurement. An example of such a filter is: 'filter > 10'.\n\n");
    }

    if ((ddsbench_numsub <= 0) && (ddsbench_numpub <= 0)) {
        throw("no publishers or subscribers specified.");
    }

    sprintf(ddsbench_topicname, "%s_%s", ddsbench_mode, ddsbench_qos);
    sprintf(ddsbench_filtername, "%s_%s_filter", ddsbench_mode, ddsbench_qos);

    return 0;
error:
    return -1;
}

int main(int argc, char *argv[])
{
    DDS_ReturnCode_t status;

    if ((argc > 1) && !strcmp(argv[1], "--help"))
    {
        printUsage();
        return 0;
    }

    if (parseArguments(argc, argv))
    {
        printUsage();
        goto error;
    }

    printf("ddsbench v1.0\n");
    printf("  config: %s\n", getenv("OSPL_URI"));
    printf("  mode: %s\n", ddsbench_mode);
    printf("  qos: %s\n", ddsbench_qos);
    printf("  topic: %s\n", ddsbench_topicname);
    if (ddsbench_filter) {
        printf("  filter: %s\n", ddsbench_filter);
    }
    printf("  payload: %d bytes\n", ddsbench_payload);
    if (!strcmp(ddsbench_mode, "throughput")) {
        printf("  burstsize: %d\n", ddsbench_burstsize);
        printf("  burstinterval: %d\n", ddsbench_burstinterval);
    }
    if (ddsbench_subid) {
        printf("  subscriber id: %d\n", ddsbench_subid);
    }
    if (ddsbench_pubid) {
        printf("  publisher id: %d\n", ddsbench_pubid);
    }
    printf("  # topics: %d\n", ddsbench_numtopic);
    printf("  # subscribers: %d\n", ddsbench_numsub);
    printf("  # publishers: %d\n", ddsbench_numpub);

    /* Register handler for Ctrl-C */
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#else
    struct sigaction sat;
    sat.sa_handler = CtrlHandler;
    sigemptyset(&sat.sa_mask);
    sat.sa_flags = 0;
    sigaction(SIGINT,&sat,&oldAction);
#endif
    terminated = DDS_GuardCondition__alloc();

    printf("\n");
    printf("ddsbench: create participant\n");

    /* Create single DomainParticipant */
    DDS_DomainParticipantFactory factory = DDS_DomainParticipantFactory_get_instance();
    CHECK_HANDLE_MACRO(factory);

    ddsbench_dp = DDS_DomainParticipantFactory_create_participant(
        factory, DDS_DOMAIN_ID_DEFAULT, DDS_PARTICIPANT_QOS_DEFAULT, 0, DDS_STATUS_MASK_NONE);
    CHECK_HANDLE_MACRO(ddsbench_dp);

    /* Start publisher and subscriber threads */
    pthread_t *threads = malloc((ddsbench_numsub + ddsbench_numpub) * sizeof(pthread_t) * ddsbench_numtopic);
    if (!threads)
    {
        throw("out of memory");
    }

    printf("ddsbench: starting %d threads\n", (ddsbench_numpub + ddsbench_numsub) * ddsbench_numtopic);

    int topic = 0, thread = 0, sub = ddsbench_subid, pub = ddsbench_pubid;
    int mode = !strcmp(ddsbench_mode, "latency");

    while (topic < ddsbench_numtopic) {
        int total = sub + ddsbench_numsub;
        for (; sub < total; sub++)
        {
            ddsbench_threadArg *arg = malloc(sizeof(ddsbench_threadArg));
            arg->id = sub;
            sprintf(arg->topicName, "%s_%d", ddsbench_topicname, topic);
            if (pthread_create
              (&threads[thread], NULL, mode ? ddsbench_latencySubscriberThread : ddsbench_throughputSubscriberThread, arg))
            {
                throw("failed to create thread: %s", strerror(errno));
            }
            thread ++;
        }

        total = pub + ddsbench_numpub;
        for (; pub < total; pub++)
        {
            ddsbench_threadArg *arg = malloc(sizeof(ddsbench_threadArg));
            arg->id = pub;
            sprintf(arg->topicName, "%s_%d", ddsbench_topicname, topic);
            if (pthread_create
              (&threads[thread], NULL, mode ? ddsbench_latencyPublisherThread : ddsbench_throughputPublisherThread, arg))
            {
                throw("failed to create thread: %s", strerror(errno));
            }
            thread ++;
        }
        topic ++;
    }

    /* Wait for threads to finish */
    int i;
    for (i = 0; i < thread; i++)
    {
        if (pthread_join(threads[i], NULL))
        {
            throw("failed to join thread %d: %s", i, strerror(errno));
        }
    }

    status = DDS_DomainParticipant_delete_contained_entities(ddsbench_dp);
    CHECK_STATUS_MACRO(status);
    status = DDS_DomainParticipantFactory_delete_participant(factory, ddsbench_dp);
    CHECK_STATUS_MACRO(status);

    #ifdef _WIN32
        SetConsoleCtrlHandler(0, FALSE);
    #else
        sigaction(SIGINT,&oldAction, 0);
    #endif

    return 0;
error:
    return -1;
}
