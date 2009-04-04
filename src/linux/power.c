/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2008, 2009, Kyuba Project Members
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

#include <curie/main.h>
#include <curie/io.h>
#include <syscall/syscall.h>
#include <linux/reboot.h>

static int print_help ()
{
#define help "power (reset|down)\n\n\
Commands:\n\
  down   shut down the computer (immediately)\n\
  reset  re-boot the ocmputer (immediately)\n"

    struct io *out = io_open (1);
    if (out != (struct io *)0)
    {
        io_write (out, help, (sizeof (help) -1));
    }

    return 1;
}

static int power_down ()
{
    sys_sync ();
    sys_reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_POWER_OFF, (void *)0);

    return 2; /* not reached */
}

static int power_reset ()
{
    sys_sync ();
    sys_reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                LINUX_REBOOT_CMD_RESTART, (void *)0);

    return 3; /* not reached */
}

int cmain ()
{
    char *c = curie_argv[1];

    if (c == (char *)0) return print_help ();

    if ((c[0] == 'd') && (c[1] == 'o') && (c[2] == 'w') && (c[3] == 'n'))
    {
        return power_down();
    }

    if ((c[0] == 'r') && (c[1] == 'e') && (c[2] == 's') && (c[3] == 'e') &&
        (c[4] == 't'))
    {
        return power_reset();
    }

    return print_help ();
}
