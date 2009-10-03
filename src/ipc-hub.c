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
#include <curie/memory.h>
#include <curie/multiplex.h>
#include <curie/network.h>
#include <syscall/syscall.h>
#include <kyuba/ipc.h>

int cmain()
{
    char *socket = KYU_IPC_SOCKET;

    terminate_on_allocation_errors();

    if (curie_argv[1] != (char *)0)
    {
        socket = curie_argv[1];
    }

    multiplex_kyu ();

    kyu_sd_add_listener (sx_open_stdio ());
    multiplex_add_socket_sx (socket, kyu_sd_sx_queue_connect, (void *)0);

#if defined(have_sys_chmod)
    sys_chmod (socket, 0660);
#endif

    while (multiplex() == mx_ok);

    return 0;
}
