
#include <ddsbench.h>
#include <../idl/ddsbench.h>

dds_condition_t terminated;
dds_entity_t ddsbench_dp;

#ifdef _WIN32
static bool CtrlHandler (DWORD fdwCtrlType)
{
  dds_guard_trigger (terminated);
  return true; //Don't let other handlers handle this key
}
#else
static void CtrlHandler (int fdwCtrlType)
{
  dds_guard_trigger (terminated);
}
#endif

static struct sigaction oldAction;

int init(ddsbench_context *ctx) {
    int status;

#ifdef _WIN32
    SetConsoleCtrlHandler ((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#else
    struct sigaction sat;
    sat.sa_handler = CtrlHandler;
    sigemptyset (&sat.sa_mask);
    sat.sa_flags = 0;
    sigaction (SIGINT, &sat, &oldAction);
#endif

    status = dds_init (0, NULL);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    status = dds_participant_create (&ddsbench_dp, DDS_DOMAIN_DEFAULT, NULL, NULL);
    DDS_ERR_CHECK (status, DDS_CHECK_REPORT | DDS_CHECK_EXIT);

    return 0;
}

void fini(void) {
#ifdef _WIN32
    SetConsoleCtrlHandler (0, FALSE);
#else
    sigaction (SIGINT, &oldAction, 0);
#endif

    //dds_entity_delete (ddsbench_dp);
    dds_fini ();
}
