/* generated common source file - do not edit */
#include "common_data.h"

#ifndef vLoggingPrintf
#include <stdarg.h>

void vLoggingPrintf (const char * pcFormat, ...)
{
    FSP_PARAMETER_NOT_USED(pcFormat);
}
#endif

#ifndef vLoggingPrint
void vLoggingPrint(const char * pcFormat);

void vLoggingPrint (const char * pcFormat)
{
    FSP_PARAMETER_NOT_USED(pcFormat);
}
#endif


dmac_instance_ctrl_t g_transfer_ospi_ctrl;
transfer_info_t g_transfer_ospi_info =
{
    .transfer_settings_word_b.dest_addr_mode = TRANSFER_ADDR_MODE_INCREMENTED,
    .transfer_settings_word_b.repeat_area    = TRANSFER_REPEAT_AREA_SOURCE,
    .transfer_settings_word_b.irq            = TRANSFER_IRQ_END,
    .transfer_settings_word_b.chain_mode     = TRANSFER_CHAIN_MODE_DISABLED,
    .transfer_settings_word_b.src_addr_mode  = TRANSFER_ADDR_MODE_INCREMENTED,
    .transfer_settings_word_b.size           = TRANSFER_SIZE_1_BYTE,
    .transfer_settings_word_b.mode           = TRANSFER_MODE_BLOCK,
    .p_dest                                  = (void *) NULL,
    .p_src                                   = (void const *) NULL,
    .num_blocks                              = 1,
    .length                                  = 64,
};
const dmac_extended_cfg_t g_transfer_ospi_extend =
{
    .offset              = 0,
    .src_buffer_size     = 0,
#if defined(VECTOR_NUMBER_DMAC0_INT)
    .irq                 = VECTOR_NUMBER_DMAC0_INT,
#else
    .irq                 = FSP_INVALID_VECTOR,
#endif
    .ipl                 = (BSP_IRQ_DISABLED),
    .channel             = 0,
    .p_callback          = NULL,
    .p_context           = NULL,
    .activation_source   = ELC_EVENT_NONE,
};
const transfer_cfg_t g_transfer_ospi_cfg =
{
    .p_info              = &g_transfer_ospi_info,
    .p_extend            = &g_transfer_ospi_extend,
};
/* Instance structure to use this module. */
const transfer_instance_t g_transfer_ospi =
{
    .p_ctrl        = &g_transfer_ospi_ctrl,
    .p_cfg         = &g_transfer_ospi_cfg,
    .p_api         = &g_transfer_on_dmac
};

ospi_b_instance_ctrl_t g_ospi0_ctrl;

static ospi_b_timing_setting_t g_ospi0_timing_settings =
{
    .command_to_command_interval = OSPI_B_COMMAND_INTERVAL_CLOCKS_2,
    .cs_pullup_lag               = OSPI_B_COMMAND_CS_PULLUP_CLOCKS_NO_EXTENSION,
    .cs_pulldown_lead            = OSPI_B_COMMAND_CS_PULLDOWN_CLOCKS_NO_EXTENSION,
    .sdr_drive_timing            = OSPI_B_SDR_DRIVE_TIMING_BEFORE_CK,
    .sdr_sampling_edge           = OSPI_B_CK_EDGE_FALLING,
    .sdr_sampling_delay          = OSPI_B_SDR_SAMPLING_DELAY_NONE,
    .ddr_sampling_extension      = OSPI_B_DDR_SAMPLING_EXTENSION_NONE,
};

static const spi_flash_erase_command_t g_ospi0_command_set_initial_erase_commands[] =
{
    { .command = 0x21, .size = 4096 },
    { .command = 0xDC, .size = 65536 },
    { .command = 0x60, .size = SPI_FLASH_ERASE_SIZE_CHIP_ERASE },
};
static const ospi_b_table_t g_ospi0_command_set_initial_erase_table =
{
    .p_table = (void *) g_ospi0_command_set_initial_erase_commands,
    .length = sizeof(g_ospi0_command_set_initial_erase_commands)/sizeof(g_ospi0_command_set_initial_erase_commands[0]),
};

static const spi_flash_erase_command_t g_ospi0_command_set_high_speed_erase_commands[] =
{
    { .command = 0x2121, .size = 4096 },
    { .command = 0xDCDC, .size = 65536 },
    { .command = 0x6060, .size = SPI_FLASH_ERASE_SIZE_CHIP_ERASE },
};
static const ospi_b_table_t g_ospi0_command_set_high_speed_erase_table =
{
    .p_table = (void *) g_ospi0_command_set_high_speed_erase_commands,
    .length = sizeof(g_ospi0_command_set_high_speed_erase_commands)/sizeof(g_ospi0_command_set_high_speed_erase_commands[0]),
};

static const ospi_b_xspi_command_set_t g_ospi0_command_set_table[] =
{
    {
        .protocol = SPI_FLASH_PROTOCOL_1S_1S_1S,
        .frame_format = OSPI_B_FRAME_FORMAT_STANDARD,
        .latency_mode = OSPI_B_LATENCY_MODE_FIXED,
        .command_bytes = OSPI_B_COMMAND_BYTES_1,
        .address_bytes = SPI_FLASH_ADDRESS_BYTES_4,
        .address_msb_mask = 0xF0,
        .status_needs_address =  false,
        .status_address = 0U,
        .status_address_bytes = (spi_flash_address_bytes_t) 0U,
        .p_erase_commands = &g_ospi0_command_set_initial_erase_table,
        .read_command = 0x0C,
        .read_dummy_cycles = 8,
        .program_command = 0x12,
        .program_dummy_cycles = 0,
        .row_load_command = 0x00,
        .row_load_dummy_cycles = 0,
        .row_store_command = 0x00,
        .row_store_dummy_cycles = 0,
        .write_enable_command = 0x06,
        .status_command = 0x05,
        .status_dummy_cycles = 0,
    },
    {
        .protocol = SPI_FLASH_PROTOCOL_8D_8D_8D,
        .frame_format = OSPI_B_FRAME_FORMAT_XSPI_PROFILE_1,
        .latency_mode = OSPI_B_LATENCY_MODE_FIXED,
        .command_bytes = OSPI_B_COMMAND_BYTES_2,
        .address_bytes = SPI_FLASH_ADDRESS_BYTES_4,
        .address_msb_mask = 0xF0,
        .status_needs_address =  true,
        .status_address = 0x00,
        .status_address_bytes = SPI_FLASH_ADDRESS_BYTES_4,
        .p_erase_commands = &g_ospi0_command_set_high_speed_erase_table,
        .read_command = 0xEEEE,
        .read_dummy_cycles = 20,
        .program_command = 0x1212,
        .program_dummy_cycles = 0,
        .row_load_command = 0x00,
        .row_load_dummy_cycles = 0,
        .row_store_command = 0x00,
        .row_store_dummy_cycles = 0,
        .write_enable_command = 0x0606,
        .status_command = 0x0505,
        .status_dummy_cycles = 3,
    }
};

static const ospi_b_table_t g_ospi0_command_set =
{
    .p_table = (void *) g_ospi0_command_set_table,
    .length = 2
};

#if OSPI_B_CFG_DOTF_SUPPORT_ENABLE
extern uint8_t g_ospi_dotf_iv[];
extern uint8_t g_ospi_dotf_key[];

static ospi_b_dotf_cfg_t g_ospi_dotf_cfg=
{
    .key_type       = OSPI_B_DOTF_AES_KEY_TYPE_128,
    .format         = OSPI_B_DOTF_KEY_FORMAT_PLAINTEXT,
    .p_start_addr   = (uint32_t *)0x90000000,
    .p_end_addr     = (uint32_t *)0x90001FFF,
    .p_key          = (uint32_t *)g_ospi_dotf_key,
    .p_iv           = (uint32_t *)g_ospi_dotf_iv,
};
#endif

static const ospi_b_extended_cfg_t g_ospi0_extended_cfg =
{
    .ospi_b_unit                             = 0,
    .channel                                 = (ospi_b_device_number_t) 1,
    .p_timing_settings                       = &g_ospi0_timing_settings,
    .p_xspi_command_set                      = &g_ospi0_command_set,
    .data_latch_delay_clocks                 = OSPI_B_DS_TIMING_DELAY_NONE,
    .p_autocalibration_preamble_pattern_addr = (uint8_t *) 0,
#if OSPI_B_CFG_DMAC_SUPPORT_ENABLE
    .p_lower_lvl_transfer                    = &g_transfer_ospi,
#endif
#if OSPI_B_CFG_DOTF_SUPPORT_ENABLE
    .p_dotf_cfg                              = &g_ospi_dotf_cfg,
#endif
#if OSPI_B_CFG_ROW_ADDRESSING_SUPPORT_ENABLE
    .row_index_bytes                         = 0xFF,
#endif
};
const spi_flash_cfg_t g_ospi0_cfg =
{
    .spi_protocol                = SPI_FLASH_PROTOCOL_1S_1S_1S,
    .read_mode                   = SPI_FLASH_READ_MODE_STANDARD, /* Unused by OSPI_B */
    .address_bytes               = SPI_FLASH_ADDRESS_BYTES_4,
    .dummy_clocks                = SPI_FLASH_DUMMY_CLOCKS_DEFAULT, /* Unused by OSPI_B */
    .page_program_address_lines  = (spi_flash_data_lines_t) 0U, /* Unused by OSPI_B */
    .page_size_bytes             = 64,
    .write_status_bit            = 0,
    .write_enable_bit            = 1,
    .page_program_command        = 0, /* OSPI_B uses command sets. See g_ospi0_command_set. */
    .write_enable_command        = 0, /* OSPI_B uses command sets. See g_ospi0_command_set. */
    .status_command              = 0, /* OSPI_B uses command sets. See g_ospi0_command_set. */
    .read_command                = 0, /* OSPI_B uses command sets. See g_ospi0_command_set. */
#if OSPI_B_CFG_XIP_SUPPORT_ENABLE
    .xip_enter_command           = 0,
    .xip_exit_command            = 0,
#else
    .xip_enter_command           = 0U,
    .xip_exit_command            = 0U,
#endif
    /* OSPI_B uses command sets, this is kept for backwards compatibility. See g_ospi0_command_set. */
    .erase_command_list_length   = sizeof(g_ospi0_command_set_initial_erase_commands)/sizeof(g_ospi0_command_set_initial_erase_commands[0]),
    .p_erase_command_list        = g_ospi0_command_set_initial_erase_commands,
    .p_extend                    = &g_ospi0_extended_cfg,
};

/** This structure encompasses everything that is needed to use an instance of this interface. */
const spi_flash_instance_t g_ospi0 =
{
    .p_ctrl = &g_ospi0_ctrl,
    .p_cfg =  &g_ospi0_cfg,
    .p_api =  &g_ospi_b_on_spi_flash,
};

#if defined OSPI_B_CFG_DOTF_PROTECTED_MODE_SUPPORT_ENABLE
rsip_instance_t const * const gp_rsip_instance = &RA_NOT_DEFINED;
#endif
rm_littlefs_spi_flash_instance_ctrl_t g_rm_littlefs0_ctrl;

#ifdef LFS_NO_MALLOC
static uint8_t g_rm_littlefs0_read[64];
static uint8_t g_rm_littlefs0_prog[64];
static uint8_t g_rm_littlefs0_lookahead[16];
#endif

struct lfs g_rm_littlefs0_lfs;

#define RA_NOT_DEFINED 0xFFFFFFFF

#if (RA_NOT_DEFINED != RA_NOT_DEFINED)
#define G_RM_LITTLEFS0_SECTOR_SIZE (RA_NOT_DEFINED)
#elif (RA_NOT_DEFINED != RA_NOT_DEFINED)
#define G_RM_LITTLEFS0_SECTOR_SIZE (RA_NOT_DEFINED)
#else
#define G_RM_LITTLEFS0_SECTOR_SIZE (4096)
#endif

const struct lfs_config g_rm_littlefs0_lfs_cfg =
{
    .context = &g_rm_littlefs0_ctrl,
    .read    = &rm_littlefs_spi_flash_read,
    .prog    = &rm_littlefs_spi_flash_write,
    .erase   = &rm_littlefs_spi_flash_erase,
    .sync    = &rm_littlefs_spi_flash_sync,
    .read_size      = 64,
    .prog_size      = 64,
    .block_size     = G_RM_LITTLEFS0_SECTOR_SIZE,
    .block_count    = (16777216/G_RM_LITTLEFS0_SECTOR_SIZE),
    .block_cycles   = 1024,
    .cache_size     = 64,
    .lookahead_size = 16,
#ifdef LFS_NO_MALLOC
    .read_buffer = (void *) g_rm_littlefs0_read,
    .prog_buffer = (void *) g_rm_littlefs0_prog,
    .lookahead_buffer = (void *) g_rm_littlefs0_lookahead,
#endif
#ifdef LFS_THREADSAFE
    .lock   = &rm_littlefs_spi_flash_lock,
    .unlock = &rm_littlefs_spi_flash_unlock,
#endif
};

const rm_littlefs_spi_flash_cfg_t g_rm_littlefs0_ext_cfg =
{
#if (RA_NOT_DEFINED != RA_NOT_DEFINED)
    .p_lower_lvl        = &RA_NOT_DEFINED,
    .base_address       = BSP_FEATURE_QSPI_DEVICE_START_ADDRESS,
#elif (RA_NOT_DEFINED != RA_NOT_DEFINED)
    .p_lower_lvl        = &RA_NOT_DEFINED,
    .base_address       = BSP_FEATURE_OSPI_DEVICE_RA_NOT_DEFINED_START_ADDRESS,
#else
    .p_lower_lvl        = &g_ospi0,
    .base_address       = BSP_OSPI0_CS1_START_ADDRESS,
#endif
    .address_offset     = 0x02000000,
    .size               = 16777216,
    .poll_status_count  = 0xFFFFFFFF,
    .p_context          = NULL,
    .p_callback         = g_rm_littlefs_spi_flash0_callback
};
#undef RA_NOT_DEFINED

const rm_littlefs_cfg_t g_rm_littlefs0_cfg =
{
    .p_lfs_cfg    = &g_rm_littlefs0_lfs_cfg,
    .p_extend     = &g_rm_littlefs0_ext_cfg
};

/* Instance structure to use this module. */
const rm_littlefs_instance_t g_rm_littlefs0 =
{
    .p_ctrl        = &g_rm_littlefs0_ctrl,
    .p_cfg         = &g_rm_littlefs0_cfg,
    .p_api         = &g_rm_littlefs_on_flash,
};
const ether_phy_lsi_cfg_t g_rmac_phy_lsi1 =
{
    .address           = 0,
    .type              = ETHER_PHY_LSI_TYPE_KIT_COMPONENT,
};
rmac_phy_instance_ctrl_t g_rmac_phy0_ctrl;
#define RA_NOT_DEFINED (1)
const rmac_phy_extended_cfg_t g_rmac_phy0_extended_cfg =
{
    .p_target_init                     = NULL,
    .p_target_link_partner_ability_get = NULL,
    .frame_format                      = RMAC_PHY_FRAME_FORMAT_MDIO,
    .mdc_clock_rate                    = 2500000,
    .mdio_hold_time                    = 0,
    .mdio_capture_time                 = 0,
    .p_phy_lsi_cfg_list = {
#if (RA_NOT_DEFINED != RA_NOT_DEFINED)
    	&RA_NOT_DEFINED,
#else
    	NULL,
#endif
#if (RA_NOT_DEFINED != g_rmac_phy_lsi1)
    	&g_rmac_phy_lsi1,
#else
    	NULL,
#endif
    },
    .default_phy_lsi_cfg_index = 1,
    .frame_preemption_enable = false,
    .frame_preemption_verification_interval = 10,
    .easi_irq =
    {
    #if defined(VECTOR_NUMBER_ETHER_EASI0)
        VECTOR_NUMBER_ETHER_EASI0,
    #else
        FSP_INVALID_VECTOR,
    #endif
    #if defined(VECTOR_NUMBER_ETHER_EASI1)
        VECTOR_NUMBER_ETHER_EASI1,
    #else
        FSP_INVALID_VECTOR,
    #endif
    },
    .easi_ipl =
    {
        (BSP_IRQ_DISABLED),
        (BSP_IRQ_DISABLED),
    },
    .p_callback = NULL,
    .p_context = NULL,
};
#undef RA_NOT_DEFINED

const ether_phy_cfg_t g_rmac_phy0_cfg =
{

    .channel                   = 1,
    .phy_reset_wait_time       = 0x00020000,
    .mii_bit_access_wait_time  = 0,
    .flow_control              = ETHER_PHY_FLOW_CONTROL_DISABLE,
    .mii_type                  = ETHER_PHY_MII_TYPE_RGMII,
    .p_context                 = NULL,
    .p_extend                  = &g_rmac_phy0_extended_cfg,

};
/* Instance structure to use this module. */
const ether_phy_instance_t g_rmac_phy0 =
{
    .p_ctrl        = &g_rmac_phy0_ctrl,
    .p_cfg         = &g_rmac_phy0_cfg,
    .p_api         = &g_ether_phy_on_rmac_phy
};
layer3_switch_instance_ctrl_t g_layer3_switch0_ctrl;

uint8_t g_layer3_switch0_mac_address_port0[6] = { 0x02,0x8A,0x9B,0x71,0x04,0xD2 };
uint8_t g_layer3_switch0_mac_address_port1[6] = { 0x02,0x8A,0x9B,0x71,0x04,0xD2 };
layer3_switch_l3_filter_t g_layer3_switch0_l3_filter_list[10];

#define RA_NOT_DEFINED (1)
layer3_switch_cbs_cfg_t p_cbs_cfg_list_port0 =
{
    .band_width_list =
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
    .max_burst_num_list =
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
};
layer3_switch_cbs_cfg_t p_cbs_cfg_list_port1 =
{
    .band_width_list =
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
    .max_burst_num_list =
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
};

layer3_switch_port_cfg_t g_layer3_switch0_port0_cfg =
{
    .p_cbs_cfg = &p_cbs_cfg_list_port0,
    .forwarding_to_cpu_enable = true,
    .p_mac_address = g_layer3_switch0_mac_address_port0,
    .p_callback = NULL,
    .p_callback_memory = NULL,
    .p_context = NULL,
};
layer3_switch_port_cfg_t g_layer3_switch0_port1_cfg =
{
    .p_cbs_cfg = &p_cbs_cfg_list_port1,
    .forwarding_to_cpu_enable = true,
    .p_mac_address = g_layer3_switch0_mac_address_port1,
    .p_callback = NULL,
    .p_callback_memory = NULL,
    .p_context = NULL,
};

const layer3_switch_extended_cfg_t g_layer3_switch0_extended_cfg =
{
    .p_ether_phy_instances          = {
#if (RA_NOT_DEFINED != RA_NOT_DEFINED)
    &RA_NOT_DEFINED,
#else
        NULL,
#endif
#if (RA_NOT_DEFINED != g_rmac_phy0)
    &g_rmac_phy0,
#else
        NULL,
#endif
    },
    .fowarding_target_port_masks =
    {
        (LAYER3_SWITCH_PORT_BITMASK_PORT2 |  0U),
        (LAYER3_SWITCH_PORT_BITMASK_PORT2 |  0U),
    },
    .ipv_queue_preemptable_bitmask =
    {
        ( 0U),
        ( 0U),
    },
    .frame_preemption_fragment_size =
    {
    	LAYER3_SWITCH_PREEMPTABLE_FRAME_FRAGMENT_SIZE_64BYTE,
    	LAYER3_SWITCH_PREEMPTABLE_FRAME_FRAGMENT_SIZE_64BYTE,
    },
    .p_mac_addresses =
    {
    g_layer3_switch0_mac_address_port0,
    g_layer3_switch0_mac_address_port1
    },
    .ipv_queue_depth_list =
    {
        {
            64, 64,
            64, 64,
            64, 64,
            64, 64
        },
        {
            64, 64,
            64, 64,
            64, 64,
            64, 64
        }
    },
    .p_port_cfg_list = { &g_layer3_switch0_port0_cfg, &g_layer3_switch0_port1_cfg },
    .l3_filter_list = g_layer3_switch0_l3_filter_list,
    .l3_filter_list_length = 10,
    .etha_error_irq_port_0 =
    #if defined(VECTOR_NUMBER_ETHER_EAEI0)
    VECTOR_NUMBER_ETHER_EAEI0,
    #else
        FSP_INVALID_VECTOR,
    #endif
    .etha_error_irq_port_1 =
    #if defined(VECTOR_NUMBER_ETHER_EAEI1)
    VECTOR_NUMBER_ETHER_EAEI1,
    #else
        FSP_INVALID_VECTOR,
    #endif
    .etha_error_ipl_port_0 = (12),
    .etha_error_ipl_port_1 = (12),
    .p_gptp_instance =
#if (RA_NOT_DEFINED != RA_NOT_DEFINED)
    &RA_NOT_DEFINED,
#else
        NULL,
#endif
    .gptp_timer_numbers = {0, 1},
};
#undef RA_NOT_DEFINED

const ether_switch_cfg_t g_layer3_switch0_cfg =
{
    .channel        = 0,

#if defined(VECTOR_NUMBER_ETHER_GWDI0)
    .irq            = VECTOR_NUMBER_ETHER_GWDI0,
#else
    .irq            = FSP_INVALID_VECTOR,
#endif

    .ipl            = (12),

    .p_callback     = NULL,
    .p_context      = NULL,
    .p_extend       = &g_layer3_switch0_extended_cfg,
};

/* Instance structure to use this module. */
const ether_switch_instance_t g_layer3_switch0 =
{
    .p_ctrl        = &g_layer3_switch0_ctrl,
    .p_cfg         = &g_layer3_switch0_cfg,
    .p_api         = &g_ether_switch_on_layer3_switch
};
rmac_instance_ctrl_t g_ether0_ctrl;
            static rmac_buffer_node_t g_ether0_buffer_node_list[24];

            uint8_t g_ether0_mac_address[6] = { 0x02,0x8A,0x9B,0x71,0x04,0xD2 };

            layer3_switch_ts_reception_process_descriptor_t g_ether0_ts_descriptor_array0[8];rmac_queue_info_t g_ether0_ts_queue[1] =
 {
{ .queue_cfg={.array_length          = 8,
.p_descriptor_array    = NULL,
.p_ts_descriptor_array = g_ether0_ts_descriptor_array0,
.ports                 = (1 << 1),
.type                  = LAYER3_SWITCH_QUEUE_TYPE_TX,
.write_back_mode       = LAYER3_SWITCH_WRITE_BACK_MODE_FULL,
.descriptor_format     = LAYER3_SWITCH_DISCRIPTOR_FORMTAT_TX_TIMESTAMP,
.rx_timestamp_storage  = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_DISABLE,
}},
};
            layer3_switch_descriptor_t           g_ether0_tx_descriptor_array0[4+1];layer3_switch_descriptor_t           g_ether0_tx_descriptor_array1[4+1];rmac_queue_info_t g_ether0_tx_queue_list[2] =
 {
{ .queue_cfg={.array_length       = 4+1,
.p_descriptor_array = g_ether0_tx_descriptor_array0,
.p_ts_descriptor_array = NULL,
.ports              = (1 << 1 ),
.type               = LAYER3_SWITCH_QUEUE_TYPE_TX,
.write_back_mode    = LAYER3_SWITCH_WRITE_BACK_MODE_FULL,
.descriptor_format  = LAYER3_SWITCH_DISCRIPTOR_FORMTAT_EXTENDED,
.rx_timestamp_storage = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_DISABLE,
}},
{ .queue_cfg={.array_length       = 4+1,
.p_descriptor_array = g_ether0_tx_descriptor_array1,
.p_ts_descriptor_array = NULL,
.ports              = (1 << 1 ),
.type               = LAYER3_SWITCH_QUEUE_TYPE_TX,
.write_back_mode    = LAYER3_SWITCH_WRITE_BACK_MODE_FULL,
.descriptor_format  = LAYER3_SWITCH_DISCRIPTOR_FORMTAT_EXTENDED,
.rx_timestamp_storage = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_DISABLE,
}},
};
            layer3_switch_descriptor_t           g_ether0_rx_descriptor_array0[4+1];layer3_switch_descriptor_t           g_ether0_rx_descriptor_array1[4+1];rmac_queue_info_t g_ether0_rx_queue_list[2] =
 {
{ .queue_cfg={.array_length       = 4+1,
.p_descriptor_array = g_ether0_rx_descriptor_array0,
.p_ts_descriptor_array = NULL,
.ports              = (1 << 1),
.type               = LAYER3_SWITCH_QUEUE_TYPE_RX,
.write_back_mode    = LAYER3_SWITCH_WRITE_BACK_MODE_FULL,
.descriptor_format  = LAYER3_SWITCH_DISCRIPTOR_FORMTAT_EXTENDED,
#if LAYER3_SWITCH_CFG_GPTP_ENABLE
.rx_timestamp_storage = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_ENABLE,
#else
.rx_timestamp_storage = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_DISABLE,
#endif
}},
{ .queue_cfg={.array_length       = 4+1,
.p_descriptor_array = g_ether0_rx_descriptor_array1,
.p_ts_descriptor_array = NULL,
.ports              = (1 << 1),
.type               = LAYER3_SWITCH_QUEUE_TYPE_RX,
.write_back_mode    = LAYER3_SWITCH_WRITE_BACK_MODE_FULL,
.descriptor_format  = LAYER3_SWITCH_DISCRIPTOR_FORMTAT_EXTENDED,
#if LAYER3_SWITCH_CFG_GPTP_ENABLE
.rx_timestamp_storage = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_ENABLE,
#else
.rx_timestamp_storage = LAYER3_SWITCH_RX_TIMESTAMP_STORAGE_DISABLE,
#endif
}},
};

            const rmac_extended_cfg_t g_ether0_extended_cfg_t =
            {
                .p_ether_switch      = &g_layer3_switch0,
                .tx_queue_num        = 2,
                .rx_queue_num        = 2,

                .p_ts_queue     = g_ether0_ts_queue,
                .p_tx_queue_list     = g_ether0_tx_queue_list,
                .p_rx_queue_list     = g_ether0_rx_queue_list,
#if defined(VECTOR_NUMBER_ETHER_RMPI1)
                .rmpi_irq                = VECTOR_NUMBER_ETHER_RMPI1,
#else
                .rmpi_irq                = FSP_INVALID_VECTOR,
#endif
                .rmpi_ipl                = (BSP_IRQ_DISABLED),
                .p_buffer_node_list      = g_ether0_buffer_node_list,
                .buffer_node_num         = 24,

            };
            uint8_t g_ether0_ether_buffer0[1536];
uint8_t g_ether0_ether_buffer1[1536];
uint8_t g_ether0_ether_buffer2[1536];
uint8_t g_ether0_ether_buffer3[1536];
uint8_t g_ether0_ether_buffer4[1536];
uint8_t g_ether0_ether_buffer5[1536];
uint8_t g_ether0_ether_buffer6[1536];
uint8_t g_ether0_ether_buffer7[1536];
uint8_t g_ether0_ether_buffer8[1536];
uint8_t g_ether0_ether_buffer9[1536];
uint8_t g_ether0_ether_buffer10[1536];
uint8_t g_ether0_ether_buffer11[1536];
uint8_t g_ether0_ether_buffer12[1536];
uint8_t g_ether0_ether_buffer13[1536];
uint8_t g_ether0_ether_buffer14[1536];
uint8_t g_ether0_ether_buffer15[1536];
uint8_t g_ether0_ether_buffer16[1536];
uint8_t g_ether0_ether_buffer17[1536];
uint8_t g_ether0_ether_buffer18[1536];
uint8_t g_ether0_ether_buffer19[1536];
uint8_t g_ether0_ether_buffer20[1536];
uint8_t g_ether0_ether_buffer21[1536];
uint8_t g_ether0_ether_buffer22[1536];
uint8_t g_ether0_ether_buffer23[1536];

            uint8_t *pp_g_ether0_ether_buffers[24] = {
(uint8_t *) &g_ether0_ether_buffer0[0],
(uint8_t *) &g_ether0_ether_buffer1[0],
(uint8_t *) &g_ether0_ether_buffer2[0],
(uint8_t *) &g_ether0_ether_buffer3[0],
(uint8_t *) &g_ether0_ether_buffer4[0],
(uint8_t *) &g_ether0_ether_buffer5[0],
(uint8_t *) &g_ether0_ether_buffer6[0],
(uint8_t *) &g_ether0_ether_buffer7[0],
(uint8_t *) &g_ether0_ether_buffer8[0],
(uint8_t *) &g_ether0_ether_buffer9[0],
(uint8_t *) &g_ether0_ether_buffer10[0],
(uint8_t *) &g_ether0_ether_buffer11[0],
(uint8_t *) &g_ether0_ether_buffer12[0],
(uint8_t *) &g_ether0_ether_buffer13[0],
(uint8_t *) &g_ether0_ether_buffer14[0],
(uint8_t *) &g_ether0_ether_buffer15[0],
(uint8_t *) &g_ether0_ether_buffer16[0],
(uint8_t *) &g_ether0_ether_buffer17[0],
(uint8_t *) &g_ether0_ether_buffer18[0],
(uint8_t *) &g_ether0_ether_buffer19[0],
(uint8_t *) &g_ether0_ether_buffer20[0],
(uint8_t *) &g_ether0_ether_buffer21[0],
(uint8_t *) &g_ether0_ether_buffer22[0],
(uint8_t *) &g_ether0_ether_buffer23[0],
};
            const ether_cfg_t g_ether0_cfg =
            {
                .channel            = 1,
                .zerocopy           = ETHER_ZEROCOPY_DISABLE,
                .multicast          = ETHER_MULTICAST_ENABLE,
                .promiscuous        = ETHER_PROMISCUOUS_DISABLE,
                .flow_control       = ETHER_FLOW_CONTROL_DISABLE,
                .padding            = ETHER_PADDING_DISABLE,
                .padding_offset     = 0,
                .broadcast_filter   = 0,
                .p_mac_address      = g_ether0_mac_address,

                .num_tx_descriptors = 12,
                .num_rx_descriptors = 12,

                .pp_ether_buffers   = pp_g_ether0_ether_buffers,

                .ether_buffer_size  = 1536,

                .irq                = FSP_INVALID_VECTOR,

                .p_callback         = vEtherISRCallback,
                .p_context          = &g_freertos_plus_tcp0,
                .p_extend           = &g_ether0_extended_cfg_t,
            };

/* Instance structure to use this module. */
const ether_instance_t g_ether0 =
{
    .p_ctrl        = &g_ether0_ctrl,
    .p_cfg         = &g_ether0_cfg,
    .p_api         = &g_ether_on_rmac,
};
#if (ipconfigIPv4_BACKWARD_COMPATIBLE == 0)
 NetworkInterface_t g_freertos_plus_tcp0_xInterface = {.pvArgument = (void *) &g_freertos_plus_tcp0};
#else
 rm_freertos_plus_tcp_instance_t * gp_freertos_plus_tcp_instance = &g_freertos_plus_tcp0;
#endif

static rm_freertos_plus_tcp_ctrl_t g_freertos_plus_tcp0_ctrl;

static rm_freertos_plus_tcp_cfg_t  g_freertos_plus_tcp0_cfg =
{
    .p_ether_instance = (ether_instance_t *)(&g_ether0),
    .rx_handler_task_stacksize = configMINIMAL_STACK_SIZE,
    .rx_handler_task_priority = configMAX_PRIORITIES - 1,
    .check_link_status_task_stacksize = configMINIMAL_STACK_SIZE,
    .check_link_status_task_priority = configMAX_PRIORITIES - 1,
    .link_check_interval = 1000,
};

rm_freertos_plus_tcp_instance_t g_freertos_plus_tcp0 =
{
    .p_ctrl = &g_freertos_plus_tcp0_ctrl,
    .p_cfg  = &g_freertos_plus_tcp0_cfg
};
#include "ethosu_driver.h"
            struct ethosu_driver g_ethosu0;
rm_ethosu_instance_ctrl_t g_rm_ethosu0_ctrl =
        {
            .p_dev = &g_ethosu0,
        };

        const rm_ethosu_cfg_t g_rm_ethosu0_cfg =
        {
#if defined(VECTOR_NUMBER_NPU_IRQ)
            .irq             = VECTOR_NUMBER_NPU_IRQ,
#else
            .irq             = FSP_INVALID_VECTOR,
#endif
            .ipl = (12),
            .p_callback       = NULL,
            .p_context        = NULL,
        };

        const rm_ethosu_instance_t g_rm_ethosu0 =
        {
            .p_ctrl = &g_rm_ethosu0_ctrl,
            .p_cfg  = &g_rm_ethosu0_cfg,
            .p_api  = &g_rm_ethosu_on_npu,
        };
/* MIPI PHY Macros */
        #define MIPI_PHY_CLKSTPT   ((uint16_t)1183)
        #define MIPI_PHY_CLKBFHT   ((uint8_t)10 + 1)
        #define MIPI_PHY_CLKKPT    ((uint8_t)22 + 4)
        #define MIPI_PHY_GOLPBKT   ((uint16_t)40)

        #define MIPI_PHY_TINIT     (74999)
        #define MIPI_PHY_TCLKPREP  (9)
        #define MIPI_PHY_TCLKSETT  (62)
        #define MIPI_PHY_TCLKMISS  (37)
        #define MIPI_PHY_THSPREP   (6)
        #define MIPI_PHY_THSETT    (24)
        #define MIPI_PHY_TCLKTRAIL (7)
        #define MIPI_PHY_TCLKPOST  (20)
        #define MIPI_PHY_TCLKPRE   (1)
        #define MIPI_PHY_TCLKZERO  (28)
        #define MIPI_PHY_THSEXIT   (12)
        #define MIPI_PHY_THSTRAIL  (8)
        #define MIPI_PHY_THSZERO   (19)
        #define MIPI_PHY_TLPEXIT   (7)


        /* MIPI PHY Structures */
        const mipi_phy_timing_t g_mipi_phy0_timing =
        {
            .t_init                   = 0x3FFFF & (uint32_t)MIPI_PHY_TINIT,
            .dphytim2_b.t_clk_prep    = (uint32_t)MIPI_PHY_TCLKPREP,
            .dphytim2_b.t_clk_settle  = (uint32_t)MIPI_PHY_TCLKSETT,
            .dphytim2_b.t_clk_miss    = (uint32_t)MIPI_PHY_TCLKMISS,
            .dphytim3_b.t_hs_prep     = (uint32_t)MIPI_PHY_THSPREP,
            .dphytim3_b.t_hs_sett     = (uint32_t)MIPI_PHY_THSETT,
            .dphytim4_b.t_clk_trail   = (uint32_t)MIPI_PHY_TCLKTRAIL,
            .dphytim4_b.t_clk_post    = (uint32_t)MIPI_PHY_TCLKPOST,
            .dphytim4_b.t_clk_pre     = (uint32_t)MIPI_PHY_TCLKPRE,
            .dphytim4_b.t_clk_zero    = (uint32_t)MIPI_PHY_TCLKZERO,
            .dphytim5_b.t_hs_exit     = (uint32_t)MIPI_PHY_THSEXIT,
            .dphytim5_b.t_hs_trail    = (uint32_t)MIPI_PHY_THSTRAIL,
            .dphytim5_b.t_hs_zero     = (uint32_t)MIPI_PHY_THSZERO,
            .t_lp_exit                = (uint32_t)MIPI_PHY_TLPEXIT,
        };

        mipi_phy_ctrl_t g_mipi_phy0_ctrl;
        const mipi_phy_cfg_t g_mipi_phy0_cfg =
        {
            .pll_settings = /* Calculated MIPI PHY PLL frequency: 1000000000 Hz (error 0.00%) = (24000000 Hz / 3) * 125.00 / 1 */{.div      = 3 - 1,.pll_div  = 0, .mul_int  = 125 - 1,.mul_frac = 0 /* Value: 0 */},
            .lp_divisor   = 5 - 1,
            .p_timing     = &g_mipi_phy0_timing,
            .dsi_mode     = (0),
        };
        /* Instance structure to use this module. */
        const mipi_phy_instance_t g_mipi_phy0 =
        {
            .p_ctrl       = &g_mipi_phy0_ctrl,
            .p_cfg        = &g_mipi_phy0_cfg,
            .p_api        = &g_mipi_phy
        };
mipi_csi_instance_ctrl_t g_mipi_csi0_ctrl;

        const mipi_csi_cfg_t g_mipi_csi0_cfg =
        {
            .p_mipi_phy_instance      = &g_mipi_phy0,

            .ctrl_data.control_0_bits.lane_count                    = 2,
            .ctrl_data.control_0_bits.zero_length_packet_output     = false,
            .ctrl_data.control_0_bits.err_frame_notify              = 1,
            .ctrl_data.control_0_bits.reserved_packet_reception     = 1,
            .ctrl_data.control_0_bits.generic_rule_mode             = 1,
            .ctrl_data.control_0_bits.ecc_check_24_bits             = 1,
            .ctrl_data.control_0_bits.descramble_enable             = 0,

            .ctrl_data.control_2_bits.frrclk                        = 10,
            .ctrl_data.control_2_bits.frrskw                        = 10,

            .option_data.data_type_enable                           = (mipi_csi_rx_data_enable_t)(MIPI_CSI_RX_DATA_ENABLE_YUV422_8_BIT |  0x0),

            .interrupt_cfg.receive_cfg.ipl                          = (12),

            #if defined(VECTOR_NUMBER_MIPICSI_RX)
            .interrupt_cfg.receive_cfg.irq                          = VECTOR_NUMBER_MIPICSI_RX,
            #else
            .interrupt_cfg.receive_cfg.irq                          = FSP_INVALID_VECTOR
            #endif

            .interrupt_cfg.data_lane_cfg.ipl                        = (12),
            #if defined(VECTOR_NUMBER_MIPICSI_DL)
            .interrupt_cfg.data_lane_cfg.irq                        = VECTOR_NUMBER_MIPICSI_DL,
            #else
            .interrupt_cfg.data_lane_cfg.irq                        = FSP_INVALID_VECTOR,
            #endif

            .interrupt_cfg.virtual_channel_cfg.ipl                  = (12),
            #if defined(VECTOR_NUMBER_MIPICSI_VC)
            .interrupt_cfg.virtual_channel_cfg.irq                  = VECTOR_NUMBER_MIPICSI_VC,
            #else
            .interrupt_cfg.virtual_channel_cfg.irq                  = FSP_INVALID_VECTOR,
            #endif

            .interrupt_cfg.power_management_cfg.ipl                 = (12),
            #if defined(VECTOR_NUMBER_MIPICSI_PM)
            .interrupt_cfg.power_management_cfg.irq                 = VECTOR_NUMBER_MIPICSI_PM,
            #else
            .interrupt_cfg.power_management_cfg.irq                 = FSP_INVALID_VECTOR,
            #endif

            .interrupt_cfg.short_packet_cfg.ipl                     = (12),
            #if defined(VECTOR_NUMBER_MIPICSI_GST)
            .interrupt_cfg.short_packet_cfg.irq                     = VECTOR_NUMBER_MIPICSI_GST,
            #else
            .interrupt_cfg.short_packet_cfg.irq                     = FSP_INVALID_VECTOR,
            #endif

            .interrupt_cfg.receive_enable_mask                      = ( 0x0),
            .interrupt_cfg.data_lane_enable_mask[0]                 = (R_MIPI_CSI_DLIE0_RULE_Msk | R_MIPI_CSI_DLIE0_EULE_Msk | R_MIPI_CSI_DLIE0_EESE_Msk | R_MIPI_CSI_DLIE0_ECTE_Msk |  0x0),
            .interrupt_cfg.data_lane_enable_mask[1]                 = (R_MIPI_CSI_DLIE0_RULE_Msk | R_MIPI_CSI_DLIE0_EULE_Msk | R_MIPI_CSI_DLIE0_EESE_Msk | R_MIPI_CSI_DLIE0_ECTE_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[0]           = ( 0x0),
            .interrupt_cfg.virtual_channel_enable_mask[1]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[2]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[3]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[4]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[5]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[6]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[7]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[8]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[9]           = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[10]          = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[11]          = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[12]          = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[13]          = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[14]          = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.virtual_channel_enable_mask[15]          = (R_MIPI_CSI_VCST0_MLF_Msk | R_MIPI_CSI_VCST0_ECD_Msk | R_MIPI_CSI_VCST0_CRC_Msk | R_MIPI_CSI_VCST0_IDE_Msk | R_MIPI_CSI_VCST0_WCE_Msk | R_MIPI_CSI_VCST0_ECC_Msk | R_MIPI_CSI_VCST0_FRS_Msk | R_MIPI_CSI_VCST0_FRD_Msk | R_MIPI_CSI_VCST0_OVF_Msk |  0x0),
            .interrupt_cfg.power_management_enable_mask             = ( 0x0),
            .interrupt_cfg.short_packet_enable_mask                 = (R_MIPI_CSI_GSIE_GOVE_Msk |  0x0),

            .p_callback         = mipi_csi0_callback,
        /* If NULL then do not add & */
        #if defined(NULL)
            .p_context          = NULL,
        #else
            .p_context          = &NULL,
        #endif
            .p_extend           = NULL,
        };

        /* Instance structure to use this module. */
        const mipi_csi_instance_t g_mipi_csi0 =
        {
            .p_ctrl        = &g_mipi_csi0_ctrl,
            .p_cfg         = &g_mipi_csi0_cfg,
            .p_api         = &g_mipi_csi
        };
uint8_t vin_image_buffer_1[VIN_BYTES_PER_FRAME] BSP_ALIGN_VARIABLE(128) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
uint8_t vin_image_buffer_2[VIN_BYTES_PER_FRAME] BSP_ALIGN_VARIABLE(128) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
uint8_t vin_image_buffer_3[VIN_BYTES_PER_FRAME] BSP_ALIGN_VARIABLE(128) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");


        vin_instance_ctrl_t g_vin0_ctrl;

        const vin_extended_cfg_t g_vin0_cfg_extend =
        {
            .p_mipi_csi_instance      = &g_mipi_csi0,

            .input_ctrl.cfg_bits.color_space_convert_bypass      = 0,
            .input_ctrl.cfg_bits.interlace_mode                  = VIN_INTERLACE_MODE_ODD_EVEN_FIELD_CAPTURE,
            .input_ctrl.cfg_bits.big_endian                      = 0,
            .input_ctrl.cfg_bits.dithering_mode                  = VIN_DITHERING_MODE_WITH_ADDITION,
            .input_ctrl.cfg_bits.input_mode                      = VIN_INPUT_FORMAT_YCBCR422_8_BIT,
            .input_ctrl.cfg_bits.lut_enable                      = 0,
            .input_ctrl.cfg_bits.dithering_direction             = false,
            .input_ctrl.cfg_bits.yuv444_conversion               = VIN_YUV444_CONVERSION_MODE_DATA_EXTEND,
            .input_ctrl.cfg_bits.scaling_enable                  = false,
            .input_ctrl.cfg_bits.pixel_data_clipping             = VIN_PIXEL_DATA_CLIPPING_DEFAULT,

            .input_ctrl.preclip.line_start                       = 0,
            .input_ctrl.preclip.line_end                         = 479,
            .input_ctrl.preclip.pixel_start                      = 0,
            .input_ctrl.preclip.pixel_end                        = 639,

            .input_ctrl.csi_mode_bits.virtual_channel            = 0,
            .input_ctrl.csi_mode_bits.data_type                  = VIN_DATA_TYPE_YUV422_8_BIT,
            .input_ctrl.csi_mode_bits.sign_extend_disable        = 1,

            .input_ctrl.csi_detect_bits.field_detect_enable      = 1,
            .input_ctrl.csi_detect_bits.even_field_detect_enable = 1,
            .input_ctrl.csi_detect_bits.even_field_number        = 0,

            .input_ctrl.image_stride                             = VIN_CFG_IMAGE_STRIDE,

            .output_ctrl.image_buffer                            = {vin_image_buffer_1, vin_image_buffer_2, vin_image_buffer_3},
            .output_ctrl.use_runtime_buffer                      = 0,
            .conversion_ctrl.data_mode_bits.data_conversion_mode      = VIN_CONVERSION_MODE_NONE,
            .conversion_ctrl.data_mode_bits.alpha_bit_value           = 1,
            .conversion_ctrl.data_mode_bits.output_data_byte_swap     = 1,
            .conversion_ctrl.data_mode_bits.extend_rgb_converted_data = 0,
            .conversion_ctrl.data_mode_bits.yc_data_transform_enable  = 0,
            .conversion_ctrl.data_mode_bits.yc_transform_mode         = VIN_YC_TRANSFORM_MODE_Y_CBCR,
            .conversion_ctrl.data_mode_bits.rgb8888_alpha_value       = 0xAA,

            .conversion_data.uv_address                          = 0x0,

            .conversion_data.yc_rgb_conversion_setting_1_bits.y_mul              = 4767,
            .conversion_data.yc_rgb_conversion_setting_1_bits.round_down_disable = 0,
            .conversion_data.yc_rgb_conversion_setting_2_bits.csub2              = 2048,
            .conversion_data.yc_rgb_conversion_setting_2_bits.ysub2              = 256,
            .conversion_data.yc_rgb_conversion_setting_3_bits.cgrmul2            = 3330,
            .conversion_data.yc_rgb_conversion_setting_3_bits.rcrmul2            = 6537,
            .conversion_data.yc_rgb_conversion_setting_4_bits.gcbmul2            = 1605,
            .conversion_data.yc_rgb_conversion_setting_4_bits.bcbmul2            = 8261,

            .conversion_data.uds_ctrl_bits.ne_bcb                = 0,
            .conversion_data.uds_ctrl_bits.ne_gy                 = 0,
            .conversion_data.uds_ctrl_bits.ne_rcr                = 0,
            .conversion_data.uds_ctrl_bits.pixel_interpolation   = 0,
            .conversion_data.uds_ctrl_bits.bilinear_advanced     = 1,
            .conversion_data.uds_ctrl_bits.scale_up_pixel_count  = 0,

            .conversion_data.uds_scale_bits.vertical_mask        = 4096,
            .conversion_data.uds_scale_bits.horizontal_mask      = 4096,

            .conversion_data.uds_bwidth_bits.bwidth_v       = 64,
            .conversion_data.uds_bwidth_bits.bwidth_h       = 64,

            .conversion_data.uds_clipping_bits.cl_vsize          = 480,
            .conversion_data.uds_clipping_bits.cl_hsize          = 640,

            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_1_bits.lrp             = 263,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_2_bits.lgp             = 516,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_2_bits.lbp             = 100,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_3_bits.lap             = 256,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_3_bits.lhen            = 0,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_3_bits.lsft            = 10,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_3_bits.persistent_bit0 = 1,
            .conversion_data.rgb_to_yuv_conversion_settings[0].setting_3_bits.persistent_bit1 = 1,

            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_1_bits.lrp             = -152,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_2_bits.lgp             = -298,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_2_bits.lbp             = 450,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_3_bits.lap             = 2048,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_3_bits.lhen            = 0,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_3_bits.lsft            = 10,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_3_bits.persistent_bit0 = 1,
            .conversion_data.rgb_to_yuv_conversion_settings[1].setting_3_bits.persistent_bit1 = 1,

            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_1_bits.lrp             = 450,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_2_bits.lgp             = -377,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_2_bits.lbp             = -73,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_3_bits.lap             = 2048,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_3_bits.lhen            = 0,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_3_bits.lsft            = 10,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_3_bits.persistent_bit0 = 1,
            .conversion_data.rgb_to_yuv_conversion_settings[2].setting_3_bits.persistent_bit1 = 1,

            .interrupt_cfg.status_enable_mask     = (R_VIN_IE_FME_Msk |  0x0),
            .interrupt_cfg.scanline_compare_value = 0,

            .interrupt_cfg.status.ipl = (10),
            #if defined(VECTOR_NUMBER_VIN_IRQ)
            .interrupt_cfg.status.irq                    = VECTOR_NUMBER_VIN_IRQ,
            #else
            .interrupt_cfg.status.irq                    = FSP_INVALID_VECTOR,
            #endif

            .interrupt_cfg.error.ipl  = (10),
            #if defined(VECTOR_NUMBER_VIN_ERR)
            .interrupt_cfg.error.irq                    = VECTOR_NUMBER_VIN_ERR,
            #else
            .interrupt_cfg.error.irq                    = FSP_INVALID_VECTOR,
            #endif
        };

        const capture_cfg_t g_vin0_cfg =
        {
            .x_capture_start_pixel   = 0xFFFF,   // Not used. See instance extended configuration
            .x_capture_pixels        = 0xFFFF,   // Not used. See instance extended configuration
            .y_capture_start_pixel   = 0xFFFF,   // Not used. See instance extended configuration
            .y_capture_pixels        = 0xFFFF,   // Not used. See instance extended configuration
            .bytes_per_pixel         = 0xFF,     // Not used. See instance extended configuration

            .p_callback         = vin0_callback,
        /* If NULL then do not add & */
        #if defined(NULL)
            .p_context          = NULL,
        #else
            .p_context          = &NULL,
        #endif
            .p_extend           = &g_vin0_cfg_extend,

        };

        /* Instance structure to use this module. */
        const capture_instance_t g_vin0 =
        {
            .p_ctrl        = &g_vin0_ctrl,
            .p_cfg         = &g_vin0_cfg,
            .p_api         = &g_capture_on_vin
        };
const uint8_t DRW_INT_IPL = (2);
            d2_device *   d2_handle;
/** Display framebuffer */
        #if GLCDC_CFG_LAYER_1_ENABLE
        uint8_t fb_background[1][DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 * DISPLAY_VSIZE_INPUT0] BSP_ALIGN_VARIABLE(64) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
        #else
        /** Graphics Layer 1 is specified not to be used when starting */
        #endif
        #if GLCDC_CFG_LAYER_2_ENABLE
        uint8_t fb_foreground[1][DISPLAY_BUFFER_STRIDE_BYTES_INPUT1 * DISPLAY_VSIZE_INPUT1] BSP_ALIGN_VARIABLE(64) BSP_PLACE_IN_SECTION(BSP_UNINIT_SECTION_PREFIX ".sdram_noinit");
        #else
        /** Graphics Layer 2 is specified not to be used when starting */
        #endif

        #if GLCDC_CFG_CLUT_ENABLE
        /** Display CLUT buffer to be used for updating CLUT */
        static uint32_t CLUT_buffer[256];

        /** Display CLUT configuration(only used if using CLUT format) */
        display_clut_cfg_t g_plcd_display_clut_cfg_glcdc =
        {
            .p_base              = (uint32_t *)CLUT_buffer,
            .start               = 0,   /* User have to update this setting when using */
            .size                = 256  /* User have to update this setting when using */
        };
        #else
        /** CLUT is specified not to be used */
        #endif

        #if (false)
         #define GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST   const
         #define GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST    (uint16_t *)
         #define GLCDC_CFG_CORRECTION_GAMMA_CFG_CAST      (display_gamma_correction_t *)
        #else
         #define GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST
         #define GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST
         #define GLCDC_CFG_CORRECTION_GAMMA_CFG_CAST
        #endif

        #if ((GLCDC_CFG_CORRECTION_GAMMA_ENABLE_R | GLCDC_CFG_CORRECTION_GAMMA_ENABLE_G | GLCDC_CFG_CORRECTION_GAMMA_ENABLE_B) && GLCDC_CFG_COLOR_CORRECTION_ENABLE)
        /** Gamma correction tables */
        #if GLCDC_CFG_CORRECTION_GAMMA_ENABLE_R
        static GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST uint16_t glcdc_gamma_r_gain[DISPLAY_GAMMA_CURVE_ELEMENT_NUM] = {1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024};
        static GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST uint16_t glcdc_gamma_r_threshold[DISPLAY_GAMMA_CURVE_ELEMENT_NUM] = {0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960};
        #endif
        #if GLCDC_CFG_CORRECTION_GAMMA_ENABLE_G
        static GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST uint16_t glcdc_gamma_g_gain[DISPLAY_GAMMA_CURVE_ELEMENT_NUM] = {1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024};
        static GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST uint16_t glcdc_gamma_g_threshold[DISPLAY_GAMMA_CURVE_ELEMENT_NUM] = {0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960};
        #endif
        #if GLCDC_CFG_CORRECTION_GAMMA_ENABLE_B
        static GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST uint16_t glcdc_gamma_b_gain[DISPLAY_GAMMA_CURVE_ELEMENT_NUM] = {1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024, 1024};
        static GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST uint16_t glcdc_gamma_b_threshold[DISPLAY_GAMMA_CURVE_ELEMENT_NUM] = {0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960};
        #endif
        GLCDC_CFG_CORRECTION_GAMMA_TABLE_CONST display_gamma_correction_t g_plcd_display_gamma_cfg =
        {
            .r =
            {
                .enable      = GLCDC_CFG_CORRECTION_GAMMA_ENABLE_R,
        #if GLCDC_CFG_CORRECTION_GAMMA_ENABLE_R
                .gain        = GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST glcdc_gamma_r_gain,
                .threshold   = GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST glcdc_gamma_r_threshold
        #else
                .gain        = NULL,
                .threshold   = NULL
        #endif
            },
            .g =
            {
                .enable      = GLCDC_CFG_CORRECTION_GAMMA_ENABLE_G,
        #if GLCDC_CFG_CORRECTION_GAMMA_ENABLE_G
                .gain        = GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST glcdc_gamma_g_gain,
                .threshold   = GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST glcdc_gamma_g_threshold
        #else
                .gain        = NULL,
                .threshold   = NULL
        #endif
            },
            .b =
            {
                .enable      = GLCDC_CFG_CORRECTION_GAMMA_ENABLE_B,
        #if GLCDC_CFG_CORRECTION_GAMMA_ENABLE_B
                .gain        = GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST glcdc_gamma_b_gain,
                .threshold   = GLCDC_CFG_CORRECTION_GAMMA_TABLE_CAST glcdc_gamma_b_threshold
        #else
                .gain        = NULL,
                .threshold   = NULL
        #endif
            }
        };
        #endif

        #define RA_NOT_DEFINED (1)
        #if (RA_NOT_DEFINED != RA_NOT_DEFINED)
          const mipi_dsi_instance_t RA_NOT_DEFINED;
        #endif
        /** Display device extended configuration */
        const glcdc_extended_cfg_t g_plcd_display_extend_cfg =
        {
            .tcon_hsync            = GLCDC_TCON_PIN_1,
            .tcon_vsync            = GLCDC_TCON_PIN_0,
            .tcon_de               = GLCDC_TCON_PIN_2,
            .correction_proc_order = GLCDC_CORRECTION_PROC_ORDER_GAMMA2BRIGHTNESS_CONTRAST,
            .clksrc                = GLCDC_CLK_SRC_INTERNAL,
            .clock_div_ratio       = GLCDC_PANEL_CLK_DIVISOR_4,
            .dithering_mode        = GLCDC_DITHERING_MODE_TRUNCATE,
            .dithering_pattern_A   = GLCDC_DITHERING_PATTERN_11,
            .dithering_pattern_B   = GLCDC_DITHERING_PATTERN_11,
            .dithering_pattern_C   = GLCDC_DITHERING_PATTERN_11,
            .dithering_pattern_D   = GLCDC_DITHERING_PATTERN_11,
        #if (RA_NOT_DEFINED != RA_NOT_DEFINED)
            .phy_layer             = (void*)&RA_NOT_DEFINED
        #else
            .phy_layer             = NULL
        #endif
        };
        #undef RA_NOT_DEFINED

        /** Display control block instance */
        glcdc_instance_ctrl_t g_plcd_display_ctrl;

        /** Display interface configuration */
        const display_cfg_t g_plcd_display_cfg =
        {
            /** Input1(Graphics1 layer) configuration */
            .input[0] =
            {
                #if GLCDC_CFG_LAYER_1_ENABLE
                .p_base              = (uint32_t *)&fb_background[0],
                #else
                .p_base              = NULL,
                #endif
                .hsize               = DISPLAY_HSIZE_INPUT0,
                .vsize               = DISPLAY_VSIZE_INPUT0,
                .hstride             = DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0,
                .format              = DISPLAY_IN_FORMAT_16BITS_RGB565,
                .line_descending_enable = false,
                .lines_repeat_enable = false,
                .lines_repeat_times  = 0
            },

            /** Input2(Graphics2 layer) configuration */
            .input[1] =
            {
                #if GLCDC_CFG_LAYER_2_ENABLE
                .p_base              = (uint32_t *)&fb_foreground[0],
                #else
                .p_base              = NULL,
                #endif
                .hsize               = DISPLAY_HSIZE_INPUT1,
                .vsize               = DISPLAY_VSIZE_INPUT1,
                .hstride             = DISPLAY_BUFFER_STRIDE_PIXELS_INPUT1,
                .format              = DISPLAY_IN_FORMAT_16BITS_RGB565,
                .line_descending_enable = false,
                .lines_repeat_enable = false,
                .lines_repeat_times  = 0
             },

            /** Input1(Graphics1 layer) layer configuration */
            .layer[0] =
            {
                .coordinate = {
                        .x           = 35,
                        .y           = 65
                },
                .fade_control        = DISPLAY_FADE_CONTROL_NONE,
                .fade_speed          = 255
            },

            /** Input2(Graphics2 layer) layer configuration */
            .layer[1] =
            {
                .coordinate = {
                        .x           = 700,
                        .y           = 0
                },
                .fade_control        = DISPLAY_FADE_CONTROL_NONE,
                .fade_speed          = 255
            },

            /** Output configuration */
            .output =
            {
                .htiming =
                {
                    .total_cyc       = 1334,
                    .display_cyc     = 1024,
                    .back_porch      = 300,
                    .sync_width       = 10,
                    .sync_polarity   = DISPLAY_SIGNAL_POLARITY_LOACTIVE
                },
                .vtiming =
                {
                    .total_cyc       = 780,
                    .display_cyc     = 600,
                    .back_porch      = 30,
                    .sync_width       = 5,
                    .sync_polarity   = DISPLAY_SIGNAL_POLARITY_LOACTIVE
                },
                .format              = DISPLAY_OUT_FORMAT_24BITS_RGB888,
                .endian              = DISPLAY_ENDIAN_LITTLE,
                .color_order         = DISPLAY_COLOR_ORDER_RGB,
                .data_enable_polarity = DISPLAY_SIGNAL_POLARITY_HIACTIVE,
                .sync_edge           = DISPLAY_SIGNAL_SYNC_EDGE_RISING,
                .bg_color =
                {
                    .byte = {
                        .a           = 255,
                        .r           = 0,
                        .g           = 0,
                        .b           = 0
                    }
                },
#if (GLCDC_CFG_COLOR_CORRECTION_ENABLE)
                .brightness =
                {
                    .enable          = false,
                    .r               = 512,
                    .g               = 512,
                    .b               = 512
                },
                .contrast =
                {
                    .enable          = false,
                    .r               = 128,
                    .g               = 128,
                    .b               = 128
                },
#if (GLCDC_CFG_CORRECTION_GAMMA_ENABLE_R | GLCDC_CFG_CORRECTION_GAMMA_ENABLE_G | GLCDC_CFG_CORRECTION_GAMMA_ENABLE_B)
 #if false
                .p_gamma_correction  = GLCDC_CFG_CORRECTION_GAMMA_CFG_CAST (&g_plcd_display_gamma_cfg),
 #else
                .p_gamma_correction  = &g_plcd_display_gamma_cfg,
 #endif
#else
                .p_gamma_correction  = NULL,
#endif
#endif
                .dithering_on        = false
            },

            /** Display device callback function pointer */
            .p_callback              = glcdc_vsync_isr,
            .p_context               = NULL,

            /** Display device extended configuration */
            .p_extend                = (void *)(&g_plcd_display_extend_cfg),

            .line_detect_ipl        = (12),
            .underflow_1_ipl        = (12),
            .underflow_2_ipl        = (12),

#if defined(VECTOR_NUMBER_GLCDC_LINE_DETECT)
            .line_detect_irq        = VECTOR_NUMBER_GLCDC_LINE_DETECT,
#else
            .line_detect_irq        = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GLCDC_UNDERFLOW_1)
            .underflow_1_irq        = VECTOR_NUMBER_GLCDC_UNDERFLOW_1,
#else
            .underflow_1_irq        = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GLCDC_UNDERFLOW_2)
            .underflow_2_irq        = VECTOR_NUMBER_GLCDC_UNDERFLOW_2,
#else
            .underflow_2_irq        = FSP_INVALID_VECTOR,
#endif
        };

#if GLCDC_CFG_LAYER_1_ENABLE
        /** Display on GLCDC run-time configuration(for the graphics1 layer) */
        display_runtime_cfg_t g_plcd_display_runtime_cfg_bg =
        {
            .input =
            {
                #if (true)
                .p_base              = (uint32_t *)&fb_background[0],
                #else
                .p_base              = NULL,
                #endif
                .hsize               = DISPLAY_HSIZE_INPUT0,
                .vsize               = DISPLAY_VSIZE_INPUT0,
                .hstride             = DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0,
                .format              = DISPLAY_IN_FORMAT_16BITS_RGB565,
                .line_descending_enable = false,
                .lines_repeat_enable = false,
                .lines_repeat_times  = 0
            },
            .layer =
            {
                .coordinate = {
                        .x           = 35,
                        .y           = 65
                },
                .fade_control        = DISPLAY_FADE_CONTROL_NONE,
                .fade_speed          = 255
            }
        };
#endif
#if GLCDC_CFG_LAYER_2_ENABLE
        /** Display on GLCDC run-time configuration(for the graphics2 layer) */
        display_runtime_cfg_t g_plcd_display_runtime_cfg_fg =
        {
            .input =
            {
                #if (true)
                .p_base              = (uint32_t *)&fb_foreground[0],
                #else
                .p_base              = NULL,
                #endif
                .hsize               = DISPLAY_HSIZE_INPUT1,
                .vsize               = DISPLAY_VSIZE_INPUT1,
                .hstride             = DISPLAY_BUFFER_STRIDE_PIXELS_INPUT1,
                .format              = DISPLAY_IN_FORMAT_16BITS_RGB565,
                .line_descending_enable = false,
                .lines_repeat_enable = false,
                .lines_repeat_times  = 0
             },
            .layer =
            {
                .coordinate = {
                        .x           = 700,
                        .y           = 0
                },
                .fade_control        = DISPLAY_FADE_CONTROL_NONE,
                .fade_speed          = 255
            }
        };
#endif

/* Instance structure to use this module. */
const display_instance_t g_plcd_display =
{
    .p_ctrl        = &g_plcd_display_ctrl,
    .p_cfg         = (display_cfg_t *)&g_plcd_display_cfg,
    .p_api         = (display_api_t *)&g_display_on_glcdc
};
ioport_instance_ctrl_t g_ioport_ctrl;
const ioport_instance_t g_ioport =
        {
            .p_api = &g_ioport_on_ioport,
            .p_ctrl = &g_ioport_ctrl,
            .p_cfg = &g_bsp_pin_cfg,
        };

EventGroupHandle_t g_ai_app_event;
                #if 1
                StaticEventGroup_t g_ai_app_event_memory;
                #endif
                void rtos_startup_err_callback(void * p_instance, void * p_data);
void g_common_init(void) {
g_ai_app_event =
                #if 1
                xEventGroupCreateStatic(&g_ai_app_event_memory);
                #else
                xEventGroupCreate();
                #endif
                if (NULL == g_ai_app_event) {
                    rtos_startup_err_callback(g_ai_app_event, 0);
                }
}
