/*
 *  kyuba-init.c
 *  kyuba
 *
 *  Created by Magnus Deininger on 26/10/2008.
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
#include <curie/memory.h>
#include <curie/exec.h>
#include <curie/multiplex.h>
#include <curie/signal.h>

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

static char **commandline;
struct sexpr_io *monitorconnection = (struct sexpr_io *)0;

static void on_conn_read(sexpr sx, struct sexpr_io *io, void *p)
{
    sx_destroy (sx);
}

enum signal_callback_result on_sig_int (enum signal signal, void *u)
{
    static sexpr msg = (sexpr)0;

    if (msg == (sexpr)0)
    {
        msg = cons (make_symbol("event"),
                    cons(make_symbol("ctrl-alt-del"), sx_end_of_list));
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
            commandline,
            curie_environment);

    switch (context->pid)
    {
        case 0:
        case -1:
            cexit(25);
        default:
            monitorconnection = sx_open_io (context->in, context->out);

            multiplex_add_sexpr (monitorconnection, on_conn_read, (void *)0);
            multiplex_add_process(context, on_init_death, (void *)0);
            break;
    }
}

int cmain ()
{
    static char *cmd[]
            = { "/lib/kyu/bin/monitor", "/etc/kyu/init.sx", (void *)0 };

    commandline = cmd;

    set_resize_mem_recovery_function(rm_recover);
    set_get_mem_recovery_function(gm_recover);

    multiplex_signal();
    multiplex_all_processes();
    multiplex_sexpr();

    on_init_death((void *)0, (void *)0);

    multiplex_add_signal(sig_int, on_sig_int, (void *)0);

    while (multiplex() == mx_ok);

    return 0;
}
