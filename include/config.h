
#ifndef CONFIG_H
#define CONFIG_H

#include <dds_dcps.h>

#ifdef __cplusplus
extern "c" {
#endif

extern char *ddsbench_mode;
extern char *ddsbench_qos;
extern char *ddsbench_filter;
extern unsigned int ddsbench_payload;
extern unsigned int ddsbench_numsub;
extern unsigned int ddsbench_numpub;
extern unsigned int ddsbench_subid;
extern unsigned int ddsbench_pubid;
extern char ddsbench_topicname[256];
extern char ddsbench_filtername[256];

extern DDS_GuardCondition terminated;
extern DDS_DomainParticipant ddsbench_dp;

typedef struct ddsbench_threadArg {
    int id;
    char topicName[256];
} ddsbench_threadArg;

#ifdef __cplusplus
}
#endif

#endif
