
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


 #ifndef QOS_H
 #define QOS_H

 #include <dds_dcps.h>

 #ifdef __cplusplus
 extern "c" {
 #endif

 DDS_TopicQos* ddsbench_getQos(char *qos);

 #ifdef __cplusplus
 }
 #endif

 #endif
