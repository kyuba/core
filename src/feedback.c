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

#include <curie/main.h>
#include <curie/stack.h>
#include <curie/memory.h>
#include <curie/sexpr.h>
#include <curie/multiplex.h>
#include <curie/signal.h>
#include <syscall/syscall.h>
#include <kyuba/ipc.h>

#if defined(have_sys_ioctl)
#include <sys/ioctl.h>
#endif

static int width  = 80;
static int height = 25;

struct sexpr_io *stdio;

static void on_event (sexpr event, void *aux)
{
    sx_write (stdio, event);
}

static void initialise_window ()
{
#if defined(have_sys_ioctl) && defined(TIOCGWINSZ)
    struct winsize size;

    if (sys_ioctl (0, TIOCGWINSZ, (long)&size) == 0)
    {
        width  = size.ws_col;
        height = size.ws_row;
    }
#endif
}

static enum signal_callback_result resize_handler
    (enum signal signal, void *aux)
{
    initialise_window ();

    return scr_keep;
}

int cmain ()
{
    stdio = sx_open_stdio();

    multiplex_signal ();
    multiplex_kyu ();

    multiplex_add_kyu_default (on_event, (void *)0);

    multiplex_add_signal
        (sig_winch, resize_handler, (void *)0);

    while (multiplex() == mx_ok);

    return 0;
}
