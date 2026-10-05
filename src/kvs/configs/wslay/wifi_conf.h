/*
 * wifi_conf.h — Ameba Pro2 WiFi configuration stub for the RA8P1 target.
 *
 * app_common.c calls wifi_get_join_status() to wait for network readiness.
 * On the RA8P1 the network is brought up by the net thread before the KVS task
 * starts, so we stub the join-status check to always return success.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WIFI_CONF_SHIM_H
#define WIFI_CONF_SHIM_H

#include <stdint.h>

/* Ameba join-status values */
typedef enum {
    RTW_JOINSTATUS_UNKNOWN   = 0,
    RTW_JOINSTATUS_STARTING  = 1,
    RTW_JOINSTATUS_SCANNING  = 2,
    RTW_JOINSTATUS_CONNECTING = 3,
    RTW_JOINSTATUS_SUCCESS   = 4,
    RTW_JOINSTATUS_FAIL      = 5,
} rtw_join_status_t;

/* The network is already up when the KVS task runs. */
static inline rtw_join_status_t wifi_get_join_status( void )
{
    return RTW_JOINSTATUS_SUCCESS;
}

/* Ameba type alias used in the WiFi init loop */
#ifndef u32
typedef uint32_t u32;
#endif

/* Invalid IP sentinel used by app_common.c */
#ifndef IP_ADDR_INVALID
#define IP_ADDR_INVALID  0x00000000UL
#endif

#endif /* WIFI_CONF_SHIM_H */
