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
#include <curie/multiplex.h>
#include <duat/9p-client.h>

static struct io *stdout      = (struct io *)0;
static struct io *stdin       = (struct io *)0;
static struct sexpr_io *stdio = (struct sexpr_io *)0;
static struct d9r_io *d9io    = (struct d9r_io *)0;

static void on_read_stdin (struct io *io, void *aux)
{
    struct io *o = (struct io *)aux;

    io_write (o, io->buffer + io->position, io->length - io->position);
    io->position = io->length;
}

static void on_close_stdin (struct io *io, void *aux)
{
    multiplex_del_io  (stdout);
    multiplex_del_d9r (d9io);
}

static void on_connect (struct d9r_io *io, void *aux)
{
    d9io = io;

    struct io *n = io_open_write_9p (io, "kyu/raw");
    multiplex_add_io (stdin, on_read_stdin, on_close_stdin, (void *)n);
}

static void on_error (struct d9r_io *io, const char *error, void *aux)
{
    sx_write (stdio, make_string (error));

    cexit (3);
}

int cmain()
{
    stdin          = io_open (0);
    stdin->type    = iot_read;
    stdout         = io_open (1);
    stdout->type   = iot_write;

    stdio          = sx_open_io (stdin, stdout);

    multiplex_io  ();
    multiplex_d9c ();

    multiplex_add_d9c_socket
            ("/dev/kyu-ipc-9p", on_connect, on_error, (void *)0);
    multiplex_add_io_no_callback (stdout);

    while (multiplex() == mx_ok);

    return 0;
}
