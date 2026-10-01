/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**********************************************************************************************************************
 * File Name    : console_output.h
 * Description  : .
 *********************************************************************************************************************/

#ifndef CONSOLE_OUTPUT_H_
#define CONSOLE_OUTPUT_H_

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "hal_data.h"
#include "common_util.h"
#include "stdio.h"

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define BUFFER_LINE_LENGTH (1024)
#define CONSOLE_OUTPUT_TYPE (1) // 0: Output to SEGGER RTT Viewer, 1: Output to UART ports
/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported global variables
 **********************************************************************************************************************/
extern char sprintf_buffer[];
extern fsp_err_t console_output_init (void);
extern fsp_err_t print_to_console(char * p_data);
extern int8_t input_from_console (void);

/* Read one received character from the interrupt-fed ring buffer.
 * Returns the character, or -1 after timeout_ms with nothing received. */
extern int console_read_char(uint32_t timeout_ms);

/*
 * Gate for the periodic status chatter (the processing report and the
 * per-detection line). Both used to print on every pass of their loop, which
 * buried anything typed at the provisioning CLI - including a pasted PEM.
 *
 * Returns true when the caller should print: not silenced, nobody typing, and
 * at least period_ms since that caller last printed. *p_last is the caller's
 * own timestamp and is updated when this returns true.
 */
extern bool console_report_due(uint32_t *p_last, uint32_t period_ms);

/* Silence the periodic chatter entirely ('quiet' on the serial CLI). */
extern void console_quiet_set(bool quiet);
extern bool console_quiet_get(void);

#endif /* CONSOLE_OUTPUT_H_ */
