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
#include <curie/graph.h>
#include <curie/filesystem.h>
#include <sievert/sexpr.h>
#include <sievert/shell.h>
#include <kyuba/ipc.h>
#include <kyuba/types.h>
#include <kyuba/system.h>

define_symbol (sym_initialise,           "initialise");
define_symbol (sym_server_seteh,         "server-seteh");
define_symbol (sym_server,               "server");
define_symbol (sym_binary_not_found,     "binary-not-found");
define_symbol (sym_schedule_limitations, "schedule-limitations");
define_symbol (sym_binary,               "binary");
define_symbol (sym_once_per_network,     "once-per-network");
define_symbol (sym_once_per_system,      "once-per-system");
define_symbol (sym_io_type,              "io-type");
define_symbol (sym_kyuba_ipc,            "kyuba-ipc");
define_symbol (sym_none,                 "none");
define_symbol (sym_pid_file,             "pid-file");
define_symbol (sym_parameters,           "parameters");

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

define_symbol (sym_enabling,             "enabling");
define_symbol (sym_enabled,              "enabled");
define_symbol (sym_disabling,            "disabling");
define_symbol (sym_action,               "action");
define_symbol (sym_blocked,              "blocked");

define_symbol (sym_action_wrap,          "action-wrap");
define_symbol (sym_action_dispatch,      "action-dispatch");
define_symbol (sym_warning,              "warning");

define_symbol (sym_module_has_no_functions,     "module-has-no-functions");
define_symbol (sym_module_action_not_available, "module-action-not-available");

static sexpr global_environment, my_modules, mod_functions, mod_metadata;
static void update_status_from_pid_files ( void );
static int open_config_files = 0;

static void on_script_file_read (sexpr sx, struct sexpr_io *io, void *p)
{
    if (consp (sx))
    {
        sexpr a = car (sx), mt, b, c;
        struct kyu_module *mo;
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
                  functiondata   = sx_end_of_list,
                  binary         = sx_end_of_list,
                  pidfile        = sx_end_of_list,
                  startcommand   = sx_end_of_list,
                  stopcommand    = sx_true,
                  parameters     = sx_end_of_list,
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
                    provides = sx_set_merge (provides, cdr (v));
                }
                else if (truep (equalp (sym_requires, va)))
                {
                    requires = sx_set_merge (requires, cdr (v));
                }
                else if (truep (equalp (sym_conflicts_with, va)))
                {
                    conflicts = sx_set_merge (conflicts, cdr (v));
                }
                else if (truep (equalp (sym_before, va)))
                {
                    before = sx_set_merge (before, cdr (v));
                }
                else if (truep (equalp (sym_after, va)))
                {
                    after = sx_set_merge (after, cdr (v));
                }
                else if (truep (equalp (sym_schedule_limitations, va)))
                {
                    schedulerflags = sx_set_merge (schedulerflags, cdr (v));
                }
                else if (truep (equalp (sym_functions, va)))
                {
                    functiondata = sx_set_merge (functiondata, cdr (v));
                }
                else if (truep (equalp (sym_pid_file, va)))
                {
                    pidfile = sx_set_merge (pidfile, cdr (v));
                }
                else if (truep (equalp (sym_binary, va)))
                {
                    binary = sx_set_merge (binary, cdr (v));
                }
                else if (truep (equalp (sym_parameters, va)))
                {
                    parameters = sx_set_merge (parameters, cdr (c));
                }

                a = cdr (a);
            }

            if (!eolp (binary))
            {
                for (a = binary; consp (a); a = cdr (a))
                {
                    b = car (a);

                    if (falsep ((c = which (b))))
                    {
                        kyu_command (cons (sym_warning,
                                       cons (sym_binary_not_found,
                                         cons (native_system,
                                           cons (name,
                                             cons (b,
                                                   sx_end_of_list))))));

                        return;
                    }

                    if (daemon)
                    {
                        startcommand =
                            cons (cons (sym_run, cons (c, parameters)),
                                  startcommand);

                        if (!eolp(pidfile) && truep(stopcommand))
                        {
                            stopcommand = sx_list1
                                (sx_list2(sym_kill_via_pid_file, pidfile));
                        }
                    }
                }
            }

            if (daemon)
            {
                functiondata = cons (cons (sym_start, startcommand),
                                     cons (cons (sym_stop, stopcommand),
                                           functiondata));
            }

            mt = lx_environment_lookup (my_modules, name);
            if (!nexp (mt))
            {
                mo = (struct kyu_module *)mt;

                schedulerflags =
                    sx_set_merge (schedulerflags, mo->schedulerflags);
            }

            module = kyu_make_module
                    (name, description, provides, requires, before, after,
                     conflicts, schedulerflags, functions);

            my_modules = lx_environment_unbind (my_modules, name);
            my_modules = lx_environment_bind   (my_modules, name, module);

            mod_functions =
                lx_environment_unbind (mod_functions, name);
            mod_functions =
                lx_environment_bind   (mod_functions, name, functiondata);

            mod_metadata =
                lx_environment_unbind (mod_metadata,  name);
            mod_metadata =
                lx_environment_bind   (mod_metadata,  name, cons (binary,
                                                                  pidfile));

            kyu_command (cons (sym_update, cons (native_system,
                         cons (module, sx_end_of_list))));
        }
    }
    else if (eofp (sx))
    {
        open_config_files--;
        if (open_config_files == 0)
        {
            update_status_from_pid_files ();

            kyu_command (cons (sym_initialised,
                         cons (sym_server_seteh, sx_end_of_list)));
        }
    }
}

static sexpr action_wrap
    (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack = cons(lx_foreign_mu (sym_action_wrap, action_wrap),
                            state->stack);
 
        state->stack = cons (car (state->code), state->stack);
        state->code  = cdr (state->code);

        return sx_nonexistent;
    }
    else
    {
        sexpr meta = car (arguments), v = sx_true, name = car (meta),
              act = cdr (meta), t, module;
        struct kyu_module *mod;

        t = lx_environment_lookup (my_modules, name);

        if (!kmodulep (t))
        {
            return sx_false;
        }

        mod = (struct kyu_module *)t;
        t = mod->schedulerflags;

        arguments = cdr (arguments);

        while (consp (arguments))
        {
            v = car (arguments);
            arguments = cdr (arguments);
        }

        if (truep (equalp (act, sym_start)))
        {
            t = sx_set_remove (t, sym_enabling); 

            if (truep (v))
            {
                t = sx_set_add (t, sym_enabled);
            }
            else
            {
                t = sx_set_add (t, sym_blocked);
            }
        }
        else if (truep (equalp (act, sym_stop)))
        {
            t = sx_set_remove (t, sym_enabled); 
            t = sx_set_remove (t, sym_disabling); 
        }
        else
        {
            t = sx_set_remove (t, cons (sym_action, act)); 
        }

        module = kyu_make_module
                (mod->name, mod->description, mod->provides, mod->requires,
                 mod->before, mod->after, mod->conflicts, t, mod->functions);

        my_modules = lx_environment_unbind (my_modules, name);
        my_modules = lx_environment_bind   (my_modules, name, module);

        kyu_command (cons (sym_update, cons (native_system,
                     cons (module, sx_end_of_list))));

        return sx_true;
    }
}

static sexpr action_dispatch
    (sexpr arguments, struct machine_state *state)
{
    if (eolp (state->stack))
    {
        state->stack = cons(lx_foreign_mu (sym_action_dispatch,
                                           action_dispatch),
                            state->stack);

        state->stack = cons (car (state->code), state->stack);
        state->code  = cdr (state->code);
        state->stack = cons (car (state->code), state->stack);
        state->code  = cdr (state->code);

        return sx_nonexistent;
    }
    else
    {
        sexpr meta = car (arguments), code, env;

        arguments = cdr (arguments);
        code = car (arguments);
        env = car (cdr (arguments));

        state->stack = sx_end_of_list;
        state->code  = cons (cons (sym_action_wrap, cons (meta, code)),
                             sx_end_of_list);

        if (environmentp (env))
        {
            state->environment = lx_environment_join (state->environment, env);
        }

        return sx_unquote;
    }
}

static sexpr handle_action
    (struct kyu_module *mod, sexpr action)
{
    sexpr act = lx_environment_lookup (mod_functions, mod->name), c, a, e;

    if (nexp (act))
    {
        kyu_command (cons (sym_warning, cons (sym_module_has_no_functions,
                     cons (mod->name, sx_end_of_list))));

        return sx_false;
    }

    for (c = act; consp (c); c = cdr (c))
    {
        a = car (c);

        if (truep (equalp (car (a), action)))
        {
            e = lx_environment_unbind (global_environment, sym_source);
            e = lx_environment_bind   (e,       sym_source, (sexpr)mod);
            lx_eval (cons (cons (sym_action_dispatch,
                              cons (cons (mod->name, action),
                                    cons (cdr (a),
                                    cons (cons (sym_get_configuration,
                                                cons (mod->name,
                                                      sx_end_of_list)),
                                          sx_end_of_list)))),
                           sx_end_of_list), e);

            return sx_true;
        }
    }

    kyu_command (cons (sym_warning, cons (sym_module_action_not_available,
                 cons (mod->name, sx_end_of_list))));

    return sx_false;
}

static sexpr handle_enable_request
    (struct kyu_module *mod)
{
    return handle_action (mod, sym_start);
}

static sexpr handle_disable_request
    (struct kyu_module *mod)
{
    return handle_action (mod, sym_stop);
}

static void handle_external_mod_update
    (struct kyu_module *newdef, struct kyu_module *mydef)
{
    sexpr c, a, module, rv = sx_nil, flags = newdef->schedulerflags;

    c = sx_set_difference (mydef->schedulerflags, newdef->schedulerflags);

    if (eolp (c))
    {
        return;
    }

    while (consp (c) && nilp (rv))
    {
        a = car (c);

        if (truep (equalp (a, sym_enabling)))
        {
            if (falsep (sx_set_memberp (mydef->schedulerflags, sym_enabling)))
            {
                rv = handle_enable_request (mydef);

                if (falsep (rv))
                {
                    flags = sx_set_add (mydef->schedulerflags, sym_blocked);
                }
            }
        }
        else if (truep (equalp (a, sym_disabling)))
        {
            if (falsep (sx_set_memberp (mydef->schedulerflags, sym_disabling)))
            {
                rv = handle_disable_request (mydef);
            }
        }
        else if (consp (a) &&
                 falsep (sx_set_memberp (mydef->schedulerflags, a)))
        {
            rv = handle_action (mydef, cdr (a));
        }

        c = cdr (c);
    }

    module = kyu_make_module
            (mydef->name, mydef->description, mydef->provides,
             mydef->requires, mydef->before, mydef->after, mydef->conflicts,
             flags, mydef->functions);

    my_modules = lx_environment_unbind (my_modules, mydef->name);
    my_modules = lx_environment_bind   (my_modules, mydef->name, module);

    kyu_command (cons (sym_update, cons (native_system,
                 cons (module, sx_end_of_list))));
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
        else if (truep (equalp (a, sym_update)))
        { /* this might be a request from the scheduler, let's have a look... */
            sx = cdr (sx);
            a  = car (sx);

            if (truep (equalp (a, native_system)))
            {
                sx = cdr (sx);

                while (consp (sx))
                {
                    a = car (sx);

                    if (kmodulep (a))
                    {
                        struct kyu_module *mod = (struct kyu_module *)a;

                        a = lx_environment_lookup (my_modules, mod->name);

                        if (!nexp (a))
                        { /* someone else updated one of our modules... */
                            struct kyu_module *mydef = (struct kyu_module *)a;

                            handle_external_mod_update (mod, mydef);
                        }
                    }

                    sx = cdr (sx);
                }
            }
        }
        else if (truep (equalp (a, sym_process_terminated)))
        {
            update_status_from_pid_files ();
        }
    }
}

static void read_configuration ()
{
    kyu_command (sx_list3 (sym_request, sym_configuration, sym_server_seteh));
}

static void update_status_from_pid_files ( void )
{
    sexpr c, a, n, pl, name, m, flags;
    struct sexpr_io *io;
    struct kyu_module *mod;
    int online;

    for (c = lx_environment_alist (mod_metadata); consp (c); c = cdr (c))
    {
        a = car (c);
        name = car (a);
        a = cdr (a);

        online = 0;

        if (consp ((pl = cdr (a))))
        {
            while (!online && (consp (pl)))
            {
                a = car (pl);

                if (truep (filep (a)))
                {
                    io = sx_open_i (io_open_read (sx_string (a)));

                    while (!eofp ((n = sx_read (io))))
                    {
                        if (integerp (n))
                        {
                            online = kyu_test_pid (sx_integer (n));
                            break;
                        }
                    }

                    sx_close_io (io);
                }

                pl = cdr (pl);
            }

            if (!nexp (m = lx_environment_lookup (my_modules, name)))
            {
                mod = (struct kyu_module *)m;

                flags = mod->schedulerflags;

                if (truep (sx_set_memberp (flags, sym_enabled))
                    != online)
                {
                    my_modules = lx_environment_unbind (my_modules, name);

                    if (online)
                    {
                        flags = sx_set_add (flags, sym_enabled);
                    }
                    else
                    {
                        flags = sx_set_remove (flags, sym_enabled);
                    }

                    m = kyu_make_module
                        (mod->name, mod->description, mod->provides,
                         mod->requires, mod->before, mod->after,
                         mod->conflicts, flags, mod->functions);

                    my_modules = lx_environment_bind (my_modules, name, m);

                    kyu_command (cons (sym_update, cons (native_system,
                                 cons (m, sx_end_of_list))));
                }
            }
        }
    }
}

int cmain ()
{
    programme_identification = cons (sym_server_seteh, make_integer (1));

    initialise_kyu_script_commands ();
    initialise_kyu_types ();
    multiplex_add_kyu_stdio (on_event, (void *)0);
    graph_initialise();

    global_environment = kyu_sx_default_environment ();
    global_environment =
        lx_environment_bind (global_environment, sym_action_wrap,
                             lx_foreign_mu (sym_action_wrap, action_wrap));
    global_environment =
        lx_environment_bind (global_environment, sym_action_dispatch,
                             lx_foreign_mu (sym_action_dispatch,
                                            action_dispatch));
    my_modules    = lx_make_environment (sx_end_of_list);
    mod_functions = lx_make_environment (sx_end_of_list);
    mod_metadata  = lx_make_environment (sx_end_of_list);

    read_configuration ();

    while (multiplex() == mx_ok);

    return 0;
}
