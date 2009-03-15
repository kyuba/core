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
#include <kyuba/ipc.h>

struct kaux
{
    void (*on_event)(sexpr, void *);
    void  *aux;
};

static struct d9r_io *d9io  = (struct d9r_io *)0;
static struct sexpr_io *kio = (struct sexpr_io *)0;
static sexpr cbuf           = sx_end_of_list;

void multiplex_kyu ()
{
    static char installed = (char)0;

    if (installed == (char)0) {
        multiplex_io    ();
        multiplex_sexpr ();
        multiplex_d9c   ();
        installed = (char)1;
    }
}

void kyu_disconnect ()
{
    if (d9io != (struct d9r_io *)0)
    {
        multiplex_del_d9r (d9io);
    }
}

static void on_event_read (sexpr e, struct sexpr_io *io, void *aux)
{
    struct kaux *k = (struct kaux *)aux;

    k->on_event (e, k->aux);
}

static void on_connect (struct d9r_io *io, void *aux)
{
    struct io *i = io_open_read_9p (io, "kyu/raw");
    struct io *o = io_open_write_9p (io, "kyu/raw");

    d9io = io;
    kio  = sx_open_io (i, o);

    multiplex_add_sexpr (kio, on_event_read, aux);

    if (consp(cbuf))
    {
        sexpr c = cbuf;

        do
        {
            sx_write (kio, car(c));
            c = cdr (c);
        }
        while (consp (c));

        sx_destroy (cbuf);
        cbuf = sx_end_of_list;
    }
}

static void on_error (struct d9r_io *io, const char *error, void *aux)
{
    struct kaux *k = (struct kaux *)aux;

    k->on_event (cons (sym_error, make_string (error)), k->aux);
}

void multiplex_add_kyu_socket
        (const char *socket, void (*on_event)(sexpr, void *), void *aux)
{
    struct memory_pool p = MEMORY_POOL_INITIALISER (sizeof (struct kaux));
    struct kaux *a = get_pool_mem (&p);

    a->on_event = on_event;
    a->aux      = aux;

    multiplex_add_d9c_socket (socket, on_connect, on_error, (void *)a);
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
