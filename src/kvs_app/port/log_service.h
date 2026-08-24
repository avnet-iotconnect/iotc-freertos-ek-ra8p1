/*
 * STM32N6570 KVS WebRTC port — log_service.h
 *
 * The AWS KVS WebRTC SDK examples/logging/logging.h includes this file to
 * allow projects to customise SdkLog.  It is included from WITHIN logging.h
 * before LogInfo/LogError are defined, so we must NOT reference those macros
 * here (doing so creates a circular expansion).
 *
 * Copyright (c) 2025 STMicroelectronics / project contributors.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOG_SERVICE_H
#define LOG_SERVICE_H

#pragma once

#include <stdio.h>

/* Set the KVS library log level before logging.h finishes defining macros. */
#ifndef LIBRARY_LOG_LEVEL
    #define LIBRARY_LOG_LEVEL    LOG_INFO
#endif

/* Route SdkLog to the project console UART (printf is not retargeted on
 * this port). Implemented in ra8p1_platform_port.c. */
#ifdef __cplusplus
extern "C" void kvs_log_printf( const char *fmt, ... );
#else
extern void kvs_log_printf( const char *fmt, ... );
#endif

#ifndef SdkLog
    #define SdkLog( message )  kvs_log_printf message
#endif

/* Override LOG_METADATA_FORMAT/ARGS to avoid pcTaskGetName/xTaskGetCurrentTaskHandle. */
#ifndef LOG_METADATA_FORMAT
    #define LOG_METADATA_FORMAT    "[%s:%d] "
#endif
#ifndef LOG_METADATA_ARGS
    #define LOG_METADATA_ARGS    __FUNCTION__, __LINE__
#endif

#endif /* LOG_SERVICE_H */
