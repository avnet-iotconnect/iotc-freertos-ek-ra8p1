/*
 * STM32N6570 KVS WebRTC port — FreeRTOS_POSIX/time.h stub
 *
 * The KVS WebRTC SDK (core_http_helper.c) includes "FreeRTOS_POSIX/time.h"
 * which on the Ameba platform provides FreeRTOS-POSIX time types.  On STM32
 * with newlib the standard <time.h> already supplies struct timespec, time_t,
 * and clock_gettime, so this header simply re-exports it.
 *
 * Copyright (c) 2025 project contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FREERTOS_POSIX_TIME_H
#define FREERTOS_POSIX_TIME_H

/* Force newlib to expose POSIX timers (clock_gettime / CLOCK_REALTIME). */
#ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200809L
#endif

#include <time.h>

/* picolibc's <time.h> already declares clock_gettime(clockid_t, struct
 * timespec *); the implementation lives in platform_stubs.c on top of
 * _gettimeofday().  No extra declaration needed here. */
#ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME 0
#endif

#endif /* FREERTOS_POSIX_TIME_H */
