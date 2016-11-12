
#ifndef OSPL_H
#define OSPL_H

#include <dds_dcps.h>

#ifdef __cplusplus
extern "c" {
#endif

extern DDS_DomainParticipantFactory ddsbench_factory;
extern DDS_DomainParticipant ddsbench_dp;
extern DDS_GuardCondition terminated;

DDS_TopicQos* ddsbench_getQos(char *qos);

#ifdef __cplusplus
}
#endif

#endif
