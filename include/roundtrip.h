
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


 #ifndef ROUNDTRIP_H
 #define ROUNDTRIP_H

 #ifdef __cplusplus
 extern "c" {
 #endif

 void* ddsbench_latencySubscriberThread(void *arg);
 void* ddsbench_latencyPublisherThread(void *arg);
 
 #ifdef __cplusplus
 }
 #endif

 #endif
