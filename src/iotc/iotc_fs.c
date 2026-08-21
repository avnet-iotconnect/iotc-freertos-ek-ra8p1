/*
 * iotc_fs.c - shared LittleFS (on OSPI flash) bring-up.
 *
 * Both the PKCS#11 credential store and the model store live on the same
 * LittleFS volume. Mount-first; format only a virgin/corrupt filesystem.
 * Thread-safe and idempotent.
 */

#include "iotc_fs.h"

#include <stdio.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "hal_data.h"
#include "lfs.h"

extern const rm_littlefs_instance_t g_rm_littlefs0;
extern struct lfs g_rm_littlefs0_lfs;
extern const struct lfs_config g_rm_littlefs0_lfs_cfg;

static SemaphoreHandle_t s_mutex;
static StaticSemaphore_t s_mutex_buf;
static bool s_ready;

int iotc_fs_init(void)
{
    if (NULL == s_mutex)
    {
        /* First call may race in theory; in practice the AI thread runs this
         * long before the net thread. Create the mutex eagerly. */
        s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buf);
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_ready)
    {
        xSemaphoreGive(s_mutex);
        return 0;
    }

    extern fsp_err_t print_to_console(char *p_data);
    static char dbg[96];

    /* Hardware-reset the OSPI flash (RESET# on P106) so it is in 1S SPI
     * mode regardless of what a previous firmware left behind (the factory
     * demo switches the MX25LW into octal DDR mode, which survives MCU
     * resets - only a power cycle or RESET# pulse clears it). */
    {
        R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_06,
                        IOPORT_CFG_PORT_DIRECTION_OUTPUT | IOPORT_CFG_PORT_OUTPUT_LOW);
        R_BSP_SoftwareDelay(100, BSP_DELAY_UNITS_MICROSECONDS);
        R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_06, BSP_IO_LEVEL_HIGH);
        R_BSP_SoftwareDelay(1, BSP_DELAY_UNITS_MILLISECONDS);
        /* Hand the pin back to the OSPI peripheral. */
        R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_01_PIN_06,
                        IOPORT_CFG_DRIVE_MID | IOPORT_CFG_PERIPHERAL_PIN | IOPORT_PERIPHERAL_OSPI);
    }

    int rc = -1;
    fsp_err_t err = g_rm_littlefs0.p_api->open(g_rm_littlefs0.p_ctrl, g_rm_littlefs0.p_cfg);
    {
        /* Probe: direct OSPI erase/program/readback at 63 MB (outside the
         * 32 MB littlefs partition), with raw FSP error codes. */
        extern const spi_flash_instance_t g_ospi0;
        volatile uint8_t *xip = (volatile uint8_t *) 0x90000000u;
        uint8_t *probe = (uint8_t *) 0x93F00000u; /* +63 MB */
        spi_flash_status_t st = {0};

        fsp_err_t e1 = g_ospi0.p_api->erase(g_ospi0.p_ctrl, probe, 4096);
        fsp_err_t e2 = FSP_SUCCESS;
        for (int i = 0; i < 10000; i++)
        {
            e2 = g_ospi0.p_api->statusGet(g_ospi0.p_ctrl, &st);
            if ((FSP_SUCCESS != e2) || !st.write_in_progress) { break; }
            vTaskDelay(1);
        }
        static const uint8_t pat[8] = {0xA5,1,2,3,4,5,6,7};
        fsp_err_t e3 = g_ospi0.p_api->write(g_ospi0.p_ctrl, (uint8_t *) pat, probe, sizeof(pat));
        for (int i = 0; i < 10000; i++)
        {
            if ((FSP_SUCCESS != g_ospi0.p_api->statusGet(g_ospi0.p_ctrl, &st)) || !st.write_in_progress) { break; }
            vTaskDelay(1);
        }
        SCB_InvalidateDCache_by_Addr((void *) probe, 32);
        snprintf(dbg, sizeof(dbg),
                 "FS: open=%d erase=%d poll=%d write=%d rb=%02x %02x %02x xip0=%02x\r\n",
                 (int) err, (int) e1, (int) e2, (int) e3, probe[0], probe[1], probe[2], xip[0]);
        print_to_console(dbg);
    }
/* Set to 1 once to wipe a filesystem written by older/broken firmware. */
#define IOTC_FS_FORCE_FORMAT 1

    if ((FSP_SUCCESS == err) || (FSP_ERR_ALREADY_OPEN == err))
    {
#if IOTC_FS_FORCE_FORMAT
        snprintf(dbg, sizeof(dbg), "FS: geometry rd=%u pg=%u blk=%u cnt=%u cache=%u look=%u\r\n",
                 (unsigned) g_rm_littlefs0_lfs_cfg.read_size,
                 (unsigned) g_rm_littlefs0_lfs_cfg.prog_size,
                 (unsigned) g_rm_littlefs0_lfs_cfg.block_size,
                 (unsigned) g_rm_littlefs0_lfs_cfg.block_count,
                 (unsigned) g_rm_littlefs0_lfs_cfg.cache_size,
                 (unsigned) g_rm_littlefs0_lfs_cfg.lookahead_size);
        print_to_console(dbg);
        int ff = lfs_format(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
        snprintf(dbg, sizeof(dbg), "FS: FORCE FORMAT rc=%d\r\n", ff);
        print_to_console(dbg);
#endif
        int lfs_err = lfs_mount(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
        if (0 != lfs_err)
        {
            snprintf(dbg, sizeof(dbg), "FS: mount rc=%d; formatting\r\n", lfs_err);
            print_to_console(dbg);
            lfs_err = lfs_format(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
            if (0 == lfs_err)
            {
                lfs_err = lfs_mount(&g_rm_littlefs0_lfs, &g_rm_littlefs0_lfs_cfg);
            }
        }
        if (0 == lfs_err)
        {
            s_ready = true;
            rc = 0;
        }
        else
        {
            snprintf(dbg, sizeof(dbg), "FS: format/mount failed rc=%d\r\n", lfs_err);
            print_to_console(dbg);
        }
    }
    else
    {
        snprintf(dbg, sizeof(dbg), "FS: littlefs/ospi open failed err=%d\r\n", (int) err);
        print_to_console(dbg);
    }
    xSemaphoreGive(s_mutex);
    return rc;
}

bool iotc_fs_is_ready(void)
{
    return s_ready;
}

void iotc_fs_lock(void)
{
    if (s_mutex)
    {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
}

void iotc_fs_unlock(void)
{
    if (s_mutex)
    {
        xSemaphoreGive(s_mutex);
    }
}

struct lfs *iotc_fs_lfs(void)
{
    return &g_rm_littlefs0_lfs;
}
