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

#include <kyuba/script.h>

struct sexpr_io *stdio;

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

static void on_death (struct exec_context *context, void *u) { }

static void sc_run(sexpr context, sexpr environment, sexpr sx)
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
        struct exec_context *context;

        cur = sx;
        while (consp(cur))
        {
            x[i] = (char *)sx_string(car(cur));
            i++;
            cur = cdr(cur);
        }
        x[i] = (char *)0;

        cur = environment;
        length = 0;
        while (consp(cur))
        {
            length++;
            cur = cdr(cur);
        }

        char *env[(length + 1)];
        cur = environment;
        i = 0;
        while (consp(cur))
        {
            env[i] = (char *)sx_string(car(cur));
            i++;
            cur = cdr(cur);
        }
        env[i] = (char *)0;

        context = execute(EXEC_CALL_PURGE | EXEC_CALL_NO_IO |
                          EXEC_CALL_CREATE_SESSION,
                          x,
                          env);

        switch (context->pid)
        {
            case 0:
            case -1:
                cexit(25);
            default:
                multiplex_add_process(context, on_death, (void *)0);
                break;
        }
    }
}

void script_run(sexpr context, sexpr sx)
{
    static sexpr sym_run         = (sexpr)0;
    static sexpr sym_environment = (sexpr)0;

    if (sym_run == (sexpr)0)
    {
        sym_run         = make_symbol ("run");
        sym_environment = make_symbol ("environment-raw");
    }

    if (consp(sx))
    {
        sexpr scar = car(sx);
        sexpr scdr = cdr(sx);

        if (truep(equalp(scar, sym_run)))
        {
            sc_run (context, lookup_symbol(context, sym_environment), scdr);
        }
    }
}
