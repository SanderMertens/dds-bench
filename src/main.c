
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
char *ddsbench_qos = "vb";
char *ddsbench_filter = NULL;
unsigned int ddsbench_payload = 4;
unsigned int ddsbench_numsub = -1; /* -1 indicates no value specified */
unsigned int ddsbench_numpub = -1; /* -1 indicates no value specified */
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
      "Usage: osplbench [latency (default)|throughput] [options]\n\n"
      "Options:\n"
      "  --qos qos             Specify QoS (see QoS codes)\n"
      "  --partition name      Specify partition name\n"
      "  --payload bytes       Specify payload of messages\n"
      "  --numsub count        Specify number of subscribers\n"
      "  --numpub count        Specify number of publishers\n"
      "  --filter              Specify filter in OMG-DDS compliant SQL\n"
      "  --help                Display this usage information\n"
      "\n"
      "Use a combination of the following letters to specify a QoS:\n"
      "  v                     volatile\n"
      "  t                     transient\n"
      "  p                     persistent\n"
      "  b                     best effort\n"
      "  r                     reliable\n"
      "  The default QoS is 'vr'.\n"
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
            else if (!strcmp(argv[i], "--numsub")) ddsbench_numsub = atoi(argv[i + 1]), i++;
            else if (!strcmp(argv[i], "--numpub")) ddsbench_numpub = atoi(argv[i + 1]), i++;
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
            if (ddsbench_numpub > ddsbench_numsub) {
                ddsbench_numsub = ddsbench_numpub;
            } else {
                ddsbench_numpub = ddsbench_numsub;
            }
            printf(
              "\n"
              "Note: to measure latency, ddsbench uses one publisher and subscriber\n"
              "that perform a roundtrip. This requires the number of subscribers and\n"
              "publishers to match exactly. Taking the maximum of specified numbers.\n\n");
        }
    }

    if ((ddsbench_numsub <= 0) && (ddsbench_numpub <= 0)) {
        throw("no publishers or subscribers specified.");
    }

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
    printf("  filter: %s\n", ddsbench_filter ? ddsbench_filter : "<not specified>");
    printf("  payload: %d bytes\n", ddsbench_payload);
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
    pthread_t *threads = malloc((ddsbench_numsub + ddsbench_numpub) * sizeof(pthread_t));
    if (!threads)
    {
        throw("out of memory");
    }

    printf("ddsbench: starting %d threads\n", ddsbench_numpub + ddsbench_numsub);

    int i;
    if (!strcmp(ddsbench_mode, "latency"))
    {
        for (i = 0; i < ddsbench_numsub; i++)
        {
            if (pthread_create(&threads[i], NULL, ddsbench_latencySubscriberThread, (void*)(intptr_t)i + 1))
            {
                throw("failed to create thread: %s", strerror(errno));
            }
        }

        for (i = 0; i < ddsbench_numpub; i++)
        {
            if (pthread_create(&threads[i + ddsbench_numsub], NULL, ddsbench_latencyPublisherThread, (void*)(intptr_t)i + 1))
            {
                throw("failed to create thread: %s", strerror(errno));
            }
        }
    } else {
        for (i = 0; i < ddsbench_numsub; i++)
        {
            if (pthread_create(&threads[i], NULL, ddsbench_throughputSubscriberThread, (void*)(intptr_t)i + 1))
            {
                throw("failed to create thread: %s", strerror(errno));
            }
        }

        for (i = 0; i < ddsbench_numpub; i++)
        {
            if (pthread_create(&threads[i + ddsbench_numsub], NULL, ddsbench_throughputPublisherThread, (void*)(intptr_t)i + 1))
            {
                throw("failed to create thread: %s", strerror(errno));
            }
        }
    }

    /* Wait for threads to finish */
    for (i = 0; i < ddsbench_numsub + ddsbench_numpub; i++)
    {
        if (pthread_join(threads[i], NULL))
        {
            throw("failed to join thread: %s", strerror(errno));
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
