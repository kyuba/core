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
#include <curie/regex.h>
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
define_symbol (sym_missing_mode_data,    "missing-mode-data");
define_symbol (sym_reevaluating_mode,    "reevaluating-mode");
define_symbol (sym_enable,               "enable");
define_symbol (sym_disable,              "disable");
define_symbol (sym_merge,                "merge");
define_symbol (sym_mode_data,            "mode-data");
define_symbol (sym_enabled,              "enabled");
define_symbol (sym_disabled,             "disabled");
define_symbol (sym_enabling,             "enabling");
define_symbol (sym_disabling,            "disabling");

static sexpr system_data,
             current_mode         = sx_nonexistent,
             target_mode          = sx_nonexistent,
             target_mode_enable   = sx_end_of_list,
             target_mode_disable  = sx_end_of_list,
             target_mode_ipc      = sx_end_of_list,
             mode_specifications  = sx_nonexistent;
static int currently_initialising = 0;

static void merge_mode (sexpr mode);
static sexpr merge_lists (sexpr a, sexpr b);

static void update_module_state (sexpr system, sexpr module, sexpr state)
{
#warning update_module_state() is not yet implemented!
}

static void update_service_state (sexpr system, sexpr service, sexpr state)
{
#warning update_service_state() is not yet implemented!
}

static sexpr system_module_action (sexpr system, sexpr module, sexpr action)
{
#warning update_service_state() is not yet implemented!
}

static sexpr system_service_action (sexpr system, sexpr service, sexpr action)
{
#warning system_service_action() is not yet implemented!
    sexpr c = lx_environment_lookup (system_data, system),
          rv = sx_true;

    if (nexp (c))
    {
        return sx_false;
    }

    return rv;
}

static sexpr reschedule_get_enable (sexpr sx, sexpr *unresolved)
{
    sexpr rv = sx_end_of_list;
#warning reschedule_get_enable() is not yet implemented!
    return rv;
}

/* note to self: unresolved services are meaningless when disabling... */
static sexpr reschedule_get_disable (sexpr sx)
{
#warning reschedule_get_disable() is not yet implemented!
    return sx;
}

/* this function should gather a list of services whose status should get
 * modified according to the mode we're currently switching to.
 *
 * the algorithm is roughly as follows: start with each of the two primary lists
 * (target_mode_enable, and target_mode_disable), and put all services and
 * modules those match into separate lists. then merge all the requirements of
 * those items in, and recurse until no further changes occur. after that, sort
 * the two lists indivually according to the requirements, as well as before/
 * after attributes, until again no further changes occur. finally, enable the
 * first batch of items in the list of items that need to get enabled, that is
 * all of those that have no further requirements (or other constraints).
 * (disabling ought to work analoguous)
 *
 * whenever any of the modules or services changes state, this function gets
 * called again, which should enable both proper enabling and disabling
 * throughout.
 */
static void reschedule ( void )
{
    sexpr enable_unresolved,
          to_enable = reschedule_get_enable
              (target_mode_enable, &enable_unresolved),
          to_disable = reschedule_get_disable
              (target_mode_disable);
#warning reschedule() is not yet implemented!
}

/* the module list is taken as the primary listing, so this function is
 * supposed to update the list of services using the dependency information
 * from the list of modules.
 * as per the original spec file, each system gets their own namespace, so we
 * need to run through the list of systems to get at all modules */
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

static sexpr merge_lists (sexpr a, sexpr b)
{
    sexpr r = a;

    while (consp (b))
    {
        sexpr v = car (b), c = r;
        char have_item = (char)0;

        while (consp (c))
        {
            sexpr va = car (c);

            if (truep (equalp (v, va)))
            {
                have_item = (char)1;
                break;
            }

            c = cdr (c);
        }

        if (have_item == (char)0)
        {
            r = cons (v, r);
        }

        b = cdr (b);
    }

    return r;
}

static void merge_mode_data (sexpr mo)
{
    while (consp (mo))
    {
        sexpr a = car (mo), aa = car (a);

        if (truep (equalp (aa, sym_merge)))
        {
            a = cdr (a);

            while (consp (a))
            {
                merge_mode (car (a));

                a = cdr (a);
            }
        }
        else if (truep (equalp (aa, sym_enable)))
        {
            target_mode_enable  = merge_lists (target_mode_enable, cdr (a));
        }
        else if (truep (equalp (aa, sym_disable)))
        {
            target_mode_disable = merge_lists (target_mode_disable, cdr (a));
        }
        else
        {
            target_mode_ipc     = merge_lists (target_mode_ipc, cdr (a));
        }

        mo = cdr (mo);
    }
}

static void merge_mode (sexpr mode)
{
    sexpr mo = lx_environment_lookup (mode_specifications, mode);

    if (consp (mo))
    {
        merge_mode_data (mo);
    }
}

static sexpr compile_list (sexpr l)
{
    sexpr r = sx_end_of_list;

    while (consp (l))
    {
        sexpr la = car (l);

        r = cons (la, r);

        l = cdr (l);
    }

    return r;
}

/* look up the target mode and merge all enable/disable/ipc data */
static void rebuild_target_data ( void )
{
    sexpr mo = lx_environment_lookup (mode_specifications, target_mode);

    if (consp (mo))
    {
        target_mode_enable  = sx_end_of_list;
        target_mode_disable = sx_end_of_list;
        target_mode_ipc     = sx_end_of_list;

        merge_mode_data (mo);
    }
    else
    {
        kyu_command (cons (sym_error, cons (sym_missing_mode_data,
                     cons (target_mode, sx_end_of_list))));
    }

    target_mode_enable  = compile_list (target_mode_enable);
    target_mode_disable = compile_list (target_mode_disable);
}

static void reevaluate_target_data ( void )
{
    kyu_command (cons (sym_reevaluating_mode,
                       cons (target_mode, sx_end_of_list)));

    rebuild_target_data ();

    /* this is debug and will get ditched once it's stable: */

    kyu_command (cons (sym_mode_data,
                       cons (target_mode_enable,
                             cons (target_mode_disable,
                                   cons (target_mode_ipc, sx_end_of_list)))));

    reschedule ();

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

/* IPC entry point */
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
