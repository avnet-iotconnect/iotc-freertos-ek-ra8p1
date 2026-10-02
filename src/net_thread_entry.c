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

/*
 * Station MAC address. The first three octets are fixed; bit 1 of octet 0 is
 * set, marking this a locally administered address, so it does not claim any
 * manufacturer's OUI. The last three are derived at boot from the MCU's unique
 * ID (prv_mac_from_unique_id below), which is what keeps two boards running
 * the same image off each other's toes - a fixed address made every board look
 * identical to the switch, the DHCP server and ARP.
 *
 * The low octets are filled in before FreeRTOS_IPInit, which is what opens the
 * Ethernet driver; the initial value here is never put on the wire.
 */
static uint8_t s_mac[6] = {0x02, 0x8A, 0x9B, 0x00, 0x00, 0x00};
/* DHCP overwrites these; they are the static fallback. */
static uint8_t s_ip[4]      = {192, 168, 71, 222};
static uint8_t s_netmask[4] = {255, 255, 252, 0};
static uint8_t s_gateway[4] = {192, 168, 68, 1};
static uint8_t s_dns[4]     = {8, 8, 8, 8};

static volatile bool s_dhcp_bound = false;

/* IoTConnect application glue (src/iotc/iotc_app.c). */
extern void iotc_app_poll(bool network_up);
/* Serial provisioning CLI (src/iotc/iotc_cli.c). */
extern void iotc_cli_start(void);

/* Debug breadcrumb, read via J-Link when the console is quiet. */
volatile uint32_t g_net_step;

/*
 * MAC address arrays the FSP configuration generates. The RMAC driver programs
 * the controller from g_ether0_mac_address when the interface is opened, and
 * the Ethernet switch takes its per-port addresses from its own pair, so all
 * three have to carry the derived address. They are writable (not const), so
 * this needs no patch to the generated sources - which matters because the
 * Smart Configurator rewrites ra_gen/ on every build.
 */
extern uint8_t g_ether0_mac_address[6];
extern uint8_t g_layer3_switch0_mac_address_port0[6];
extern uint8_t g_layer3_switch0_mac_address_port1[6];

/*
 * Derive the host portion of the MAC from the MCU's 128-bit unique ID, so each
 * board gets its own address without per-unit configuration, and keeps the same
 * one across reboots and reflashes. The ID is folded with FNV-1a rather than
 * taking four bytes of it directly: neighbouring parts can share long runs, and
 * a hash spreads those differences across all three octets.
 */
static void prv_mac_from_unique_id(void)
{
    bsp_unique_id_t const *p_uid = R_BSP_UniqueIdGet();
    uint32_t hash = 2166136261U; /* FNV-1a offset basis */

    for (uint32_t i = 0; i < sizeof(p_uid->unique_id_bytes); i++)
    {
        hash ^= (uint32_t) p_uid->unique_id_bytes[i];
        hash *= 16777619U; /* FNV-1a prime */
    }

    s_mac[3] = (uint8_t) (hash >> 16);
    s_mac[4] = (uint8_t) (hash >> 8);
    s_mac[5] = (uint8_t) (hash);

    memcpy(g_ether0_mac_address, s_mac, sizeof(s_mac));
    memcpy(g_layer3_switch0_mac_address_port0, s_mac, sizeof(s_mac));
    memcpy(g_layer3_switch0_mac_address_port1, s_mac, sizeof(s_mac));
}

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

/* FreeRTOS+TCP's weak default returns pdFALSE, which silently aborts DHCP
 * (no transaction ID). TODO: back with the RSIP TRNG for TLS-grade entropy. */
BaseType_t xApplicationGetRandomNumber(uint32_t *pulNumber)
{
    static uint32_t s_seeded;
    if (!s_seeded)
    {
        s_seeded = 1;
        srand((unsigned) xTaskGetTickCount() ^ 0xA5A5A5A5u);
    }
    *pulNumber = ulRand();
    return pdTRUE;
}

uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
                                            uint16_t usSourcePort,
                                            uint32_t ulDestinationAddress,
                                            uint16_t usDestinationPort)
{
    return (ulSourceAddress + ulDestinationAddress + usSourcePort + usDestinationPort) ^ ulRand();
}

static volatile uint32_t s_dhcp_discovers;

#if (ipconfigUSE_DHCP != 0)
eDHCPCallbackAnswer_t xApplicationDHCPHook(eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress)
{
    FSP_PARAMETER_NOT_USED(ulIPAddress);
    if (eDHCPPhase == eDHCPPhasePreDiscover)
    {
        s_dhcp_discovers++;
    }
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

static volatile uint32_t s_ping_replies;

void vApplicationPingReplyHook(ePingReplyStatus_t eStatus, uint16_t usIdentifier)
{
    FSP_PARAMETER_NOT_USED(usIdentifier);
    if (eSuccess == eStatus)
    {
        s_ping_replies++;
    }
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

    iotc_cli_start();

    /* Must precede FreeRTOS_IPInit: that is where the Ethernet driver opens
     * and latches the address into the controller. */
    prv_mac_from_unique_id();
    NET_PRINT("Ethernet: MAC %02x:%02x:%02x:%02x:%02x:%02x (from MCU unique ID)\r\n",
              s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5]);

    if (pdFALSE == FreeRTOS_IPInit(s_ip, s_netmask, s_gateway, s_dns, s_mac))
    {
        NET_PRINT("Ethernet: FreeRTOS_IPInit FAILED\r\n");
        vTaskSuspend(NULL);
    }

    bool reported_up = false;
    bool reported_down = false;
    uint32_t loops = 0;
    extern volatile uint32_t g_net_step;
    while (true)
    {
        /* Link management runs in the rm_freertos_plus_tcp wrapper's link
         * status task; calling R_RMAC_LinkProcess here as well races it. */
        fsp_err_t link = 0;

        bool ip_up = (pdTRUE == FreeRTOS_IsNetworkUp());
        if (0 == (loops++ % 60)) /* every ~30s */
        {
            NET_PRINT("Ethernet: up=%d dhcp=%d pong=%u\r\n",
                      (int) ip_up, (int) s_dhcp_bound, (unsigned) s_ping_replies);
            if (ip_up)
            {
                (void) FreeRTOS_SendPingRequest(FreeRTOS_GetGatewayAddress(), 8,
                                                100 / portTICK_PERIOD_MS);
            }
        }
        (void) link;

        bool up = ip_up;
#if (ipconfigUSE_DHCP != 0)
        up = up && s_dhcp_bound;
#endif
        g_net_step = 100 + (loops % 10);
        if (up && !reported_up)
        {
            g_net_step = 200;
            print_ip_config();
            g_net_step = 201;
            reported_up = true;
            reported_down = false;
        }
        else if (!up && !reported_down)
        {
            NET_PRINT("\r\nEthernet: waiting for link/DHCP...\r\n");
            reported_down = true;
            reported_up = false;
        }

        g_net_step = 300;
        iotc_app_poll(up);
        g_net_step = 301;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
