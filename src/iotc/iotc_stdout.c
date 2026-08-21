/*
 * Route libc stdout/stderr (printf) to the demo console UART so library
 * diagnostics (littlefs LFS_ERROR, iotc-c-lib IOTCL_* logs) are visible.
 * picolibc-style: stdout is a FILE with a putc hook.
 */

#include <stdio.h>
#include <string.h>

#include "hal_data.h"

extern fsp_err_t print_to_console(char *p_data);

static char s_line[160];
static size_t s_fill;

static int prv_putc(char c, FILE *f)
{
    (void) f;
    if (s_fill < sizeof(s_line) - 1)
    {
        s_line[s_fill++] = c;
    }
    if ((c == '\n') || (s_fill >= sizeof(s_line) - 1))
    {
        s_line[s_fill] = 0;
        print_to_console(s_line);
        s_fill = 0;
    }
    return (unsigned char) c;
}

static FILE s_stdio = FDEV_SETUP_STREAM(prv_putc, NULL, NULL, _FDEV_SETUP_WRITE);

#ifdef __strong_reference
FILE *const stdout = &s_stdio;
__strong_reference(stdout, stderr);
#else
FILE *const stdout = &s_stdio;
FILE *const stderr = &s_stdio;
#endif
