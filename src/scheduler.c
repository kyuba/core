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
#include <kyuba/ipc.h>
#include <kyuba/types.h>

define_symbol (sym_scheduler,            "scheduler");
define_symbol (sym_modes,                "modes");
define_symbol (sym_get_mode,             "get-mode");
define_symbol (sym_mode,                 "mode");
define_symbol (sym_update,               "update");
define_symbol (sym_switch,               "switch");
define_symbol (sym_switching_to,         "switching-to");
define_symbol (sym_initialising,         "initialising");
define_symbol (sym_initialised,          "initialised");
define_symbol (sym_get_scheduler_data,   "get-scheduler-data");
define_symbol (sym_default,              "default");
define_symbol (sym_no_data_for_mode,     "no-data-for-mode");
define_symbol (sym_reevaluating_mode,    "reevaluating-mode");

static sexpr system_data,
             current_mode         = sx_nonexistent,
             target_mode          = sx_nonexistent,
             target_mode_enable   = sx_end_of_list,
             target_mode_disable  = sx_end_of_list,
             target_mode_ipc      = sx_end_of_list,
             mode_specifications  = sx_nonexistent;
static int currently_initialising = 0;

/* the module list is taken as the primary listing, so this function is
   supposed to update the list of services using the dependency information
   from the list of modules.
   as per the original spec file, each system gets their own namespace, so we
   need to run through the list of systems to get at all modules */
static void update_services ( void )
{
    define_string (sym_c_s, ", ");
    sexpr c = lx_environment_alist (system_data);
    system_data = lx_make_environment (sx_end_of_list);

    while (consp (c))
    {
        sexpr a = car (c), sysname = car (a), sys = cdr (a);
        struct kyu_system *s = (struct kyu_system *)sys;
        sexpr services = sx_end_of_list, mo = s->modules;

        while (consp (mo))
        {
            sexpr s_mo = car (mo);
            struct kyu_module *m = (struct kyu_module *)s_mo;
            sexpr p = m->provides;

            while (consp (p))
            {
                sexpr sm = car (p);

                if (stringp (sm))
                {
                    sexpr sc = services, nservices = sx_end_of_list;
                    char have_service = (char)0;

                    while (consp (sc))
                    {
                        sexpr s_se = car (sc);
                        struct kyu_service *sv = (struct kyu_service *)s_se;

                        if ((have_service == (char)0) &&
                            truep (equalp (sm, sv->name)))
                        {
                            sexpr desc = sv->description,
                                  flags = sv->schedulerflags,
                                  mods = sv->modules,
                                  f = m->schedulerflags;

                            if (falsep (equalp (desc, m->description)))
                            {
                                desc = sx_join (desc, sym_c_s, m->description);
                            }

                            while (consp (f))
                            {
                                sexpr fa = car (f), fc = flags;
                                char fh = (char)0;

                                while (consp (fc))
                                {
                                    sexpr fca = car (fc);

                                    if (truep (equalp (fc, fca)))
                                    {
                                        fh = (char)1;
                                    }

                                    fc = cdr (fc);
                                }

                                if (fh == (char)0)
                                {
                                    flags = cons (fa, flags);
                                }

                                f = cdr (f);
                            }

                            mods = cons (s_se, mods);

                            nservices = cons
                                (kyu_make_service (sm, desc, flags, mods),
                                 nservices);

                            have_service = (char)1;
                        }
                        else
                        {
                            nservices = cons (s_se, nservices);
                        }

                        sc = cdr (sc);
                    }

                    services = nservices;

                    if (have_service == (char)0)
                    {
                        services = cons
                            (kyu_make_service (sm, m->description,
                                               m->schedulerflags,
                                               cons (s_mo, sx_end_of_list)),
                             services);
                    }
                }

                p = cdr (p);
            }

            mo = cdr (mo);
        }

        system_data = lx_environment_bind
                (system_data, sysname,
                 kyu_make_system (sysname, s->description, s->location,
                                  s->schedulerflags, s->modules, services));

        c = cdr (c);
    }
}

static void print_scheduler_data ( void )
{
    sexpr sys = lx_environment_alist (system_data);

    while (consp (sys))
    {
        sexpr sc = car (sys), sd = cdr (sc);

        kyu_command (cons (sym_update, cons (sd, sx_end_of_list)));

        sys = cdr (sys);
    }
}

static void rebuild_target_data ( void )
{
    /* TODO: look up the target mode and merge all enable/disable/ipc data */
}

static void reevaluate_target_data ( void )
{
    kyu_command (cons (sym_reevaluating_mode,
                       cons (target_mode, sx_end_of_list)));

    rebuild_target_data ();

    /* TODO: do actual mode switching here */

    /* TODO: send this once the new mode is active: */
/*    current_mode = mode;
    kyu_command (cons (sym_mode, cons (mode, sx_end_of_list)));*/
}

static void switch_mode (sexpr mode)
{
    kyu_command (cons (sym_switching_to, cons (mode, sx_end_of_list)));
    target_mode = mode;

    reevaluate_target_data ();
}

static void on_event (sexpr sx, void *aux)
{
    if (consp (sx))
    {
        sexpr a = car (sx);

        if (truep (equalp (a, sym_update)))
        {
            sx = cdr (sx);
            a  = car (sx);

            if (symbolp (a))
            {
                sexpr target_system = a;
                sexpr name;

                sx = cdr (sx);
                a  = car (sx);

                if (kmodulep (a))
                {
                    sexpr ts, ts_name, ts_description, ts_location,
                          ts_schedulerflags, ts_modules, ts_services, c;

                    name = ((struct kyu_module *)a)->name;

                    ts = lx_environment_lookup (system_data, target_system);

                    if (!nexp (ts))
                    {
                        struct kyu_system *s = (struct kyu_system *)ts;

                        ts_name           = s->name;
                        ts_description    = s->description;
                        ts_location       = s->location;
                        ts_schedulerflags = s->schedulerflags;
                        ts_modules        = s->modules;
                        ts_services       = s->services;

                        system_data =
                            lx_environment_unbind (system_data, target_system);
                    }
                    else
                    {
                        ts_name           = target_system;
                        ts_description    = sx_nil;
                        ts_location       = sx_nil;
                        ts_schedulerflags = sx_nil;
                        ts_modules        = sx_end_of_list;
                        ts_services       = sx_end_of_list;
                    }

                    c = ts_modules;
                    ts_modules = cons (a, sx_end_of_list);
                    while (consp (c))
                    {
                        sexpr m = car (c);
                        if (falsep (equalp (((struct kyu_module *)m)->name,
                                            name)))
                        {
                            ts_modules = cons (m, ts_modules);
                        }

                        c = cdr (c);
                    }

                    ts = kyu_make_system
                            (ts_name, ts_description, ts_location,
                             ts_schedulerflags, ts_modules, ts_services);

                    system_data = lx_environment_bind
                            (system_data, target_system, ts);
                }
            }
            else if (ksystemp (a))
            {
                struct kyu_system *s = (struct kyu_system *)a;
                sexpr name = s->name;
                system_data = lx_environment_unbind (system_data, name);
                system_data = lx_environment_bind   (system_data, name, a);
            }
        }
        else if (truep (equalp (a, sym_initialising)))
        {
            currently_initialising++;
        }
        else if (truep (equalp (a, sym_initialised)))
        {
            if (currently_initialising > 0)
            {
                currently_initialising--;
            }

            if (currently_initialising == 0)
            {
                update_services();
                print_scheduler_data ();
                reevaluate_target_data ();
            }
        }
        else if (truep (equalp (a, sym_get_scheduler_data)))
        {
            print_scheduler_data ();
        }
        else if (truep (equalp (a, sym_reply)))
        {
            sx = cdr (sx);
            a = car (sx);

            if (truep (equalp (a, sym_configuration)))
            {
                sx = cdr (sx);
                a = car (sx);

                if (truep (equalp (a, sym_modes)))
                {
                    a = car (cdr (sx));

                    if (environmentp (a))
                    {
                        mode_specifications = a;

                        reevaluate_target_data ();
                    }
                }
            }
        }
        else if (truep (equalp (a, sym_get_mode)))
        {
            kyu_command (cons (sym_mode, cons (current_mode, sx_end_of_list)));
        }
        else if (truep (equalp (a, sym_switch)))
        {
            switch_mode (car (cdr (sx)));
        }
    }
}

static void read_configuration ()
{
        kyu_command (cons (sym_request,
                           cons (sym_configuration,
                                 cons (sym_modes, sx_end_of_list))));
}

int cmain ()
{
    terminate_on_allocation_errors ();

    programme_identification = cons (sym_scheduler, make_integer (1));

    system_data = lx_make_environment (sx_end_of_list);

    initialise_kyu_script_commands ();
    initialise_kyu_types ();
    multiplex_add_kyu_stdio (on_event, (void *)0);

    read_configuration ();
    switch_mode (sym_default);

    while (multiplex() == mx_ok);

    return 0;
}
