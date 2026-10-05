/*
 * sntp/sntp.h — Ameba Pro2 SNTP shim for the RA8P1 target.
 *
 * The KVS WebRTC networking_utils.c calls sntp_get_lasttime() which is
 * an Ameba-specific API.  This shim provides the declaration; the
 * implementation lives in src/kvs_app/port/ra8p1_platform_port.c and
 * uses clock_gettime() as the time source.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KVS_SNTP_SHIM_H
#define KVS_SNTP_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the last NTP-synchronised wall-clock time.
 *
 * On this port it always delegates to clock_gettime so that
 * the values are always current (tick delta is forced to 0).
 *
 * @param[out] sec   Seconds since Unix epoch (1 January 1970).
 * @param[out] usec  Microseconds within the current second.
 * @param[out] tick  FreeRTOS tick count at the moment of the snapshot.
 */
void sntp_get_lasttime( long long * sec,
                        long long * usec,
                        unsigned int * tick );

/**
 * @brief Start SNTP time synchronisation.
 *
 * Time is already synced by iotc_time_sync() before the KVS task starts;
 * this stub satisfies the SDK call in app_common.c without doing extra work.
 */
void sntp_init( void );

#ifdef __cplusplus
}
#endif

#endif /* KVS_SNTP_SHIM_H */
