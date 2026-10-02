/*
 * Copyright (c) 2026 Avnet, Inc.
 * SPDX-License-Identifier: MIT
 *
 * Serial provisioning CLI on the console UART. Lets a user set the
 * IOTCONNECT identity (env/cpid/duid + device certificate/key) at runtime;
 * values persist on LittleFS (OSPI flash) across power cycles and take
 * precedence over any compiled-in identity.
 *
 * Commands:
 *   help                    this text
 *   show                    stored configuration (key redacted)
 *   set env|cpid|duid <v>   store one identity value
 *   set cert                paste the device certificate PEM (multi-line)
 *   set key                 paste the device private key PEM (multi-line)
 *   apply                   connect using the stored configuration now
 *   erase                   remove the stored configuration
 *   reboot                  restart the device (warm reset; the LCD panel only
 *                           re-initialises from a cold start, so power-cycle to
 *                           bring the display back)
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"
#include "hal_data.h"

#include "console_output/console_output.h"
#include "iotc_config.h"

#define CLI_LINE_MAX 128
#define CLI_TASK_STACK 2048 /* words */

extern void iotc_app_request_restart(void); /* iotc_app.c: re-run credential load + connect */

static char s_pem[IOTC_CONFIG_PEM_MAX];

static void prv_print(const char *s)
{
    print_to_console((char *) s);
}

/*
 * Tick of the last character typed at the prompt. The periodic status chatter
 * checks this (console_report_due) and holds off while someone is interacting,
 * so typed input - and a pasted certificate above all - stays readable. Zero
 * means nothing has ever been typed, so a board nobody touches still reports.
 */
static volatile uint32_t s_last_input_tick;
#define CLI_INTERACTIVE_GRACE_MS (20000U)

static void prv_mark_input(void)
{
    uint32_t t = (uint32_t) xTaskGetTickCount();
    s_last_input_tick = (0U == t) ? 1U : t; /* 0 is reserved for "never" */
}

bool iotc_cli_is_interactive(void)
{
    uint32_t t = s_last_input_tick;

    if (0U == t)
    {
        return false;
    }
    return (((uint32_t) xTaskGetTickCount() - t) < pdMS_TO_TICKS(CLI_INTERACTIVE_GRACE_MS));
}

/* Read one line. With echo off (PEM capture), a complete "-----END ...-----"
 * line also ends the read, so a paste copied without a trailing newline still
 * finishes instead of leaving the END line pending. Returns the length. */
static int prv_read_line(char *buf, size_t size, bool echo)
{
    static bool s_last_was_cr = false;
    size_t n = 0;
    for (;;)
    {
        int c = console_read_char(0xFFFFFFFFu);
        prv_mark_input();
        /* A CR+LF terminal sends both for one Enter: drop the LF so it does not
         * read as an empty line (which would cancel a PEM capture). */
        bool after_cr = s_last_was_cr;
        s_last_was_cr = (c == '\r');
        if ((c == '\n') && after_cr)
        {
            continue;
        }
        if ((c == '\r') || (c == '\n'))
        {
            if (echo)
            {
                prv_print("\r\n");
            }
            buf[n] = '\0';
            return (int) n;
        }
        if ((c == 0x08) || (c == 0x7F)) /* backspace */
        {
            if (n > 0)
            {
                n--;
                if (echo)
                {
                    prv_print("\b \b");
                }
            }
            continue;
        }
        if ((c >= 0x20) && (c < 0x7F) && (n < size - 1))
        {
            buf[n++] = (char) c;
            if (echo)
            {
                char e[2] = {(char) c, 0};
                prv_print(e);
            }
            else if ((n >= 16) && (0 == strncmp(buf, "-----END", 8)) &&
                     (0 == strncmp(&buf[n - 5], "-----", 5)))
            {
                buf[n] = '\0';
                return (int) n;
            }
        }
    }
}

/*
 * Capture a pasted PEM block: lines are accumulated from the -----BEGIN line
 * until the -----END line. Anything before BEGIN is ignored (a paste of the
 * wrong selection), and a second BEGIN restarts the capture. A blank line
 * before any content cancels. Echo is off (pastes are long).
 */
static int prv_read_pem(const char *what, const char *label)
{
    char line[96];
    size_t used = 0;

    snprintf(s_pem, sizeof(s_pem), "%s", "");
    sprintf_buffer[0] = 0;
    snprintf(sprintf_buffer, BUFFER_LINE_LENGTH,
             "Paste the %s PEM now (ends automatically at the -----END line; "
             "an empty line or 'cancel' aborts):\r\n", what);
    prv_print(sprintf_buffer);

    bool seen_input = false;
    bool too_large = false;
    bool wrong_type = false;
    for (;;)
    {
        int n = prv_read_line(line, sizeof(line), false);
        if (n <= 0)
        {
            if (!seen_input)
            {
                prv_print("cancelled\r\n");
                return -1;
            }
            continue; /* tolerate blank lines inside a paste */
        }
        seen_input = true;
        if ((used == 0) && (0 == strcmp(line, "cancel")))
        {
            prv_print("cancelled\r\n");
            return -1;
        }
        if (0 == strncmp(line, "-----BEGIN", 10))
        {
            used = 0;
            too_large = false;
            /* A key pasted at "set cert" (or the reverse) is rejected rather
             * than stored in the wrong slot. */
            wrong_type = (NULL == strstr(line, label));
        }
        else if ((used == 0) && !wrong_type)
        {
            continue; /* not inside a PEM block yet */
        }
        if (wrong_type)
        {
            if (0 == strncmp(line, "-----END", 8))
            {
                snprintf(sprintf_buffer, BUFFER_LINE_LENGTH,
                         "error: that is not a %s PEM\r\n", what);
                prv_print(sprintf_buffer);
                return -1;
            }
            continue;
        }
        if (too_large || (used + (size_t) n + 2 >= sizeof(s_pem)))
        {
            /* Swallow the rest of the paste so its lines are not run as commands. */
            too_large = true;
            if (0 == strncmp(line, "-----END", 8))
            {
                prv_print("error: PEM too large\r\n");
                return -1;
            }
            continue;
        }
        memcpy(&s_pem[used], line, (size_t) n);
        used += (size_t) n;
        s_pem[used++] = '\n';
        s_pem[used] = '\0';
        if (0 == strncmp(line, "-----END", 8))
        {
            return (int) used;
        }
    }
}

static void prv_show(void)
{
    char buf[IOTC_CONFIG_PEM_MAX];
    static const char *const fields[] = {"env", "cpid", "duid"};
    for (size_t i = 0; i < 3; i++)
    {
        int n = iotc_config_read(fields[i], buf, sizeof(buf));
        snprintf(sprintf_buffer, BUFFER_LINE_LENGTH, "  %-5s: %s\r\n",
                 fields[i], (n > 0) ? buf : "(not set)");
        prv_print(sprintf_buffer);
    }
    int c = iotc_config_read("cert", buf, sizeof(buf));
    snprintf(sprintf_buffer, BUFFER_LINE_LENGTH, "  cert : %s\r\n",
             (c > 0) ? "stored" : "(not set)");
    prv_print(sprintf_buffer);
    int k = iotc_config_read("key", buf, sizeof(buf));
    snprintf(sprintf_buffer, BUFFER_LINE_LENGTH, "  key  : %s\r\n",
             (k > 0) ? "stored (redacted)" : "(not set)");
    prv_print(sprintf_buffer);
}

static void prv_help(void)
{
    prv_print("IOTCONNECT provisioning CLI\r\n"
              "  show                    stored configuration\r\n"
              "  set env <value>         e.g. set env poc\r\n"
              "  set cpid <value>        Company ID (Settings -> Key Vault)\r\n"
              "  set duid <value>        device unique ID\r\n"
              "  set cert                then paste the device certificate PEM\r\n"
              "  set key                 then paste the device private key PEM\r\n"
              "  apply                   connect with the stored configuration\r\n"
              "  erase                   remove the stored configuration\r\n"
              "  reboot                  restart the device (LCD needs a power cycle)\r\n"
              "  snapshot                capture + upload a snapshot now\r\n"
              "  brightness <0|1>        camera exposure: 0 normal, 1 bright\r\n"
              "  led-auto [0|1]          green LED follows detections (no arg toggles)\r\n"
              "  quiet [0|1]             silence the periodic status reports\r\n");
}

static void prv_handle(char *line)
{
    if (0 == strcmp(line, "help") || 0 == strcmp(line, "?"))
    {
        prv_help();
    }
    else if (0 == strcmp(line, "show"))
    {
        prv_show();
    }
    else if (0 == strcmp(line, "snapshot"))
    {
        /* Same deferred path the cloud command uses: the net thread runs
         * the capture + upload. Lets uploads be tested without a dashboard
         * (and while video is streaming, to check the heap headroom). */
        extern void iotc_app_request_snapshot(void);
        iotc_app_request_snapshot();
        prv_print("snapshot queued\r\n");
    }
    else if (0 == strncmp(line, "brightness ", 11))
    {
        extern void camera_set_brightness_level(uint8_t level);
        if ((line[11] == '0') || (line[11] == '1'))
        {
            camera_set_brightness_level((uint8_t) (line[11] - '0'));
            prv_print("ok\r\n");
        }
        else
        {
            prv_print("use: brightness 0 | brightness 1\r\n");
        }
    }
    else if (0 == strncmp(line, "led-auto", 8) &&
             ((line[8] == '\0') || (line[8] == ' ')))
    {
        extern void led_auto_set(bool on);
        extern bool led_auto_get(void);
        const char *arg = &line[8];
        bool on;

        while (' ' == *arg)
        {
            arg++;
        }
        if ('\0' == *arg)
        {
            on = !led_auto_get(); /* no argument: toggle */
        }
        else if (('1' == *arg) || (0 == strcmp(arg, "on")))
        {
            on = true;
        }
        else if (('0' == *arg) || (0 == strcmp(arg, "off")))
        {
            on = false;
        }
        else
        {
            prv_print("use: led-auto | led-auto 0 | led-auto 1\r\n");
            return;
        }
        led_auto_set(on);
        prv_print(on ? "detection LED enabled\r\n" : "detection LED disabled\r\n");
    }
    else if (0 == strncmp(line, "quiet", 5) &&
             ((line[5] == '\0') || (line[5] == ' ')))
    {
        const char *arg = &line[5];
        bool quiet;

        while (' ' == *arg)
        {
            arg++;
        }
        if ('\0' == *arg)
        {
            quiet = !console_quiet_get(); /* no argument: toggle */
        }
        else if (('1' == *arg) || (0 == strcmp(arg, "on")))
        {
            quiet = true;
        }
        else if (('0' == *arg) || (0 == strcmp(arg, "off")))
        {
            quiet = false;
        }
        else
        {
            prv_print("use: quiet | quiet 0 | quiet 1\r\n");
            return;
        }
        console_quiet_set(quiet);
        prv_print(quiet ? "status reports silenced\r\n"
                        : "status reports enabled\r\n");
    }
    else if (0 == strncmp(line, "set env ", 8))
    {
        prv_print((0 == iotc_config_write("env", &line[8], strlen(&line[8])))
                      ? "ok\r\n" : "error\r\n");
    }
    else if (0 == strncmp(line, "set cpid ", 9))
    {
        prv_print((0 == iotc_config_write("cpid", &line[9], strlen(&line[9])))
                      ? "ok\r\n" : "error\r\n");
    }
    else if (0 == strncmp(line, "set duid ", 9))
    {
        prv_print((0 == iotc_config_write("duid", &line[9], strlen(&line[9])))
                      ? "ok\r\n" : "error\r\n");
    }
    else if (0 == strcmp(line, "set cert"))
    {
        int n = prv_read_pem("certificate", "CERTIFICATE");
        if (n > 0)
        {
            prv_print((0 == iotc_config_write("cert", s_pem, (size_t) n))
                          ? "certificate stored\r\n" : "error\r\n");
        }
    }
    else if (0 == strcmp(line, "set key"))
    {
        int n = prv_read_pem("private key", "PRIVATE KEY");
        if (n > 0)
        {
            prv_print((0 == iotc_config_write("key", s_pem, (size_t) n))
                          ? "private key stored\r\n" : "error\r\n");
        }
    }
    else if (0 == strcmp(line, "apply"))
    {
        prv_print("applying stored configuration...\r\n");
        iotc_app_request_restart();
    }
    else if (0 == strcmp(line, "erase"))
    {
        prv_print((0 == iotc_config_erase()) ? "configuration erased\r\n"
                                             : "error\r\n");
    }
    else if (0 == strcmp(line, "reboot"))
    {
        prv_print("rebooting...\r\n");
        vTaskDelay(pdMS_TO_TICKS(200));
        NVIC_SystemReset();
    }
    else if (line[0] != '\0')
    {
        prv_print("unknown command; type 'help'\r\n");
    }
}

static void prv_cli_task(void *arg)
{
    (void) arg;
    char line[CLI_LINE_MAX];

    /* Give the boot banner a moment before offering the prompt. */
    vTaskDelay(pdMS_TO_TICKS(3000));
    prv_print("\r\nType 'help' for the IOTCONNECT provisioning CLI.\r\n");

    for (;;)
    {
        int n = prv_read_line(line, sizeof(line), true);
        if (n > 0)
        {
            prv_handle(line);
        }
    }
}

void iotc_cli_start(void)
{
    static StaticTask_t s_tcb;
    static StackType_t s_stack[CLI_TASK_STACK];
    (void) xTaskCreateStatic(prv_cli_task, "iotc_cli", CLI_TASK_STACK, NULL,
                             tskIDLE_PRIORITY + 1, s_stack, &s_tcb);
}
