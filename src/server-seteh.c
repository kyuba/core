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

define_symbol (sym_init_script,          "init-script");
define_symbol (sym_daemon,               "daemon");
define_symbol (sym_update,               "update");

define_symbol (sym_provides,             "provides");
define_symbol (sym_requires,             "requires");
define_symbol (sym_before,               "before");
define_symbol (sym_after,                "after");
define_symbol (sym_conflicts_with,       "conflicts-with");
define_symbol (sym_functions,            "functions");

define_symbol (sym_start,                "start");
define_symbol (sym_stop,                 "stop");

define_symbol (sym_initialising,         "initialising");
define_symbol (sym_initialised,          "initialised");

static sexpr global_environment;
static int open_config_files = 0;

static void on_script_file_read (sexpr sx, struct sexpr_io *io, void *p)
{
    if (consp (sx))
    {
        sexpr a = car (sx);
        char daemon = 0;

        if (truep (equalp (sym_init_script, a)) ||
            (daemon = truep (equalp (sym_daemon, a))))
        {
            sexpr name           = sx_nonexistent,
                  description    = sx_nonexistent,
                  provides       = sx_end_of_list,
                  requires       = sx_end_of_list,
                  before         = sx_end_of_list,
                  after          = sx_end_of_list,
                  conflicts      = sx_end_of_list,
                  schedulerflags = sx_end_of_list,
                  functions      = sx_end_of_list,
                  module;

            if (daemon)
            {
                functions = cons (sym_stop, cons (sym_start, functions));
            }

            a = cdr (sx);
            name        = car (a); a = cdr (a);
            description = car (a); a = cdr (a);

            while (consp (a))
            {
                sexpr v  = car (a);
                sexpr va = car (v);

                if (truep (equalp (sym_provides, va)))
                {
                    provides = cdr (v);
                }
                else if (truep (equalp (sym_requires, va)))
                {
                    requires = cdr (v);
                }
                else if (truep (equalp (sym_conflicts_with, va)))
                {
                    conflicts = cdr (v);
                }
                else if (truep (equalp (sym_before, va)))
                {
                    before = cdr (v);
                }
                else if (truep (equalp (sym_after, va)))
                {
                    after = cdr (v);
                }
                else if (truep (equalp (sym_schedule_limitations, va)))
                {
                    schedulerflags = cdr (v);
                }

                a = cdr (a);
            }

            module = kyu_make_module
                    (name, description, provides, requires, before, after,
                     conflicts, schedulerflags, functions);

            kyu_command (cons (sym_update, cons (native_system,
                         cons (module, sx_end_of_list))));
        }
    }
    else if (eofp (sx))
    {
        open_config_files--;
        if (open_config_files == 0)
        {
            kyu_command (cons (sym_initialised,
                         cons (sym_server_seteh, sx_end_of_list)));
        }
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
                    kyu_command (cons (sym_initialising,
                                 cons (sym_server_seteh, sx_end_of_list)));

                    sx = lx_environment_lookup (car (cdr (sx)), sym_source);

                    while (consp (sx))
                    {
                        sexpr files = read_directory_sx (car (sx));

                        while (consp (files))
                        {
                            sexpr t = car (files);

                            open_config_files++;

                            multiplex_add_sexpr
                                    (sx_open_i (io_open_read (sx_string (t))),
                                     on_script_file_read, (void *)0);

                            files = cdr (files);
                        }

                        sx = cdr (sx);
                    }

                    if (open_config_files == 0)
                    {
                        kyu_command (cons (sym_initialised,
                                     cons (sym_server_seteh, sx_end_of_list)));
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

    programme_identification = cons (sym_server_seteh, make_integer (1));

    initialise_kyu_script_commands ();
    initialise_kyu_types ();
    multiplex_add_kyu_stdio (on_event, (void *)0);
    graph_initialise();

    global_environment = kyu_sx_default_environment ();

    read_configuration ();

    while (multiplex() == mx_ok);

    return 0;
}
