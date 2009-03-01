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

#include <kyuba/script.h>

struct sexpr_io *stdio;

static enum gstate {
    gs_power_on,
    gs_power_reset,
    gs_power_off,
    gs_ctrl_alt_del
} init_state = gs_power_on;

static void run_scripts();

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

static void do_dispatch_script (sexpr sx, sexpr context)
{
    static sexpr sym_power_reset     = (sexpr)0;
    static sexpr sym_power_off       = (sexpr)0;

    if (sym_power_reset == (sexpr)0)
    {
        sym_power_reset     = make_symbol ("power-reset");
        sym_power_off       = make_symbol ("power-off");
    }

    sexpr cur = sx;

    while (consp(cur))
    {
        sexpr t = car (cur);
        sexpr tcar = car(t);

        if (truep(equalp(tcar, sym_power_reset)))
        {
            init_state = gs_power_reset;
            run_scripts();
        }
        else if (truep(equalp(tcar, sym_power_off)))
        {
            init_state = gs_power_off;
            run_scripts();
        }
        else
        {
            script_enqueue (context, t);
        }

        cur = cdr(cur);
    }
}

static void do_dispatch_event (sexpr sx, sexpr context)
{
    static sexpr sym_ctrl_alt_del    = (sexpr)0;

    if (sym_ctrl_alt_del == (sexpr)0)
    {
        sym_ctrl_alt_del    = make_symbol ("ctrl-alt-del");
    }

    if (truep(equalp(car(sx), sym_ctrl_alt_del)))
    {
        init_state = gs_ctrl_alt_del;
        run_scripts();
    }
}

static void dispatch_script (sexpr sx, sexpr context)
{
    static sexpr sym_on_ctrl_alt_del = (sexpr)0;
    static sexpr sym_on_power_on     = (sexpr)0;
    static sexpr sym_on_power_reset  = (sexpr)0;
    static sexpr sym_on_power_off    = (sexpr)0;
    static sexpr sym_always          = (sexpr)0;
    static sexpr sym_event           = (sexpr)0;

    if (sym_on_ctrl_alt_del == (sexpr)0)
    {
        sym_on_ctrl_alt_del = make_symbol ("on-ctrl-alt-del");
        sym_on_power_on     = make_symbol ("on-power-on");
        sym_on_power_reset  = make_symbol ("on-power-reset");
        sym_on_power_off    = make_symbol ("on-power-off");
        sym_always          = make_symbol ("always");
        sym_event           = make_symbol ("event");
    }

    if (consp (sx))
    {
        sexpr ccar = car(sx);

        if (truep(equalp(ccar, sym_event)))
        {
            do_dispatch_event (cdr(sx), context);
        } else if (truep(equalp(ccar, sym_always)) ||
            ((init_state == gs_power_on) && truep(equalp(ccar, sym_on_power_on))) ||
            ((init_state == gs_power_off) && truep(equalp(ccar, sym_on_power_off))) ||
            ((init_state == gs_power_reset) && truep(equalp(ccar, sym_on_power_reset))) ||
            ((init_state == gs_ctrl_alt_del) && truep(equalp(ccar, sym_on_ctrl_alt_del))))
        {
            do_dispatch_script (cdr(sx), context);
        }
    }
}

static void on_script_read(sexpr sx, struct sexpr_io *io, void *p)
{
    dispatch_script (sx, (sexpr)p);
    sx_destroy (sx);
}

static void run_scripts()
{
    sexpr context = sx_end_of_list;
    int i = 1;

    while (curie_argv[i] != (char *)0)
    {
        sexpr n = make_string (curie_argv[i]);

        if (filep(n))
        {
            multiplex_add_sexpr(sx_open_io (io_open_read (curie_argv[i]),
                                io_open (-1)),
                                          on_script_read,
                                          (void *)context);
        }

        sx_destroy (n);

        i++;
    }
}

static void on_stdio_read(sexpr sx, struct sexpr_io *io, void *p)
{
    on_script_read(sx, io, p);
}

int cmain ()
{
    sexpr context = sx_end_of_list;

    set_resize_mem_recovery_function(rm_recover);
    set_get_mem_recovery_function(gm_recover);

    multiplex_all_processes();
    multiplex_sexpr();

    stdio = sx_open_stdio();

    run_scripts();

    multiplex_add_sexpr(stdio, on_stdio_read, (void *)context);

    while (multiplex() == mx_ok);

    return 21;
}