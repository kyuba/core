/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2008-2010, Kyuba Project Members
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
#include <kyuba/ipc.h>
#include <kyuba/types.h>

define_symbol (sym_initialise,           "initialise");
define_symbol (sym_server_job,           "server-job");
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
define_symbol (sym_initialising,         "initialising");
define_symbol (sym_initialised,          "initialised");

static sexpr global_environment;
static sexpr binaries;
static int open_config_files = 0;

static void termination_cleanup ()
{
#warning job server still needs to kill clients on termination request
}

static void on_job_file_read (sexpr sx, struct sexpr_io *io, void *p)
{
    if (consp (sx))
    {
        sexpr a = car (sx);

        if (truep (equalp (a, sym_server)))
        {
            sexpr id;
            sexpr binary_name = sx_false;
            sexpr active = sx_false;
            sexpr c;
            sexpr kyuba_ipc = sx_true;

            sx = cdr (sx);
            id = car (sx);
            sx = cdr (sx);

            while (consp (sx))
            {
                a = car (sx);
                sx = cdr (sx);

                if (consp (a))
                {
                    sexpr b = car (a);

                    if (truep (equalp (b, sym_binary_name)))
                    {
                        binary_name = cdr (a);
                    }
                    else if (truep (equalp (b, sym_io_type)))
                    {
                        kyuba_ipc = equalp (sym_kyuba_ipc, cdr (a));
                    }
                }
                else if (truep (equalp (a, sym_active)))
                {
                    active = sx_true;
                }
                else if (truep (equalp (a, sym_inactive)))
                {
                    active = sx_false;
                }
            }

            if (truep (active) && stringp (binary_name))
            {
                c = lx_environment_lookup (binaries, id);
                if (!nexp (c))
                {
                    if (truep (equalp (c, binary_name)))
                    {
                        active = sx_false;
                    }
                    else
                    {
                        binaries = lx_environment_unbind (binaries, id);
                    }
                }
            }

            if (truep (active) && stringp (binary_name))
            {
                sexpr args;

                if (truep (kyuba_ipc))
                {
                    args = cons (binary_name, sx_end_of_list);
                }
                else
                {
                    args = cons (sx_false, cons (binary_name, sx_end_of_list));
                }

                if (falsep (kyu_sc_keep_alive
                                (args,
                                 (struct machine_state *)sx_end_of_list)))
                {
                    kyu_command
                            (cons (sym_event,
                                   cons (sym_error,
                                         cons (sym_binary_not_found,
                                               cons (binary_name,
                                                     sx_end_of_list)))));
                }
                else
                {
                    binaries = lx_environment_bind (binaries, id, binary_name);
                }
            }
        }
    }
    else if (eofp (sx))
    {
        open_config_files--;
        if (open_config_files == 0)
        {
            kyu_command (cons (sym_initialised,
                         cons (sym_server_job, sx_end_of_list)));
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

                if (truep (equalp (a, sym_server_job)))
                {
                    kyu_command (cons (sym_initialising,
                                 cons (sym_server_job, sx_end_of_list)));

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
                                     on_job_file_read, (void *)0);

                            files = cdr (files);
                        }

                        sx = cdr (sx);
                    }

                    if (open_config_files == 0)
                    {
                        kyu_command (cons (sym_initialised,
                                     cons (sym_server_job, sx_end_of_list)));
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
                             cons (sym_server_job, sx_end_of_list))));
}

int cmain ()
{
    programme_identification = cons (sym_server_job, make_integer (1));
    cleanup_on_termination_request = termination_cleanup;

    initialise_kyu_script_commands ();
    initialise_kyu_types ();
    multiplex_add_kyu_stdio (on_event, (void *)0);

    global_environment = kyu_sx_default_environment ();
    binaries           = lx_make_environment (sx_end_of_list);

    read_configuration ();

    while (multiplex() == mx_ok);

    return 0;
}
