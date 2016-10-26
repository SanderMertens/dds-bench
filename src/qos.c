
#include <qos.h>
#include <config.h>

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
