
#include <dds_dcps.h>
#include <ospl.h>
#include <ddsbench.h>
#include <../idl/ddsbench.h>
#include <example_utilities.h>
#include <example_error_sac.h>

DDS_DomainParticipantFactory ddsbench_factory;
DDS_DomainParticipant ddsbench_dp;
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

int init(ddsbench_context *config)
{
    DDS_ReturnCode_t status;

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

    char uri[1024], cwd[1024];
    getcwd(cwd, sizeof(cwd));
    sprintf(uri, "file://%s/ospl.xml", cwd);
    setenv("OSPL_URI", uri, 1);

    printf("\n");
    printf("ddsbench: create participant\n");
    printf("ddsbench: uri = %s\n", getenv("OSPL_URI"));

    /* Create single DomainParticipant */
    ddsbench_factory = DDS_DomainParticipantFactory_get_instance();
    CHECK_HANDLE_MACRO(ddsbench_factory);

    ddsbench_dp = DDS_DomainParticipantFactory_create_participant(
        ddsbench_factory, DDS_DOMAIN_ID_DEFAULT, DDS_PARTICIPANT_QOS_DEFAULT, 0, DDS_STATUS_MASK_NONE);
    CHECK_HANDLE_MACRO(ddsbench_dp);

    return 0;
error:
    return -1;
}

void fini()
{
  DDS_ReturnCode_t status = DDS_DomainParticipant_delete_contained_entities(ddsbench_dp);
    CHECK_STATUS_MACRO(status);
    status = DDS_DomainParticipantFactory_delete_participant(ddsbench_factory, ddsbench_dp);
    CHECK_STATUS_MACRO(status);

#ifdef _WIN32
    SetConsoleCtrlHandler(0, FALSE);
#else
    sigaction(SIGINT,&oldAction, 0);
#endif
}

DDS_TopicQos* ddsbench_getQos(char *qos) {
    DDS_TopicQos *result = DDS_TopicQos__alloc();
    DDS_DomainParticipant_get_default_topic_qos(ddsbench_dp, result);

    char *ptr, ch;
    for (ptr = qos; (ch = *ptr); ptr++) {
        if (ch == 'v') {
            result->durability.kind = DDS_VOLATILE_DURABILITY_QOS;
        } else if (ch == 't') {
            result->durability.kind = DDS_TRANSIENT_DURABILITY_QOS;
        } else if (ch == 'p') {
            result->durability.kind = DDS_PERSISTENT_DURABILITY_QOS;
        } else if (ch == 'b') {
            result->reliability.kind = DDS_BEST_EFFORT_RELIABILITY_QOS;
        } else if (ch == 'r') {
            result->reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
            result->reliability.max_blocking_time.sec = 10;
            result->reliability.max_blocking_time.nanosec = 0;
        }
    }

    result->history.kind = DDS_KEEP_ALL_HISTORY_QOS;
    result->resource_limits.max_samples = 100;

    return result;
}
