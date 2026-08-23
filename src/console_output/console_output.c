/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**********************************************************************************************************************
 * File Name    : console_output.c
 * Description  : This file defines the jlink console implementations.
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "hal_data.h"
#include "common_data.h"

#include <stdio.h>
#include <string.h>

#include "common_util.h"
#include "console_output.h"
#include "FreeRTOS.h"
#include "semphr.h"


/***************************************************************************************************************************
 * Macro definitions
 ***************************************************************************************************************************/


/***************************************************************************************************************************
 * Typedef definitions
 ***************************************************************************************************************************/
/***************************************************************************************************************************
 * Imported global variables and functions (from other files)
 ***************************************************************************************************************************/
/***************************************************************************************************************************
 * Exported global variables and functions (to be accessed by other files)
 ***************************************************************************************************************************/
char sprintf_buffer[BUFFER_LINE_LENGTH] = {};
/***************************************************************************************************************************
 * Private global variables and functions
 ***************************************************************************************************************************/
static uint8_t s_rx_buf;

/* Interrupt-fed receive ring for the provisioning CLI. Sized so a pasted
 * PEM block at 230400 baud cannot overrun between reader wakeups. */
#define CONSOLE_RX_RING_SIZE 1024
static volatile uint8_t  s_rx_ring[CONSOLE_RX_RING_SIZE];
static volatile uint32_t s_rx_head;
static volatile uint32_t s_rx_tail;

static volatile uint32_t g_transfer_complete = 0;
static volatile uint32_t g_receive_complete  = 0;


static fsp_err_t console_output_write(const char *buffer);
static bool key_pressed(void);
static uint8_t get_detected_key(void);

/*********************************************************************************************************************
 *  Initialize the SCI UART
 *  @param[IN]   None
 *  @retval      None
***********************************************************************************************************************/

fsp_err_t console_output_init (void)
{
    fsp_err_t fsp_err = FSP_SUCCESS;


    fsp_err = g_console_output_uart.p_api->open(g_console_output_uart.p_ctrl, g_console_output_uart.p_cfg);


    return fsp_err;
}

/*********************************************************************************************************************
 *  Global API: Print a string buffer to Jlink console
 *  @param[IN]   None
 *  @retval      return success
***********************************************************************************************************************/
fsp_err_t print_to_console(char * p_data)
{
    fsp_err_t err = FSP_SUCCESS;
    vision_ai_app_err_t vision_ai_status = VISION_AI_APP_SUCCESS;

    err = console_output_write(p_data);
    if(FSP_SUCCESS != err)
    {
    	handle_error(VISION_AI_APP_ERR_CONSOLE_WRITE);
    	return VISION_AI_APP_ERR_CONSOLE_WRITE;
    }

    return vision_ai_status;
}

/*********************************************************************************************************************
 *  Read user input from the Jlink console
 *  @param[IN]   None
 *  @retval      return input buffer
***********************************************************************************************************************/
int8_t input_from_console (void)
{
	fsp_err_t err = FSP_SUCCESS;

	s_rx_buf = 0;

	g_receive_complete = false;

	err = g_console_output_uart.p_api->read(g_console_output_uart.p_ctrl, &s_rx_buf, 1);
	if (FSP_SUCCESS != err)
    {
    	handle_error(VISION_AI_APP_ERR_CONSOLE_READ);
    }


    while(key_pressed() == false)
    {
        vTaskDelay(1);
    }

    return ((int8_t)get_detected_key());
}

/*********************************************************************************************************************
 *  Local function: write a string over the JLink console
 *  @param[IN]   buffer: string buffer
 *  @retval      None
***********************************************************************************************************************/
static fsp_err_t console_output_write(const char *buffer)
{
    fsp_err_t err = FSP_SUCCESS;

    /* Serialize writers: multiple tasks (vision, display, net, iotc) print;
     * a write() while the UART is busy would return FSP_ERR_IN_USE and trap. */
    static SemaphoreHandle_t s_console_mutex;
    static StaticSemaphore_t s_console_mutex_buf;
    if (NULL == s_console_mutex)
    {
        s_console_mutex = xSemaphoreCreateMutexStatic(&s_console_mutex_buf);
    }
    xSemaphoreTake(s_console_mutex, portMAX_DELAY);

    g_transfer_complete = false;

    err = g_console_output_uart.p_api->write(g_console_output_uart.p_ctrl, (uint8_t *)buffer, strlen(buffer));
    if (FSP_SUCCESS != err)
    {
        xSemaphoreGive(s_console_mutex);
        handle_error(VISION_AI_APP_ERR_CONSOLE_WRITE);
        return err;
    }

    /* Bounded wait: a lost TX-complete event must cost one line, not hang
     * every future print behind the mutex. */
    for (uint32_t i = 0; (i < 200) && !g_transfer_complete; i++)
    {
        vTaskDelay(1);
    }

    xSemaphoreGive(s_console_mutex);
    return err;
}

/*********************************************************************************************************************
 *  Read one received character from the interrupt-fed ring.
 *  @param[IN]   timeout_ms: how long to wait for a character
 *  @retval      the character (0..255), or -1 on timeout
***********************************************************************************************************************/
int console_read_char(uint32_t timeout_ms)
{
    uint32_t waited = 0;
    while (s_rx_tail == s_rx_head)
    {
        if (waited >= timeout_ms)
        {
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        waited += 10;
    }
    uint8_t c = s_rx_ring[s_rx_tail % CONSOLE_RX_RING_SIZE];
    s_rx_tail++;
    return (int) c;
}

/*********************************************************************************************************************
 *  Get key pressed
 *  @param[IN]   None
 *  @retval      uint8_t: key ascii
***********************************************************************************************************************/
uint8_t get_detected_key(void)
{

    return (s_rx_buf);

}


static bool key_pressed(void)
{

    return (g_receive_complete);

}


/*********************************************************************************************************************
 *  Console console callback
 *  @param[IN]   uart_callback_args_t *p_args: callback information
 *  @retval      None
***********************************************************************************************************************/
void console_output_uart_callback(uart_callback_args_t *p_args)
{
    /* Handle the UART event */
    switch (p_args->event)
    {
        /* Received a character: push into the CLI ring (drop on overflow) */
        case UART_EVENT_RX_CHAR:
        {
            if ((s_rx_head - s_rx_tail) < CONSOLE_RX_RING_SIZE)
            {
                s_rx_ring[s_rx_head % CONSOLE_RX_RING_SIZE] = (uint8_t) p_args->data;
                s_rx_head++;
            }
            break;
        }
        /* Receive complete */
        case UART_EVENT_RX_COMPLETE:
        {
            g_receive_complete = 1;
            break;
        }
        /* Transmit complete */
        case UART_EVENT_TX_COMPLETE:
        {
            g_transfer_complete = 1;
            break;
        }
        default:
        {
            /* Do nothing */
        }
    }
}

