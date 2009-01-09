/*
 *  ipc-hub.c
 *  kyuba
 *
 *  Created by Magnus Deininger on 09/01/2009.
 *  Copyright 2009 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2009, Magnus Deininger All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. *
 * Neither the name of the project nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
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
}

static void mx_sx_queue_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    sx_write (stdio, sx);
}

static void on_event_read
        (struct d9r_io *io, int_16 tag,
         struct dfs_file *f, int_64 offset, int_32 length)
{
    d9r_reply_read (io, tag, 6, (int_8*)"(nop)\n");
}

int_32 on_event_write
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
    struct io *queue_in;

    dfs_mk_file (d_kyu, "raw", (char *)0, (int_8 *)0, 0, (void *)0,
                 on_event_read, on_event_write);

    queue_io = io_open_special();
    stdio = sx_open_stdio();

    net_open_loop (&queue_in, &queue_io);
/*    queue = sx_open_io (queue_io, queue_io);*/
    queue = sx_open_io (queue_in, queue_io);

    multiplex_sexpr();
    multiplex_network();
    multiplex_d9s();

    multiplex_add_sexpr (stdio, mx_sx_stdio_read, (void *)0);
    multiplex_add_sexpr (queue, mx_sx_queue_read, (void *)0);

    multiplex_add_d9s_socket ("/dev/kyu-ipc-9p", fs);

    while (multiplex() == mx_ok);

    return 0;
}
