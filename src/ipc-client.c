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
#include <curie/sexpr.h>
#include <curie/multiplex.h>
#include <duat/9p-client.h>
#include <kyuba/ipc.h>

static struct sexpr_io *stdio = (struct sexpr_io *)0;

static void *rm_recover(unsigned long int s, void *c, unsigned long int l)
{
    cexit(22);
    return (void *)0;
}

static void *gm_recover(unsigned long int s)
{
    cexit(23);
    return (void *)0;
}

static void on_read_stdio (sexpr e, struct sexpr_io *io, void *aux)
{
    kyu_command (e);
}

static void on_event (sexpr event, void *aux)
{
    sx_write (stdio, event);

    if (consp (event) && truep(equalp(car(event), sym_disconnect)))
    {
        cexit (24);
    }
}

int cmain()
{
    set_resize_mem_recovery_function(rm_recover);
    set_get_mem_recovery_function(gm_recover);

    stdio = sx_open_stdio ();

    multiplex_kyu ();

    multiplex_add_kyu_socket
            (KYU_9P_IPC_SOCKET, on_event, (void *)0);
    multiplex_add_sexpr (stdio, on_read_stdio, (void *)0);

/*    kyu_command (cons (sym_shut_down, sx_end_of_list));*/

    while (multiplex() == mx_ok);

    return 0;
}
