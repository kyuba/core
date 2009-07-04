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
#include <duat/9p-server.h>
#include <kyuba/ipc-9p.h>

struct open_read_data
{
    struct d9r_io         *io;
    struct io             *iox;
    int_16                 tag;
    struct open_read_data *next;
};

struct sexpr_io              *stdio     = (struct sexpr_io *)0;
static struct sexpr_io       *queue;
static struct sexpr_io       *sx_io_buf;
static struct io             *queue_io;
static struct io             *io_buf;

static struct open_read_data *od        = (struct open_read_data *)0;

static void mx_sx_stdio_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    sx_write (sx_io_buf, sx);
}

static void mx_sx_queue_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    sx_write (sx_io_buf, sx);
    sx_write (stdio, sx);
}

static void enqueue_read_request (struct d9r_io *io, int_16 tag)
{
    struct memory_pool p
            = MEMORY_POOL_INITIALISER (sizeof (struct open_read_data));
    struct open_read_data *c = od;

    while (c != (struct open_read_data *)0)
    {
        if (c->tag == NO_TAG_9P)
        {
            if (c->io == (struct d9r_io *)0)
            {
                c->tag           = tag;
                c->io            = io;
                c->iox->length   = 0;
                c->iox->position = 0;
                return;
            }
            else if (c->io == io)
            {
                c->tag           = tag;
                return;
            }
        }

        c                        = c->next;
    }

    c                            = get_pool_mem (&p);

    c->io                        = io;
    c->iox                       = io_open_special ();
    c->tag                       = tag;
    c->next                      = od;
    od                           = c;
}

static void handle_read_requests ()
{
    struct open_read_data *c = od;

    while (c != (struct open_read_data *)0)
    {
        if ((c->io  != (struct d9r_io *)0) &&
            (c->tag != NO_TAG_9P))
        {
            struct io *io = c->iox;
            unsigned int l = io->length - io->position;

            if (l > 0)
            {
                if (l > 0x1000) l = 0x1000;
                d9r_reply_read (c->io, c->tag, l,
                                (int_8 *)io->buffer + io->position);

                io->position += l;
                c->tag = NO_TAG_9P;
            }
        }

        c = c->next;
    }
}

static void io_buf_read (struct io *io, void *aux)
{
    struct open_read_data *c      = od;
    char                  *bstart = io->buffer + io->position;
    unsigned int           len    = io->length - io->position;

    while (c != (struct open_read_data *)0)
    {
        if (c->io != (struct d9r_io *)0)
        {
            io_write (c->iox, bstart, len);
        }
        c = c->next;
    }

    io->position = io->length;

    handle_read_requests ();
}

static void on_event_read
        (struct d9r_io *io, int_16 tag,
         struct dfs_file *f, int_64 offset, int_32 length)
{
    enqueue_read_request (io, tag);
}

static int_32 on_event_write
        (struct dfs_file *f, int_64 offset, int_32 length, int_8 *data)
{
    io_write (queue_io, (char *)data, length);

    return length;
}

static void on_client_disconnect (struct d9r_io *io, void *aux)
{
    struct open_read_data *c = od;

    while (c != (struct open_read_data *)0)
    {
        if (c->io == io)
        {
            c->io            = (struct d9r_io *)0;
            c->tag           = NO_TAG_9P;
            c->iox->length   = 0;
            c->iox->position = 0;
        }

        c = c->next;
    }
}

int cmain()
{
    char *socket = KYU_9P_IPC_SOCKET;

    if (curie_argv[1] != (char *)0)
    {
        socket = curie_argv[1];
    }

    terminate_on_allocation_errors();

    struct dfs *fs = dfs_create(on_client_disconnect, (void *)0);
    struct dfs_directory *d_kyu  = dfs_mk_directory (fs->root, "kyu");

    io_buf         = io_open_special();
    sx_io_buf      = sx_open_io (io_open (-1), io_buf);

    dfs_mk_file (d_kyu, "raw", (char *)0, (int_8 *)0, 0, (void *)0,
                 on_event_read, on_event_write);

    queue_io       = io_open_special();
    stdio          = sx_open_stdio();

    queue = sx_open_io (queue_io, queue_io);

    multiplex_sexpr();
    multiplex_network();
    multiplex_d9s();

    multiplex_add_sexpr (stdio,  mx_sx_stdio_read, (void *)0);
    multiplex_add_sexpr (queue,  mx_sx_queue_read, (void *)0);
    multiplex_add_io    (io_buf, io_buf_read, (void *)0, (void *)0);

    multiplex_add_d9s_socket (socket, fs);

#if defined(have_sys_chmod)
    sys_chmod (socket, 0660);
#endif

    while (multiplex() == mx_ok);

    return 0;
}
