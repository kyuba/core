/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2008-2010, Kyuba Project Members
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

sexpr native_system            = sx_nil;
sexpr programme_identification = sx_nil;

struct kaux
{
    void       (*on_event)(sexpr, void *);
    void        *aux;
    struct kaux *next;
};

static struct kaux *auxlist;
static sexpr cmdtemp = sx_end_of_list;

static void on_ipc_read (sexpr sx)
{
    struct kaux *c = auxlist;

    if (consp (sx))
    {
        sexpr a = car (sx);

        if (truep (equalp (sym_native_system, a)))
        {
            native_system = cdr (sx);
            return;
        }
        else if (truep (equalp (sym_statusp, a)))
        {
            kyu_command (cons (sym_status, cons (native_system, cons
                           (programme_identification, sx_end_of_list))));
        }
    }

    while (c != (struct kaux *)0)
    {
        c->on_event (sx, c->aux);
        c = c->next;
    }
}

void multiplex_kyu ()
{
    static char installed = (char)0;

    if (installed == (char)0)
    {
        multiplex_io ();
        multiplex_sexpr ();
        multiplex_network ();
        kyu_sd_on_read = on_ipc_read;
        installed = (char)1;
    }
}

void kyu_disconnect ()
{
}

void kyu_command (sexpr sx)
{
    if (auxlist != (struct kaux *)0)
    {
        kyu_sd_write_to_all_listeners (sx, (void *)0);
    }
    else
    {
        cmdtemp = cons (sx, cmdtemp);
    }
}

void multiplex_add_kyu_sexpr
        (struct sexpr_io *io, void (*on_event)(sexpr, void *), void *aux)
{
    if (on_event != (void (*)(sexpr, void *))0)
    {
        multiplex_add_kyu_callback (on_event, aux);
    }

    kyu_sd_sx_queue_connect (io, (void *)0);

    while (consp (cmdtemp))
    {
        kyu_sd_write_to_all_listeners (car (cmdtemp), (void *)0);

        cmdtemp = cdr (cmdtemp);
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

void multiplex_add_kyu_callback (void (*on_event)(sexpr, void *), void *aux)
{
    struct memory_pool p = MEMORY_POOL_INITIALISER (sizeof (struct kaux));
    struct kaux *a = get_pool_mem (&p);

    a->on_event = on_event;
    a->aux      = aux;
    a->next     = auxlist;

    auxlist     = a;
}
