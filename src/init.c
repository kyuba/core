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
#include <curie/shell.h>

#include <syscall/syscall.h>

#include <kyuba/script.h>

#define PATH "PATH=/bin:/sbin"

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

static const char **commandline;
static struct sexpr_io *monitorconnection = (struct sexpr_io *)0;
static sexpr mbinary = sx_false;

static void on_conn_read(sexpr sx, struct sexpr_io *io, void *p)
{
    /* could be doing something, right here... */
    sx_destroy (sx);
}

enum signal_callback_result on_sig_int (enum signal signal, void *u)
{
    static sexpr msg = (sexpr)0;

    if (msg == (sexpr)0)
    {
        msg = cons (sym_event, cons(sym_ctrl_alt_del, sx_end_of_list));
    }

    if (monitorconnection != (struct sexpr_io *)0)
    {
        sx_write (monitorconnection, msg);
    }

    return scr_keep;
}

static void on_init_death (struct exec_context *ctx, void *u)
{
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
            multiplex_add_process(context, on_init_death, (void *)0);
            break;
    }
}

int cmain ()
{
    static const char *cmd[] = { (char *)0, "/etc/kyu/init.sx", (char *)0 };
    define_string (str_monitor, "monitor");

    for (int i = 0; curie_environment[i] != (char *)0; i++)
    {
        char *y = curie_environment[i];
        if ((y[0] == 'P') && (y[1] == 'A') && (y[2] == 'T') && (y[3] == 'H') &&
            (y[4] == '='))
        {
            curie_environment[i] = PATH;
        }
    }

    set_resize_mem_recovery_function(rm_recover);
    set_get_mem_recovery_function(gm_recover);

    mbinary = which (str_monitor);

    if (falsep(mbinary)) {
        return 1;
    }

    cmd[0] = sx_string (mbinary);
    commandline = cmd;

    multiplex_signal();
    multiplex_all_processes();
    multiplex_sexpr();

    on_init_death((void *)0, (void *)0);

    multiplex_add_signal (sig_int, on_sig_int, (void *)0);

#ifdef have_sys_close
    sys_close (0);
    sys_close (1);
#endif

    while (1) multiplex(); /* make sure to not get outta this loop, ever */

    return 0; /* this should never be reached... */
}
