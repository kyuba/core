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
#include <curie/multiplex.h>
#include <curie/memory.h>
#include <curie/directory.h>
#include <curie/graph.h>
#include <kyuba/ipc.h>
#include <kyuba/types.h>

define_symbol (sym_initialise,           "initialise");
define_symbol (sym_server_seteh,         "server-seteh");
define_symbol (sym_source,               "source");
define_symbol (sym_server,               "server");
define_symbol (sym_binary_not_found,     "binary-not-found");
define_symbol (sym_schedule_limitations, "schedule-limitations");
define_symbol (sym_binary_name,          "binary-name");
define_symbol (sym_once_per_network,     "once-per-network");
define_symbol (sym_once_per_system,      "once-per-system");
define_symbol (sym_io_type,              "io-type");
define_symbol (sym_kyuba_ipc,            "kyuba-ipc");
define_symbol (sym_none,                 "none");
define_symbol (sym_active,               "active");
define_symbol (sym_inactive,             "inactive");

static sexpr global_environment;

static void on_script_file_read (sexpr sx, struct sexpr_io *io, void *p)
{
    if (consp (sx))
    {
        sexpr a = car (sx);

        kyu_command (a);
    }
}

static void on_event (sexpr sx, void *aux)
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
                sx = cdr (sx);
                a  = car (sx);

                if (truep (equalp (a, sym_server_seteh)))
                {
                    sx = lx_environment_lookup (car (cdr (sx)), sym_source);

                    while (consp (sx))
                    {
                        sexpr files = read_directory_sx (car (sx));

                        while (consp (files))
                        {
                            sexpr t = car (files);

                            multiplex_add_sexpr
                                    (sx_open_i (io_open_read (sx_string (t))),
                                     on_script_file_read, (void *)0);

                            files = cdr (files);
                        }

                        sx = cdr (sx);
                    }
                }
            }
        }
    }
}

static void read_configuration ()
{
    kyu_command (cons (sym_request,
                       cons (sym_configuration,
                             cons (sym_server_seteh, sx_end_of_list))));
}

int cmain ()
{
    terminate_on_allocation_errors ();

    initialise_kyu_script_commands ();
    initialise_kyu_types ();
    multiplex_add_kyu_stdio (on_event, (void *)0);
    graph_initialise();

    global_environment = kyu_sx_default_environment ();

    read_configuration ();

    while (multiplex() == mx_ok);

    return 0;
}
