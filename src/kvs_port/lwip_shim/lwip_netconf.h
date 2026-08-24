/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * lwip_netconf.h shim: the ICE controller calls LwIP_GetIP(0) to learn the
 * local IPv4 address for host candidates. Backed by FreeRTOS+TCP.
 */
#ifndef KVS_LWIP_NETCONF_H
#define KVS_LWIP_NETCONF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns a pointer to a static 4-byte array holding the local IPv4 address
 * in network byte order (index 0 = first octet). */
uint8_t *LwIP_GetIP(int interface_index);

#ifdef __cplusplus
}
#endif

#endif /* KVS_LWIP_NETCONF_H */
