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
#include <duat/9p-server.h>

static struct sexpr_io *stdio;
static struct sexpr_io *queue;
static struct io *queue_io;

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

static void mx_sx_stdio_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    sx_destroy (sx);
}

static void mx_sx_queue_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    sx_write (stdio, sx);
    sx_destroy (sx);
}

static void on_event_read
        (struct d9r_io *io, int_16 tag,
         struct dfs_file *f, int_64 offset, int_32 length)
{
    d9r_reply_read (io, tag, 6, (int_8*)"(nop)\n");
}

static int_32 on_event_write
        (struct dfs_file *f, int_64 offset, int_32 length, int_8 *data)
{
    io_write (queue_io, (char *)data, length);

    return length;
}

int cmain()
{
    set_resize_mem_recovery_function(rm_recover);
    set_get_mem_recovery_function(gm_recover);

    struct dfs *fs = dfs_create();
    struct dfs_directory *d_kyu  = dfs_mk_directory (fs->root, "kyu");

    dfs_mk_file (d_kyu, "raw", (char *)0, (int_8 *)0, 0, (void *)0,
                 on_event_read, on_event_write);

    queue_io = io_open_special();
    stdio = sx_open_stdio();

    queue = sx_open_io (queue_io, queue_io);

    multiplex_sexpr();
    multiplex_network();
    multiplex_d9s();

    multiplex_add_sexpr (stdio, mx_sx_stdio_read, (void *)0);
    multiplex_add_sexpr (queue, mx_sx_queue_read, (void *)0);

    multiplex_add_d9s_socket ("/dev/kyu-ipc-9p", fs);

    while (multiplex() == mx_ok);

    return 0;
}
