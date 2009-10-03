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
#include <curie/filesystem.h>

#include <syscall/syscall.h>

#include <kyuba/ipc.h>

static sexpr monitor_script = sx_end_of_list;
static unsigned int open_script_files = 0;
static sexpr global_environment;

static enum gstate {
    gs_none,
    gs_power_on,
    gs_power_reset,
    gs_power_down,
    gs_ctrl_alt_del
} init_state = gs_none;

static void change_state (enum gstate state)
{
    sexpr m = sx_nonexistent;

    if (init_state != state)
    {
        init_state = state;

        switch (state)
        {
            case gs_power_on:
                m = sym_power_on;
                break;
            case gs_power_down:
                m = sym_power_down;
                break;
            case gs_power_reset:
                m = sym_power_reset;
                break;
            case gs_ctrl_alt_del:
                m = sym_ctrl_alt_del;
                break;
            default:
                break;
        }

        kyu_sd_write_to_all_listeners (cons(sym_event, cons(m, sx_end_of_list)),
                                       (void *)0);

        (void)lx_eval (monitor_script, global_environment);
    }
}

void script_reading_finished ( void )
{
    open_script_files--;

    if (open_script_files == 0)
    {
        monitor_script = sx_reverse (monitor_script);
        change_state (gs_power_on);
    }
}

static void on_script_read(sexpr sx, struct sexpr_io *io, void *p)
{
    if (eofp (sx))
    {
        script_reading_finished ();
    }
    else
    {
        monitor_script = cons (sx, monitor_script);
    }
}

sexpr on_event (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        sexpr tstate = car (arguments);

        if (truep(equalp(tstate, sym_always)) ||
            ((init_state == gs_power_on) &&
                  truep(equalp(tstate, sym_power_on))) ||
            ((init_state == gs_power_down) &&
                  truep(equalp(tstate, sym_power_down))) ||
            ((init_state == gs_power_reset) &&
                  truep(equalp(tstate, sym_power_reset))) ||
            ((init_state == gs_ctrl_alt_del) &&
                  truep(equalp(tstate, sym_ctrl_alt_del))))
        {
            state->code = cdr (state->code);
            return sx_nonexistent;
                /* makes the seteh code execute the remainder of the script */
        }
        else
        {
            return sx_false;
            // wrong state
        }
    }
    else
    {
        return sx_true;
    }
}

static sexpr power_on (sexpr arguments, struct machine_state *state)
{
    change_state (gs_power_on);
    return sx_true;
}

static sexpr power_down (sexpr arguments, struct machine_state *state)
{
    change_state (gs_power_down);
    return sx_true;
}

static sexpr power_reset (sexpr arguments, struct machine_state *state)
{
    change_state (gs_power_reset);
    return sx_true;
}

static sexpr ctrl_alt_del (sexpr arguments, struct machine_state *state)
{
    change_state (gs_ctrl_alt_del);
    return sx_true;
}

static void on_ipc_read (sexpr sx)
{
    (void)lx_eval (sx, global_environment);
}

int cmain ()
{
    int i;

    terminate_on_allocation_errors();

#if defined(have_sys_setsid)
    sys_setsid();
#endif

    initialise_kyu_script_commands ();
    multiplex_kyu ();

    kyu_sd_on_read = on_ipc_read;

    global_environment = kyu_sx_default_environment ();

    global_environment =
            lx_environment_bind
                (global_environment, sym_on_event,
                 lx_foreign_mu (sym_on_event, on_event));
    global_environment =
            lx_environment_bind
                (global_environment, sym_power_on,
                 lx_foreign_mu (sym_power_on, power_on));
    global_environment =
            lx_environment_bind
                (global_environment, sym_power_down,
                 lx_foreign_mu (sym_power_down, power_down));
    global_environment =
            lx_environment_bind
                (global_environment, sym_power_reset,
                 lx_foreign_mu (sym_power_reset, power_reset));
    global_environment =
            lx_environment_bind
                (global_environment, sym_ctrl_alt_del,
                 lx_foreign_mu (sym_ctrl_alt_del, ctrl_alt_del));

    for (i = 1; curie_argv[i] != (char *)0; i++)
    {
        sexpr n = make_string (curie_argv[i]);

        if (filep(n))
        {
            open_script_files++;
            multiplex_add_sexpr
                    (sx_open_io (io_open_read (curie_argv[i]), io_open (-1)),
                     on_script_read, (void *)0);
        }
    }

    kyu_sd_add_listener (sx_open_stdio ());

    while (multiplex() == mx_ok);

    return 21;
}
