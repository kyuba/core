/*
 *  script.c
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
#include <curie/sexpr.h>
#include <curie/multiplex.h>
#include <curie/memory.h>

#include <kyuba/script.h>

struct sexpr_io *stdio;

struct sqelement {
    sexpr sx;
    sexpr context;

    struct sqelement *next;
};

static char deferred = 0;

static struct sqelement *script_queue = (struct sqelement *)0;

static void sc_keep_alive(sexpr, sexpr);

#if 0
static sexpr lookup_symbol (sexpr context, sexpr key)
{
    sexpr cur = context;

    while (consp(cur))
    {
        sexpr sx_car = car(cur);

        if (consp(sx_car))
        {
            if (truep(equalp(car(sx_car), key)))
            {
                return cdr(sx_car);
            }
        }

        cur = cdr (cur);
    }

    return sx_nonexistent;
}
#endif

static void on_death (struct exec_context *ctx, void *u)
{
    free_exec_context (ctx);

    deferred = 0;
    script_dequeue();
}

static void on_death_respawn (struct exec_context *ctx, void *u)
{
    sexpr rs = (sexpr)u;
    sexpr context = car(rs);

    free_exec_context (ctx);

    sc_keep_alive (context,
                   cdr(rs));

    sx_destroy (rs);
}

static struct exec_context *sc_run_x(sexpr context, sexpr sx)
{
    sexpr cur = sx;
    unsigned int length = 0;

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
            x[i] = (char *)sx_string(car(cur));
            i++;
            cur = cdr(cur);
        }
        x[i] = (char *)0;

        proccontext = execute(EXEC_CALL_PURGE | EXEC_CALL_NO_IO |
                EXEC_CALL_CREATE_SESSION,
                x,
                curie_environment);

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

static void sc_run(sexpr context, sexpr sx)
{
    struct exec_context *c = sc_run_x (context, sx);

    deferred = 1;
    multiplex_add_process(c, on_death, (void *)0);
}

static void sc_keep_alive(sexpr context, sexpr sx)
{
    struct exec_context *c = sc_run_x (context, sx);

    sx_xref (context);
    sx_xref (sx);

    multiplex_add_process(c, on_death_respawn, (void *)cons(context, sx));
}

static void script_run(sexpr context, sexpr sx)
{
    static sexpr sym_run         = (sexpr)0;
    static sexpr sym_keep_alive  = (sexpr)0;
    static sexpr sym_exit        = (sexpr)0;

    if (sym_run == (sexpr)0)
    {
        sym_run         = make_symbol ("run");
        sym_keep_alive  = make_symbol ("keep-alive");
        sym_exit        = make_symbol ("exit");
    }

    if (consp(sx))
    {
        sexpr scar = car(sx);
        sexpr scdr = cdr(sx);

        if (truep(equalp(scar, sym_run)))
        {
            sc_run (context, scdr);
        } else if (truep(equalp(scar, sym_keep_alive)))
        {
            sc_keep_alive (context,
                           scdr);
        } else if (truep(equalp(scar, sym_exit)))
        {
            cexit (sx_integer(scdr));
        }
    }
}

void script_dequeue()
{
    if (deferred == 1) return;

    while ((script_queue != (struct sqelement *)0) &&
           (deferred == 0))
    {
        struct sqelement *e = script_queue;

        script_run (e->context, e->sx);

        sx_destroy (e->context);
        sx_destroy (e->sx);

        script_queue = script_queue->next;
        free_pool_mem (e);
    }
}

void script_enqueue(sexpr context, sexpr sx)
{
    static struct memory_pool pool
            = MEMORY_POOL_INITIALISER(sizeof(struct sqelement));

    if (deferred == 0)
    {
        script_run (context, sx);
    }
    else
    {
        struct sqelement *e = (struct sqelement *)get_pool_mem (&pool);

        sx_xref(context);
        sx_xref(sx);

        e->sx = sx;
        e->context = context;
        e->next = (struct sqelement *)0;

        if (script_queue == (struct sqelement *)0)
        {
            script_queue = e;
        }
        else
        {
            struct sqelement *cur = script_queue;
            while (cur->next != (struct sqelement *)0)
            {
                cur = cur->next;
            }

            cur->next = e;
        }
    }
}
