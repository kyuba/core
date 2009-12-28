/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2009, Kyuba Project Members
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
#include <curie/multiplex.h>
#include <curie/directory.h>
#include <seteh/lambda.h>
#include <kyuba/ipc.h>

define_symbol (sym_bind, "bind");

static sexpr data;

static void on_event (sexpr sx, void *aux)
{
    if (consp (sx))
    {
        sexpr a = car (sx);

        if (truep (equalp (a, sym_request)))
        {
            sx = cdr (sx);
            a = car (sx);

            if (truep (equalp (a, sym_configuration)))
            {
                a = car (cdr (sx));

                kyu_command (cons (sym_reply, cons (sym_configuration, cons (a,
                             cons (lx_environment_lookup (data, a),
                             sx_end_of_list)))));
            }
        }
    }
}

static void on_configuration_read (sexpr sx, struct sexpr_io *io, void *aux)
{
    if (consp (sx))
    {
        sexpr a = car (sx);

        if (truep (equalp (a, sym_bind)))
        {
            sx = cdr (sx);
            a  = car (sx);
            sx = cdr (sx);

            data = lx_environment_unbind (data, a);
            data = lx_environment_bind   (data, a, lx_make_environment (sx));
        }
    }
}

static void read_configuration ()
{
    int i = 1;

    while (curie_argv[i] != (char *)0)
    {
        sexpr files = read_directory (curie_argv[i]);

        while (consp (files))
        {
            sexpr fname = car (files);
            const char *n = sx_string (fname);
            struct sexpr_io *io = sx_open_i (io_open_read(n));

            multiplex_add_sexpr (io, on_configuration_read, (void *)0);

            files = cdr (files);
        }

        i++;
    }
}

int cmain ()
{
    define_symbol (sym_server_configuration, "server-configuration");

    programme_identification =
            cons (sym_server_configuration, make_integer (1));

    initialise_seteh ();
    multiplex_kyu();

    data = lx_make_environment (sx_end_of_list);

    read_configuration ();

    multiplex_add_kyu_stdio (on_event, (void *)0);

    while (multiplex() == mx_ok);

    return 0;
}
