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
#include <curie/multiplex.h>
#include <curie/memory.h>

#include <sievert/shell.h>
#include <sievert/sexpr.h>

#include <kyuba/ipc.h>

struct open_cfg_requests
{
    sexpr id;
    sexpr key;
    sexpr continuation;
    struct open_cfg_requests *next;
};

struct termination_meta
{
    sexpr regex;
    sexpr continuation;
    struct termination_meta *next;
};

static sexpr configuration_data = sx_end_of_list;
static struct open_cfg_requests *open_cfg_requests = (void *)0;
static struct termination_meta  *termination_meta = (void *)0;
static sexpr running_processes  = sx_end_of_list;

static void script_command_callback (sexpr sx, void *aux)
{
    if (consp (sx))
    {
        sexpr a = car (sx);

        if (truep (equalp (a, sym_reply)))
        {
            sx = cdr (sx);
            a  = car (sx);

            if (truep (equalp (a, sym_configuration)))
            {
                struct open_cfg_requests *cur = open_cfg_requests;
                struct open_cfg_requests **p  = &open_cfg_requests;

                sx = cdr (sx);
                a  = car (sx);
                sx = car (cdr (sx));

                while (cur != (void *)0)
                {
                    if (truep (equalp (cur->id, a)))
                    {
                        *p = cur->next;

                        if (nexp (cur->key))
                        {
                            lx_continue_explicit
                                (cons (sx, sx_end_of_list), sx_end_of_list,
                                 sx_end_of_list, cur->continuation);
                        }
                        else
                        {
                            lx_continue_explicit
                                (cons (lx_environment_lookup (sx, cur->key),
                                       sx_end_of_list),
                                 sx_end_of_list, sx_end_of_list,
                                 cur->continuation);
                        }

                        free_pool_mem (cur);
                        cur = *p;
                        continue;
                    }

                    p   = &(cur->next);
                    cur = cur->next;
                }
            }
        }
    }
}

static void global_death_notification (struct exec_context *ctx, void *aux)
{
    sexpr rv = (ctx->exitstatus == 0) ? sx_true
                                      : make_integer (ctx->exitstatus);

    kyu_sd_write_to_all_listeners
        (cons (sym_process_terminated,
               cons (make_integer (ctx->pid),
                     cons (rv, sx_end_of_list))), (void *)0);
}

void initialise_kyu_script_commands ( void )
{
    static char installed = (char)0;

    if (installed == (char)0)
    {
        initialise_seteh ();
        multiplex_kyu ();
        multiplex_all_processes ();

        multiplex_add_process ((struct exec_context *)0,
                               global_death_notification, (void *)0);

        multiplex_add_kyu_callback (script_command_callback, (void *)0);
        configuration_data = lx_make_environment (sx_end_of_list);

        installed = (char)1;
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
                 cons (cons (sym_get_configuration,
                             lx_foreign_mu (sym_get_configuration,
                                            kyu_sc_get_configuration)),
                 cons (cons (sym_message,
                             lx_foreign_mu (sym_message, kyu_sc_message)),
                 cons (cons (sym_exit,
                             lx_foreign_mu (sym_exit, kyu_sc_exit)),
                 cons (cons (sym_kill_subprocesses,
                             lx_foreign_mu (sym_kill_subprocesses,
                                            kyu_sc_kill_subprocesses)),
                 cons (cons (sym_out,
                             lx_foreign_mu (sym_out, kyu_sc_out)),
                       sx_end_of_list))))))));
    }

    return env;
}

static void on_death (struct exec_context *ctx, void *u)
{
    sexpr rv = (ctx->exitstatus == 0) ? sx_true
                                      : make_integer (ctx->exitstatus);

    free_exec_context (ctx);

    lx_continue_explicit
        (cons (rv, sx_end_of_list), sx_end_of_list, sx_end_of_list, (sexpr)u);
}

static void on_death_respawn (struct exec_context *ctx, void *u)
{
    sexpr name = car (u);
    struct termination_meta *cur = termination_meta;
    struct termination_meta **p  = &termination_meta;
    int respawn = ~0;
    sexpr continuations = sx_end_of_list;

    free_exec_context (ctx);

    if (falsep (name))
    {
        name = car (cdr (u));
    }

    while (cur != (void *)0)
    {
        if (truep (sx_set_rx_memberp (running_processes, cur->regex)))
        {
            respawn = 0;
            break;
        }

        cur = cur->next;
    }

    cur = termination_meta;

    running_processes = sx_set_remove (running_processes, name);

    while (cur != (void *)0)
    {
        if (falsep (sx_set_rx_memberp (running_processes, cur->regex)))
        {
            continuations = cons (cur->continuation, continuations);

            *p  = cur->next;
            cur = *p;

            continue;
        }

        p   = &(cur->next);
        cur = cur->next;
    }

    while (consp (continuations))
    {
        lx_continue_explicit
            (cons (sx_true, sx_end_of_list), sx_end_of_list, sx_end_of_list,
             car(continuations));

        continuations = cdr (continuations);
    }

    if (respawn)
    {
        kyu_sc_keep_alive ((sexpr)u, (void *)sx_end_of_list);
    }
}

static struct exec_context *sc_run_x (sexpr sx)
{
    sexpr cur = sx;
    unsigned int length = 0;
    char do_io = 1;
    define_symbol (sym_launch_with_io,    "launch-with-io");
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
        sexpr c = car (cur);

        if (stringp (c))
        {
            length++;
        }

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

            if (stringp (c))
            {
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
            }

            cur = cdr(cur);
        }
        x[i] = (char *)0;

        if (do_io == (char)1)
        {
            struct sexpr_io *io;

            proccontext
                    = execute(EXEC_CALL_PURGE | EXEC_CALL_CREATE_SESSION,
                              x, curie_environment);

            io = sx_open_io (proccontext->in, proccontext->out);
            sx_write (io, cons (sym_native_system, native_system));
            kyu_sd_add_listener (io);
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
    if (eolp (state->stack))
    {
        state->stack = cons(lx_foreign_mu (sym_run, kyu_sc_run), state->stack);

        return sx_nonexistent;
    }
    else
    {
        struct exec_context *c = sc_run_x (arguments);

        if (c != (struct exec_context *)0)
        {
            sexpr continuation = state->dump;
            multiplex_add_process (c, on_death, (void *)continuation);

            state->environment = sx_end_of_list;
            state->stack = sx_end_of_list;
            state->code = sx_end_of_list;
            state->dump = sx_end_of_list;

            return sx_nonexistent;
        }

        return sx_false;
    }
}

sexpr kyu_sc_keep_alive (sexpr arguments, struct machine_state *state)
{
    if (!eolp ((sexpr)state) && eolp (state->stack))
    {
        state->stack = cons(lx_foreign_mu (sym_keep_alive, kyu_sc_keep_alive),
                            state->stack);

        return sx_nonexistent;
    }
    else
    {
        struct exec_context *c = sc_run_x (arguments);
    
        running_processes = sx_set_add (running_processes, car (arguments));

        if (c != (struct exec_context *)0)
        {
            multiplex_add_process(c, on_death_respawn, (void *)arguments);

            return sx_true;
        }

        return sx_false;
    }
}

sexpr kyu_sc_get_configuration (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack =
                cons(lx_foreign_mu (sym_get_configuration,
                                    kyu_sc_get_configuration),
                     state->stack);

        return sx_nonexistent;
    }
    else
    {
        struct memory_pool p
                = MEMORY_POOL_INITIALISER (sizeof (struct open_cfg_requests));
        struct open_cfg_requests *a = get_pool_mem (&p);

        a->id           = car (arguments);
        a->key          = car (cdr (arguments));
        a->continuation = state->dump;
        a->next         = open_cfg_requests;

        open_cfg_requests = a;

        kyu_sd_write_to_all_listeners
                (cons (sym_request, cons (sym_configuration,
                       cons (a->id, sx_end_of_list))),
                 (void *)0);

        state->environment = sx_end_of_list;
        state->stack = sx_end_of_list;
        state->code = sx_end_of_list;
        state->dump = sx_end_of_list;

        return sx_nonexistent;
    }
}

sexpr kyu_sc_message (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack =
                cons(lx_foreign_mu (sym_message, kyu_sc_message), state->stack);

        return sx_nonexistent;
    }
    else
    {
        kyu_sd_write_to_all_listeners
                (cons (sym_event, cons (sym_message, arguments)), (void *)0);

        return sx_true;
    }
}

sexpr kyu_sc_exit (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack =
                cons(lx_foreign_mu (sym_exit,
                                    kyu_sc_exit), state->stack);

        return sx_nonexistent;
    }
    else
    {
        sexpr ev = car (arguments);

        if (truep (ev))
        {
            cexit (0);
        }
        else if (falsep (ev))
        {
            cexit (1);
        }
        else
        {
            cexit (sx_integer (ev));
        }

        return ev; /* not reached... */
    }
}

sexpr kyu_sc_kill_subprocesses (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack =
                cons(lx_foreign_mu (sym_kill_subprocesses,
                                    kyu_sc_kill_subprocesses), state->stack);

        return sx_nonexistent;
    }
    else
    {
        sexpr rx = car (arguments);
        static struct memory_pool pool
                = MEMORY_POOL_INITIALISER (sizeof (termination_meta));
        struct termination_meta *m;

        if (falsep (sx_set_rx_memberp (running_processes, rx)))
        {
            return make_integer (0);
        }

        m = (struct termination_meta *)get_pool_mem (&pool);

        m->regex           = rx;
        m->continuation    = state->dump;
        m->next            = termination_meta;

        termination_meta   = m;

        kyu_command (cons (sym_terminate,
                           cons (native_system,
                                 cons (rx, sx_end_of_list))));

        state->stack       = sx_end_of_list;
        state->environment = sx_end_of_list;
        state->code        = sx_end_of_list;
        state->dump        = sx_end_of_list;

        return sx_nil;
    }
}

sexpr kyu_sc_out (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack =
                cons(lx_foreign_mu (sym_out,
                                    kyu_sc_out), state->stack);

        return sx_nonexistent;
    }
    else
    {
        sexpr src = lx_environment_lookup (state->environment, sym_source);

        if (nexp (src))
        {
            kyu_command (cons (sym_event,
                               cons (sym_out, cons (src, arguments))));
        }
        else
        {
            kyu_command (cons (sym_event, cons (sym_out, arguments)));
        }

        return sx_nil;
    }
}

