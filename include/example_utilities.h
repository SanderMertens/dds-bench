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

#ifndef __EXAMPLE_UTILITIES_H__
#define __EXAMPLE_UTILITIES_H__

#include <stdlib.h>
#include <time.h>
#ifdef __QNX__
#include <sys/time.h>
#endif
#include "vortex_os.h"
#ifdef _WIN32
#include <Windows.h>
#endif

/**
 * @file
 * This file defines some simple common utility functions for use in the OpenSplice C and C++ examples.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define NS_IN_ONE_US 1000
#define US_IN_ONE_MS 1000
#define US_IN_ONE_SEC 1000000

/**
 * Sleep for the specified period of time.
 * @param milliseconds The period that should be slept for in milliseconds.
 */
void exampleSleepMilliseconds(unsigned long milliseconds);

#define TIME_STATS_SIZE_INCREMENT 50000

/**
 * Struct to keep a running average time as well as recording the minimum
 * and maximum times
 */
typedef struct ExampleTimeStats
{
    unsigned long* values;
    unsigned long valuesSize;
    unsigned long valuesMax;
    double average;
    unsigned long min;
    unsigned long max;
    unsigned long count;
} ExampleTimeStats;

/**
 * Returns an ExampleTimeStats struct with zero initialised variables
 * @return An ExampleTimeStats struct with zero initialised variables
 */
ExampleTimeStats exampleInitTimeStats();

/**
 * Resets an ExampleTimeStats struct variables to zero
 * @param stats An ExampleTimeStats struct to reset
 */
void exampleResetTimeStats(ExampleTimeStats* stats);
/**
 * Deletes the ExampleTimeStats values
 * @param stats An ExampleTimeStats struct delete the values from
 */
void exampleDeleteTimeStats(ExampleTimeStats* stats);

/**
 * Updates an ExampleTimeStats struct with new time data, keeps a running average
 * as well as recording the minimum and maximum times
 * @param stats ExampleTimeStats struct to update
 * @param microseconds A time in microseconds to add to the stats
 * @return The updated ExampleTimeStats struct
 */
ExampleTimeStats* exampleAddMicrosecondsToTimeStats(ExampleTimeStats* stats, unsigned long microseconds);

/**
 * Compares two unsigned longs
 *
 * @param a an unsigned long
 * @param b an unsigned long
 * @param int -1 if a < b, 1 if a > b, 0 if equal
 */
int exampleCompareul(const void* a, const void* b);

/**
 * Calculates the median time from an ExampleTimeStats
 *
 * @param stats the ExampleTimeStats
 * @return the median time
 */
double exampleGetMedianFromTimeStats(ExampleTimeStats* stats);

/**
 * Gets the current time
 * @return A timeval struct representing the current time
 */
struct timeval exampleGetTime();

/**
 * Adds one timeval to another
 * @param t1 to add to
 * @param t2 to add
 * @return A timeval struct representing the sum of two times
 */
struct timeval exampleAddTimevalToTimeval(const struct timeval* t1, const struct timeval* t2);

/**
 * Subtracts one timeval from another
 * @param t1 to subtract from
 * @param t2 to subtract
 * @return A timeval struct representing the difference between two times
 */
struct timeval exampleSubtractTimevalFromTimeval(const struct timeval* t1, const struct timeval* t2);

/**
 * Converts a timeval to microseconds
 * @param t The time to convert
 * @return The result of converting a timeval to microseconds
 */
unsigned long exampleTimevalToMicroseconds(const struct timeval* t);

/**
 * Converts microseconds to a timeval
 * @param microseconds the number of microseconds to convert
 * @return The result of converting a timeval to microseconds
 */
struct timeval exampleMicrosecondsToTimeval(const unsigned long microseconds);

#if defined (__cplusplus)
}
#endif

/*
 * The below define is normally off. It enables richer doxygen
 * documentation auto-linking.
 */
#ifdef DOXYGEN_FOR_ISOCPP
#include "workaround.h"
#endif

#endif
