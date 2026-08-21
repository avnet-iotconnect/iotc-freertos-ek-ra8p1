/*
 * net_thread_entry.c - Ethernet + FreeRTOS+TCP bring-up for the
 * IOTCONNECT EK-RA8P1 Vision AI demo.
 *
 * Phase 1 scope: bring the RGMII link up, run DHCP, and report the IP
 * configuration on the console while the camera/NPU/display threads run.
 * The IoTConnect client (Phase 3) attaches on top of this stack.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_thread.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "common_util.h"
#include "console_output/console_output.h"

/* MAC must match the module.driver.ether MAC in configuration.xml. */
static uint8_t s_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
/* DHCP overwrites these; they are the static fallback. */
static uint8_t s_ip[4]      = {192, 168, 0, 52};
static uint8_t s_netmask[4] = {255, 255, 255, 0};
static uint8_t s_gateway[4] = {192, 168, 0, 1};
static uint8_t s_dns[4]     = {8, 8, 8, 8};

static volatile bool s_dhcp_bound = false;

/* IoTConnect application glue (src/iotc/iotc_app.c). */
extern void iotc_app_poll(bool network_up);

static char s_print_buf[256];

#define NET_PRINT(...)                                        \
    do {                                                      \
        snprintf(s_print_buf, sizeof(s_print_buf), __VA_ARGS__); \
        print_to_console(s_print_buf);                        \
    } while (0)

uint32_t ulRand(void)
{
    /* rand() returns 15 bits; assemble 32. TODO: back with the RA8P1 TRNG
     * before TLS sequence numbers matter (coreMQTT phase uses mbedTLS TRNG). */
    return ((((uint32_t) rand()) & 0x7fffuL)) |
           ((((uint32_t) rand()) & 0x7fffuL) << 15) |
           ((((uint32_t) rand()) & 0x0003uL) << 30);
}

uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
                                            uint16_t usSourcePort,
                                            uint32_t ulDestinationAddress,
                                            uint16_t usDestinationPort)
{
    return (ulSourceAddress + ulDestinationAddress + usSourcePort + usDestinationPort) ^ ulRand();
}

#if (ipconfigUSE_DHCP != 0)
eDHCPCallbackAnswer_t xApplicationDHCPHook(eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress)
{
    FSP_PARAMETER_NOT_USED(ulIPAddress);
    if (eDHCPPhase == eDHCPPhasePreRequest)
    {
        s_dhcp_bound = true;
    }
    return eDHCPContinue;
}
#endif

#if (ipconfigDHCP_REGISTER_HOSTNAME == 1)
const char *pcApplicationHostnameHook(void)
{
    return "iotc-ek-ra8p1";
}
#endif

void vApplicationPingReplyHook(ePingReplyStatus_t eStatus, uint16_t usIdentifier)
{
    FSP_PARAMETER_NOT_USED(eStatus);
    FSP_PARAMETER_NOT_USED(usIdentifier);
}

static void print_ip_config(void)
{
    uint32_t ip, mask, gw, dns;
    FreeRTOS_GetAddressConfiguration(&ip, &mask, &gw, &dns);
    char ip_s[16], mask_s[16], gw_s[16], dns_s[16];
    FreeRTOS_inet_ntoa(ip, ip_s);
    FreeRTOS_inet_ntoa(mask, mask_s);
    FreeRTOS_inet_ntoa(gw, gw_s);
    FreeRTOS_inet_ntoa(dns, dns_s);
    NET_PRINT("\r\nNetwork up (%s):\r\n", s_dhcp_bound ? "DHCP" : "static");
    NET_PRINT("  MAC     : %02x:%02x:%02x:%02x:%02x:%02x\r\n",
              s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5]);
    NET_PRINT("  IPv4    : %s\r\n", ip_s);
    NET_PRINT("  Netmask : %s\r\n", mask_s);
    NET_PRINT("  Gateway : %s\r\n", gw_s);
    NET_PRINT("  DNS     : %s\r\n", dns_s);
}

void net_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    NET_PRINT("\r\nEthernet: initializing FreeRTOS+TCP\r\n");

    if (pdFALSE == FreeRTOS_IPInit(s_ip, s_netmask, s_gateway, s_dns, s_mac))
    {
        NET_PRINT("Ethernet: FreeRTOS_IPInit FAILED\r\n");
        vTaskSuspend(NULL);
    }

    bool reported_up = false;
    bool reported_down = false;
    while (true)
    {
        /* Drive PHY link management; required for RMAC. */
        (void) R_RMAC_LinkProcess(g_ether0.p_ctrl);

        bool up = (pdTRUE == FreeRTOS_IsNetworkUp());
#if (ipconfigUSE_DHCP != 0)
        up = up && s_dhcp_bound;
#endif
        if (up && !reported_up)
        {
            print_ip_config();
            reported_up = true;
            reported_down = false;
        }
        else if (!up && !reported_down)
        {
            NET_PRINT("\r\nEthernet: waiting for link/DHCP...\r\n");
            reported_down = true;
            reported_up = false;
        }

        iotc_app_poll(up);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
