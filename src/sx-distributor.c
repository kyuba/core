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

#include <curie/multiplex.h>
#include <curie/memory.h>

#include <kyuba/sx-distributor.h>

void (*kyu_sd_on_read) (sexpr) = (void (*) (sexpr))0;

struct socket_list
{
    struct sexpr_io    *io;
    struct socket_list *next;
};

static struct socket_list *sl = (struct socket_list *)0;

void kyu_sd_write_to_all_listeners (sexpr sx, struct sexpr_io *except)
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

    if (kyu_sd_on_read != (void (*) (sexpr))0)
    {
        kyu_sd_on_read (sx);
    }
}

void kyu_sd_remove_listener (struct sexpr_io *io)
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

void kyu_sd_add_listener (struct sexpr_io *io)
{
    struct memory_pool pool =
            MEMORY_POOL_INITIALISER (sizeof (struct socket_list));
    struct socket_list *nl = get_pool_mem (&pool);

    nl->io   = io;
    nl->next = sl;
    sl       = nl;

    multiplex_add_sexpr (io, kyu_sd_sx_queue_read, (void *)0);
}

void kyu_sd_sx_queue_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    if (eofp (sx))
    {
        kyu_sd_remove_listener (io);
    }
    else
    {
        kyu_sd_write_to_all_listeners (sx, io);
    }
}

void kyu_sd_sx_queue_connect (struct sexpr_io *io, void *aux)
{
    define_symbol (sym_connected, "connected");

    kyu_sd_add_listener (io);

    kyu_sd_write_to_all_listeners (cons (sym_connected, sx_end_of_list), io);
}
