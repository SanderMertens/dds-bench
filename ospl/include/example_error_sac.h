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

#ifndef __EXAMPLE_ERROR_SAC_H__
#define __EXAMPLE_ERROR_SAC_H__

/**
 * @file
 * This file defines some simple error handling functions for use in the OpenSplice Standalone C examples.
 */

#include "dds_dcps.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INT_TO_STRING_MACRO(n) I_TO_STR_MACRO(n)
#define I_TO_STR_MACRO(n) #n
#define CHECK_STATUS_MACRO(returnCode) checkStatus (returnCode, " : Invalid return code at " __FILE__ ":" INT_TO_STRING_MACRO(__LINE__) )
#define CHECK_HANDLE_MACRO(handle) checkHandle (handle, "Failed to create entity : Invalid handle at " __FILE__ ":" INT_TO_STRING_MACRO(__LINE__) )
#define CHECK_ALLOC_MACRO(handle) checkHandle (handle, "Failed to allocate memory at " __FILE__ ":" INT_TO_STRING_MACRO(__LINE__) )

extern const char* ReturnCodeName[13];

/**
 * Function to convert DDS return codes into an error message on the standard output.
 * @param returnCode DDS return code
 * @param where A string detailing where the error occurred
 */
void checkStatus(DDS_ReturnCode_t returnCode, const char *where);

/**
 * Function to check for a null handle and display an error message on the standard output.
 * @param Handle to check for null
 * @param A string detailing where the error occured
 */
void checkHandle(void *handle, const char *where);

#ifdef __cplusplus
}
#endif

#endif
