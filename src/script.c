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
#include <curie/memory.h>

#include <curie/shell.h>
#include <kyuba/script.h>
#include <kyuba/sx-distributor.h>

void initialise_kyu_script_commands ( void )
{
    static char installed = (char)0;

    if (installed == (char)0)
    {
        multiplex_all_processes ();
        initialise_seteh ();
    }
}

sexpr kyu_sx_default_environment ( void )
{
    static sexpr env = sx_nonexistent;

    if (nexp (env))
    {
        env = lx_make_environment
                (cons (cons (sym_run,
                             lx_foreign_mu (sym_run, kyu_sc_run)),
                 cons (cons (sym_keep_alive,
                             lx_foreign_mu (sym_keep_alive, kyu_sc_keep_alive)),
                       sx_end_of_list)));
    }

    return env;
}

static void on_death (struct exec_context *ctx, void *u)
{
    struct machine_state *stu = (struct machine_state *)u;
    sexpr nstate;
    sexpr rv = (ctx->exitstatus == 0) ? sx_true
                                      : make_integer (ctx->exitstatus);

    nstate = lx_make_state (cons (rv, sx_end_of_list), stu->environment,
                            stu->code, stu->dump);

//    kyu_sd_write_to_all_listeners (nstate, (void *)0);

    kyu_sd_write_to_all_listeners
            (cons (sym_process_terminated,
                   cons (make_integer (ctx->pid),
                         cons (rv, sx_end_of_list))), (void *)0);

    free_exec_context (ctx);

    lx_continue (nstate);
}

static void on_death_respawn (struct exec_context *ctx, void *u)
{
    sexpr rv = (ctx->exitstatus == 0) ? sx_true
                                      : make_integer (ctx->exitstatus);

    kyu_sd_write_to_all_listeners
            (cons (sym_process_terminated,
                   cons (make_integer (ctx->pid),
                         cons (rv, sx_end_of_list))), (void *)0);

    free_exec_context (ctx);

//    kyu_sd_write_to_all_listeners ((sexpr)u, (sexpr)0);

    kyu_sc_keep_alive ((sexpr)u, (void *)sx_end_of_list);
}

static struct exec_context *sc_run_x (sexpr sx)
{
    sexpr cur = sx;
    unsigned int length = 0;
    char do_io = 1;
    define_symbol (sym_launch_with_io, "launch-with-io");
    define_symbol (sym_launch_without_io, "launch-without-io");

    if (falsep (car (sx)))
    {
        cur = cdr (cur);
        do_io = 0;
        sx = cur;
    }

    sexpr t = cons (((do_io == (char)1) ? sym_launch_with_io
                                        : sym_launch_without_io), cur);

    kyu_sd_write_to_all_listeners (t, (void *)0);

    while (consp(cur))
    {
        length++;
        cur = cdr(cur);
    }

    if (length > 0)
    {
        char *x[(length + 1)];
        unsigned int i = 0;
        struct exec_context *proccontext;

        cur = sx;
        while (consp(cur))
        {
            sexpr c = car(cur);
            const char *s = sx_string(c);
            if ((i == 0) && (s[0] != '/'))
            {
                c = which (c);
                if (falsep (c))
                {
                    return (struct exec_context *)0;
                }
                x[0] = (char *)sx_string(c);
            }
            else
            {
                x[i] = (char *)s;
            }
            i++;
            cur = cdr(cur);
        }
        x[i] = (char *)0;

        if (do_io == (char)1)
        {
            proccontext
                    = execute(EXEC_CALL_PURGE | EXEC_CALL_CREATE_SESSION,
                              x, curie_environment);

            kyu_sd_sx_queue_connect
                    (sx_open_io (proccontext->in, proccontext->out), (void *)0);
        }
        else
        {
            proccontext
                    = execute(EXEC_CALL_PURGE | EXEC_CALL_CREATE_SESSION,
                              x, curie_environment);

/*            multiplex_add_io
                    (proccontext->in,
                     on_read_write_to_console, (void *)0, (void *)0);*/
        }

        if (proccontext->pid > 0)
        {
            return proccontext;
        }
        else
        {
            cexit(25);
        }
    }

    return (struct exec_context *)0;
}

sexpr kyu_sc_run (sexpr arguments, struct machine_state *state)
{
    struct exec_context *c = sc_run_x (arguments);

    if (c != (struct exec_context *)0)
    {
        sexpr continuation = lx_make_state
                (sx_end_of_list, sx_end_of_list, sx_end_of_list, state->dump);
        multiplex_add_process (c, on_death, (void *)continuation);

        state->environment = sx_end_of_list;
        state->stack = sx_end_of_list;
        state->code = sx_end_of_list;
        state->dump = sx_end_of_list;

        return sx_nonexistent;
    }

    return sx_false;
}

sexpr kyu_sc_keep_alive (sexpr arguments, struct machine_state *state)
{
    struct exec_context *c = sc_run_x (arguments);

    if (c != (struct exec_context *)0)
    {
        multiplex_add_process(c, on_death_respawn, (void *)arguments);

        return sx_true;
    }

    return sx_false;
}
