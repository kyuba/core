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
#include <curie/sexpr.h>
#include <curie/memory.h>
#include <curie/multiplex.h>
#include <curie/network.h>
#include <syscall/syscall.h>
#include <kyuba/ipc.h>

struct socket_list
{
    struct sexpr_io    *io;
    struct socket_list *next;
};

static struct socket_list *sl = (struct socket_list *)0;

static void mx_sx_queue_read (sexpr sx, struct sexpr_io *io, void *aux);

static void write_to_all_listeners (sexpr sx, struct sexpr_io *except)
{
    struct socket_list *c = sl;

    while (c != (struct socket_list *)0)
    {
        if (c->io != except)
        {
            sx_write (c->io, sx);
        }
        c = c->next;
    }
}

static void remove_listener (struct sexpr_io *io)
{
    struct socket_list **ll = &sl;
    struct socket_list *c   = sl;

    while (c != (struct socket_list *)0)
    {
        if (c->io == io)
        {
            *ll = c->next;
            free_pool_mem ((void *)c);
            c = *ll;
            continue;
        }

        ll = &(sl->next);
        c = c->next;
    }
}

static void add_listener (struct sexpr_io *io)
{
    struct memory_pool pool =
            MEMORY_POOL_INITIALISER (sizeof (struct socket_list));
    struct socket_list *nl = get_pool_mem (&pool);

    nl->io   = io;
    nl->next = sl;
    sl       = nl;

    multiplex_add_sexpr (io, mx_sx_queue_read, (void *)0);
}

static void mx_sx_queue_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    if (eofp (sx))
    {
        remove_listener (io);
    }
    else
    {
        write_to_all_listeners (sx, (void *)0);
    }
}

static void mx_sx_queue_connect (struct sexpr_io *io, void *aux)
{
    define_symbol (sym_connected, "connected");

    add_listener (io);

    write_to_all_listeners (cons (sym_connected, sx_end_of_list), io);
}

int cmain()
{
    char *socket = KYU_IPC_SOCKET;
    struct sexpr_io *stdio = (struct sexpr_io *)0;

    if (curie_argv[1] != (char *)0)
    {
        socket = curie_argv[1];
    }

    terminate_on_allocation_errors();

    stdio          = sx_open_stdio();

    multiplex_sexpr();
    multiplex_network();

    add_listener (stdio);
    multiplex_add_socket_sx (socket, mx_sx_queue_connect, (void *)0);

#if defined(have_sys_chmod)
    sys_chmod (socket, 0660);
#endif

    while (multiplex() == mx_ok);

    return 0;
}
