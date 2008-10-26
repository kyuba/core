/*
 *  kyuba-monitor.c
 *  kyuba
 *
 *  Created by Magnus Deininger on 23/10/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2008, Magnus Deininger All rights reserved.
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
        if (filep(curie_argv[i]))
        {
            multiplex_add_sexpr(sx_open_io (io_open_read (curie_argv[i]),
                                io_open (-1)),
                                          on_script_read,
                                          (void *)context);
        }

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
