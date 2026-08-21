/*
 * Route libc stdout/stderr (printf) to the demo console UART so library
 * diagnostics (littlefs LFS_ERROR, iotc-c-lib IOTCL_* logs) are visible.
 */

#include <string.h>
#include <stdio.h>

#include "hal_data.h"

extern fsp_err_t print_to_console(char *p_data);

int _write(int fd, const void *buf, size_t len);

int _write(int fd, const void *buf, size_t len)
{
    (void) fd;
    static char line[160];
    size_t off = 0;
    while (off < len)
    {
        size_t n = len - off;
        if (n > sizeof(line) - 1)
        {
            n = sizeof(line) - 1;
        }
        memcpy(line, (const char *) buf + off, n);
        line[n] = 0;
        print_to_console(line);
        off += n;
    }
    return (int) len;
}
