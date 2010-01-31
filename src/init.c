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
#include <curie/memory.h>
#include <curie/exec.h>
#include <curie/multiplex.h>
#include <curie/signal.h>
#include <curie/network.h>

#include <syscall/syscall.h>

#include <sievert/shell.h>

#include <kyuba/script.h>

static struct sexpr_io *monitorconnection = (struct sexpr_io *)0;

static void on_conn_read(sexpr sx, struct sexpr_io *io, void *p)
{
    /* could be doing something, right here... */
}

enum signal_callback_result on_sig_int (enum signal signal, void *u)
{
    static sexpr msg = (sexpr)0;

    if (msg == (sexpr)0)
    {
        msg = cons (sym_ctrl_alt_del, sx_end_of_list);
    }

    if (monitorconnection != (struct sexpr_io *)0)
    {
        sx_write (monitorconnection, msg);
    }

    return scr_keep;
}

static void on_init_death (struct exec_context *ctx, void *u)
{
    const char **commandline = (const char **)u;

    struct exec_context *context;
    if (ctx != (struct exec_context *)0)
    {
        free_exec_context (ctx);
    }

    context = execute(EXEC_CALL_PURGE | EXEC_CALL_CREATE_SESSION,
                      (char **)commandline,
                      curie_environment);

    switch (context->pid)
    {
        case 0:
        case -1:
            break; /* this is bad, but it should only happen during a
                      last-rites call, or when the monitor dies during a very
                      bad moment while updating kyuba. */
        default:
            monitorconnection = sx_open_io (context->in, context->out);

            multiplex_add_sexpr (monitorconnection, on_conn_read, (void *)0);
            multiplex_add_process(context, on_init_death, u);
            break;
    }
}

static void do_nothing (struct io *i, void *aux)
{
    /* really, we don't do anything here */
}

static void prevent_hissyfits ()
{
    struct io *in, *out;

    net_open_loop (&in, &out);

    multiplex_add_io (in,  do_nothing, do_nothing, (void *)0);
    multiplex_add_io (out, (void*)0, do_nothing, (void *)0);
}

static void global_death_notification (struct exec_context *ctx, void *aux)
{
    sexpr rv = (ctx->exitstatus == 0) ? sx_true
                                      : make_integer (ctx->exitstatus);

    sx_write (monitorconnection,
              cons (sym_process_terminated,
                  cons (make_integer (ctx->pid),
                        cons (rv, sx_end_of_list))));
}

int cmain ()
{
    static const char *cmd[] =
            { (char *)0, "/etc/kyu/init.sx", "local", (char *)0 };
    define_string (str_monitor, "monitor");

    sexpr mbinary = sx_false;

#if defined(have_sys_setsid)
    sys_setsid();
#endif

    mbinary = which (str_monitor);

    if (falsep(mbinary)) {
        return 1;
    }

    cmd[0] = sx_string (mbinary);

    multiplex_signal();
    multiplex_all_processes();
    multiplex_sexpr();

    multiplex_add_process ((struct exec_context *)0,
                           global_death_notification, (void *)0);

    on_init_death((void *)0, cmd);

    multiplex_add_signal (sig_int, on_sig_int, (void *)0);

#ifdef have_sys_close
    sys_close (0);
    sys_close (1);
#endif

    prevent_hissyfits ();

    while (1) multiplex(); /* make sure to not get outta this loop, ever */

    return 0; /* this should never be reached... */
}
