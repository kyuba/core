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

static void on_script_read(sexpr sx, struct sexpr_io *io, void *p)
{
    sexpr context = (sexpr)p;

    script_enqueue (context, sx);

    sx_destroy (sx);
}

int cmain ()
{
    sexpr context = sx_end_of_list;
    sexpr environment = sx_end_of_list;
    sexpr environment_raw = sx_end_of_list;
    int i = 0;

    set_resize_mem_recovery_function(rm_recover);
    set_get_mem_recovery_function(gm_recover);

    stdio = sx_open_stdio();

    multiplex_all_processes();
    multiplex_sexpr();

    while (curie_environment[i] != (char *)0)
    {
        int y = 0;
        while ((curie_environment[i][y] != (char)0) &&
               (curie_environment[i][y] != (char)'=')) y++;

        if (curie_environment[i][y] != (char)0)
        {
            curie_environment[i][y] = 0;
            environment
                    = cons(cons(make_symbol(curie_environment[i]),
                                make_string((curie_environment[i]) + y + 1)),
                            environment);
            curie_environment[i][y] = '=';

            environment_raw
                    = cons(make_string(curie_environment[i]),
                           environment_raw);
        }

        i++;
    }

    context
        = cons (cons (make_symbol ("environment"), environment),
                cons(cons (make_symbol ("environment-raw"), environment_raw),
                     context));

    i = 1;
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

    while (multiplex() == mx_ok);

    return 21;
}
