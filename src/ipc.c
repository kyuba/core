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

#include <curie/memory.h>
#include <curie/multiplex.h>
#include <curie/network.h>
#include <kyuba/ipc.h>

struct kaux
{
    void (*on_event)(sexpr, void *);
    void  *aux;
};

static struct sexpr_io *kio = (struct sexpr_io *)0;
static sexpr cbuf           = sx_end_of_list;

void multiplex_kyu ()
{
    static char installed = (char)0;

    if (installed == (char)0)
    {
        multiplex_io ();
        multiplex_sexpr ();
        multiplex_network ();
        installed = (char)1;
    }
}

void kyu_disconnect ()
{
    if (kio != (struct sexpr_io *)0)
    {
        sx_close_io (kio);
        kio = (struct sexpr_io *)0;
    }
}

void kyu_command (sexpr s)
{
    if (kio != (struct sexpr_io *)0)
    {
        sx_write (kio, s);
    }
    else
    {
        cbuf = cons (s, cbuf);
    }
}

static void on_event_read (sexpr e, struct sexpr_io *io, void *aux)
{
    struct kaux *k = (struct kaux *)aux;

    k->on_event (e, k->aux);
}

void multiplex_add_kyu_sexpr
        (struct sexpr_io *io, void (*on_event)(sexpr, void *), void *aux)
{
    struct memory_pool p = MEMORY_POOL_INITIALISER (sizeof (struct kaux));
    struct kaux *a = get_pool_mem (&p);

    a->on_event = on_event;
    a->aux      = aux;

    kio = io;

    multiplex_add_sexpr (kio, on_event_read, a);

    if (consp(cbuf))
    {
        sexpr c = cbuf;

        do
        {
            sx_write (kio, car(c));
            c = cdr (c);
        }
        while (consp (c));

        cbuf = sx_end_of_list;
    }
}

void multiplex_add_kyu_stdio (void (*on_event)(sexpr, void *), void *aux)
{
    struct sexpr_io *io = sx_open_stdio ();
    multiplex_add_kyu_sexpr (io, on_event, aux);
}

void multiplex_add_kyu_default (void (*on_event)(sexpr, void *), void *aux)
{
    struct sexpr_io *io = sx_open_socket (KYU_IPC_SOCKET);
    multiplex_add_kyu_sexpr (io, on_event, aux);
}
