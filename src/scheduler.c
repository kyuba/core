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
#include <curie/regex.h>
#include <kyuba/ipc.h>
#include <kyuba/types.h>

#include <sievert/sexpr.h>

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
define_symbol (sym_enabling,             "enabling");
define_symbol (sym_disabling,            "disabling");
define_symbol (sym_unresolved,           "unresolved");
define_symbol (sym_blocked,              "blocked");
define_symbol (sym_action,               "action");
define_symbol (sym_any_rx,               ".*");
define_symbol (sym_extra_scheduler_data, "extra-scheduler-data");
define_symbol (sym_affinity,             "affinity");
define_symbol (sym_implicit_affinity_multipliers,
                                         "implicit-affinity-multipliers");
define_symbol (sym_stable,               "stable");
define_symbol (sym_experimental,         "experimental");
define_symbol (sym_deprecated,           "deprecated");
define_symbol (sym_problematic,          "problematic");

static sexpr system_data,
             current_mode           = sx_nonexistent,
             target_mode            = sx_nonexistent,
             target_mode_enable     = sx_end_of_list,
             target_mode_disable    = sx_end_of_list,
             target_mode_ipc        = sx_end_of_list,
             target_extra_enable    = sx_end_of_list,
             target_extra_disable   = sx_end_of_list,
             mode_specifications    = sx_nonexistent;
static int currently_initialising   = 0;
static int ia_m_in_mode             = 1;
static int ia_m_num_of_provides     = 1;
static int ia_m_num_of_requirements = 1;
static int ia_m_deprecated          = 1;
static int ia_m_stable              = 1;
static int ia_m_experimental        = 1;
static int ia_m_problematic         = 1;
static sexpr affinities             = sx_end_of_list;

static void merge_mode (sexpr mode);
static sexpr compile_list (sexpr l);
static void update_services ( void );

/* return #t if s is currently in some kind of status transformation */
static sexpr statusp (sexpr s)
{
    sexpr f = sx_end_of_list, fa;
 
    if (consp (s))
    {
        s = car (s);
    }

    if (kmodulep (s))
    {
        struct kyu_module *m = (struct kyu_module *)s;
        f = m->schedulerflags;
    }
    else if (kservicep (s))
    {
        struct kyu_service *sv = (struct kyu_service *)s;
        f = sv->schedulerflags;
    }

    while (consp (f))
    {
        fa = car (f);

        if (truep (equalp (fa, sym_enabling)) ||
            truep (equalp (fa, sym_disabling)) ||
            (consp (fa) && truep (equalp (car(fa), sym_action))))
        {
            return sx_true;
        }

        f = cdr (f);
    }

    return sx_false;
}

/* test if a module or service has a specific flag */
static sexpr flagp (sexpr s, sexpr flag)
{
    sexpr f = sx_end_of_list;

    if (consp (s))
    {
        s = car (s);
    }
        
    if (kmodulep (s))
    {
        struct kyu_module *m = (struct kyu_module *)s;
        f = m->schedulerflags;
    }
    else if (kservicep (s))
    {
        struct kyu_service *sv = (struct kyu_service *)s;
        f = sv->schedulerflags;
    }

    return sx_set_memberp (f, flag);
}

static void system_module_action (sexpr system, sexpr module, sexpr action)
{
    struct kyu_module *m = (struct kyu_module *)module;
    sexpr f = m->schedulerflags;
    sexpr s = lx_environment_lookup (system_data, system);
    struct kyu_system *sys = (struct kyu_system *)s;
    sexpr ml = sx_set_remove (sys->modules, module), mo;
    sexpr fc, fa, fr = sx_end_of_list;

    for (fc = f; consp (fc); fc = cdr(fc))
    {
        fa = car (fc);

        if (truep (equalp (fa, sym_enabling)) ||
            truep (equalp (fa, sym_disabling)) ||
            (consp (fa) && truep (equalp (car(fa), sym_action))))
        {
            continue;
        }

        fr = cons (fa, fr);
    }

    f = fr;

    if (truep (equalp (action, sym_enable)))
    {
        f = sx_set_add (f, sym_enabling);
    }
    else if (truep (equalp (action, sym_disable)))
    {
        f = sx_set_add (f, sym_disabling);
    }
    else
    {
        f = sx_set_add (f, cons (sym_action, action));
    }

    mo = kyu_make_module (m->name, m->description, m->provides, m->requires,
                              m->before, m->after, m->conflicts, f,
                              m->functions);
    ml = sx_set_add (ml, mo);

    system_data = lx_environment_unbind (system_data, system);
    system_data = lx_environment_bind (system_data, system,
        kyu_make_system (sys->name, sys->description, sys->location,
                         sys->schedulerflags, ml, sys->services));

    update_services ();

    kyu_command (cons (sym_update, cons (system, cons (mo, sx_end_of_list))));
}

static void system_service_action (sexpr system, sexpr service, sexpr action)
{
    struct kyu_service *s = (struct kyu_service *)service;
    struct kyu_module *mo;
    sexpr m = s->modules;

    /*! \todo another great place to decide on greediness of the scheduler... */

    while (consp (m))
    {
        mo = (struct kyu_module *)car (m);

        if ((truep (sx_set_memberp (mo->schedulerflags, sym_enabling)) ||
             truep (sx_set_memberp (mo->schedulerflags, sym_disabling)) ||
             truep (sx_set_memberp (mo->schedulerflags, sym_enabled))))
        {
            system_module_action (system, (sexpr)mo, action);
            return;
        }

        m = cdr (m);
    }

    m = s->modules;

    while (consp (m))
    {
        mo = (struct kyu_module *)car (m);

        if (falsep (sx_set_memberp (mo->schedulerflags, sym_blocked)))
        {
            system_module_action (system, (sexpr)mo, action);
            return;
        }

        m = cdr (m);
    }

    /* this should never have been reached... */
    system_module_action (system, car (s->modules), action);
}

/* this just greedily grabs a list of matching modules and services... */
static sexpr reschedule_get_enable (sexpr sx, sexpr *unresolved)
{
    sexpr rv = sx_end_of_list, c, a, ca, sa, sd, sdd,
          sys = lx_environment_alist (system_data), ures = sx;

    while (consp (sys))
    {
        ca = car (sys);
        sa = car (ca);
        sd = cdr (ca);

        if (ksystemp (sd))
        {
            struct kyu_system *sp = (struct kyu_system *)sd;

            for (sd = sp->modules; consp (sd); sd = cdr (sd))
            {
                struct kyu_module *m = (struct kyu_module *)car (sd);

                for (c = sx; consp (c); c = cdr (c))
                {
                    a = car (c);
                    sdd = cdr (a);

                    if (symbolp (sdd) && truep (rx_match_sx (car(a), sa)) &&
                        truep (equalp (sdd, m->name)))
                    {
                        rv = cons (cons ((sexpr)m, sa), rv);
                        ures = sx_set_remove (ures, a);
                    }
                }
            }

            for (sd = sp->services; consp (sd); sd = cdr (sd))
            {
                struct kyu_service *s = (struct kyu_service *)car (sd);

                for (c = sx; consp (c); c = cdr (c))
                {
                    a = car (c);
                    sdd = cdr (a);

                    if (consp (sdd) && truep (rx_match_sx (car(a), sa)) &&
                        truep (rx_match_sx (cdr (sdd), s->name)))
                    {
                        rv = cons (cons ((sexpr)s, sa), rv);
                        ures = sx_set_remove (ures, a);
                    }
                }
            }
        }

        sys = cdr (sys);
    }

    while (consp (ures))
    {
        ca = car (ures);
        sd = cdr (ca);

        *unresolved = cons (car (sd), *unresolved);

        ures = cdr (ures);
    }

    return rv;
}

/* note to self: unresolved services are meaningless when disabling... */
static sexpr reschedule_get_disable (sexpr sx)
{
    sexpr rv = sx_end_of_list, c, a, ca, sa, sd, sdd,
          sys = lx_environment_alist (system_data);

    while (consp (sys))
    {
        ca = car (sys);
        sa = car (ca);
        sd = cdr (ca);

        if (ksystemp (sd))
        {
            struct kyu_system *sp = (struct kyu_system *)sd;

            for (sd = sp->modules; consp (sd); sd = cdr (sd))
            {
                struct kyu_module *m = (struct kyu_module *)car (sd);

                for (c = sx; consp (c); c = cdr (c))
                {
                    a = car (c);
                    sdd = cdr (a);

                    if (symbolp (sdd) && truep (rx_match_sx (car(a), sa)) &&
                        truep (rx_match_sx (sdd, m->name)))
                    {
                        rv = cons (cons ((sexpr)m, sa), rv);
                    }
                }
            }

            for (sd = sp->services; consp (sd); sd = cdr (sd))
            {
                struct kyu_service *s = (struct kyu_service *)car (sd);

                for (c = sx; consp (c); c = cdr (c))
                {
                    a = car (c);
                    sdd = cdr (a);

                    if (consp (sdd) && truep (rx_match_sx (car(a), sa)) &&
                        truep (rx_match_sx (cdr (sdd), s->name)))
                    {
                        rv = cons (cons ((sexpr)s, sa), rv);
                    }
                }
            }
        }

        sys = cdr (sys);
    }

    return rv;
}

/* returns #t if the requirments have already been met, (#nonexistent list) if
 * they cannot be met and a list of requirements if the requirements have not
 * been met yet, but they can be met; finally, returns #f if not only the
 * requirments have been met but that whatever was passed as this function's
 * parameter is already enabled. */
static sexpr requirements (sexpr sx)
{
    if (consp (sx))
    {
        return requirements (car (sx));
    }
    else if (kservicep (sx))
    {
        struct kyu_service *s = (struct kyu_service *)sx;
        sexpr c = s->modules, br = sx_nonexistent, r;

        if (truep (sx_set_memberp (s->schedulerflags, sym_enabled)))
        {
            return sx_false;
        }

        while (consp (c))
        {
            r = requirements (car (c));

            /*! \todo: this ought to be the best spot to add some
             * configurability about whether the scheduler should operate
             * greedily or not... */

            if (truep (r))
            {
                return sx_true;
            }
            else if (consp (r))
            {
                if ((nexp (br) || nexp (car (br))) || !nexp (car (r)))
                {
                    br = r;
                }
            }

            c = cdr (c);
        }

        return br;
    }
    else if (kmodulep (sx))
    {
        struct kyu_module *m = (struct kyu_module *)sx;
        sexpr unresolved = sx_end_of_list, reqs, rv = sx_true, a, aa;

        if (truep (sx_set_memberp (m->schedulerflags, sym_enabled)))
        {
            return sx_false;
        }

        reqs = reschedule_get_enable (compile_list(m->requires), &unresolved);

        if (!eolp (unresolved))
        {
            return cons (sx_nonexistent, unresolved);
        }

        for (; consp (reqs); reqs = cdr (reqs))
        {
            a = car (reqs);
            aa = car (a);

            if (kmodulep (aa))
            {
                struct kyu_module *ms = (struct kyu_module *)aa;

                if (truep (sx_set_memberp (ms->schedulerflags, sym_enabled)))
                {
                    continue;
                }

                if (truep (sx_set_memberp (ms->schedulerflags, sym_blocked)))
                {
                    return cons (sx_nonexistent, cons (a, sx_end_of_list));
                }
            }
            else if (kservicep (aa))
            {
                struct kyu_service *ss = (struct kyu_service *)aa;

                if (truep (sx_set_memberp (ss->schedulerflags, sym_enabled)))
                {
                    continue;
                }

                if (truep (sx_set_memberp (ss->schedulerflags, sym_blocked)))
                {
                    return cons (sx_nonexistent, cons (a, sx_end_of_list));
                }
            }

            if (truep (rv))
            {
                rv = sx_end_of_list;
            }

            rv = cons (a, rv);
        }

        return rv;
    }

    return sx_nonexistent;
}

/* pretty much analogous to requirements()...
 *
 * return #t if it's safe to disable, #f if it's already disabled, otherwise a
 * list with stuff to disable first. the (#nonexistent ...) reply can't occur
 * with this one.
 *
 * this should work fine by just going through all modules, checking if they're
 * enabled, if so, gather what they (require ), then see if that somehow refers
 * to sx.
 */
static sexpr users (sexpr sx)
{
    sexpr c, sysname, sysx, mc, ma = sx_nil, modlist = sx_end_of_list,
          using, ua, uv, user_list = sx_end_of_list, tsysname, mlx = sx_nil;
    struct kyu_system  *sys;
    struct kyu_module  *mod;
    struct kyu_service *svc;

    tsysname = cdr (sx);
    sx = car (sx);

    if (kservicep (sx))
    {
        svc = (struct kyu_service *)sx;

        if (falsep (sx_set_memberp (svc->schedulerflags, sym_enabled)))
        {
            return sx_false;
        }

        modlist = svc->modules;

        for (c = modlist; consp (c); c = cdr(c))
        {
            ma = car (c);

            if (!kmodulep (ma))
            {
                continue;
            }

            mod = (struct kyu_module *)ma;

            if (falsep (sx_set_memberp (mod->schedulerflags, sym_enabled)))
            {
                modlist = sx_set_remove (modlist, ma);
            }
        }

        if (eolp (ma))
        {
            return sx_false;
        }
    }
    else if (kmodulep (sx))
    {
        mod = (struct kyu_module *)sx;

        if (falsep (sx_set_memberp (mod->schedulerflags, sym_enabled)))
        {
            return sx_false;
        }

        modlist = cons (sx, modlist);
    }

    for (c = lx_environment_alist (system_data); consp (c); c = cdr (c))
    {
        sysx    = car (c);
        sysname = car (sysx);
        sysx    = cdr (sysx);

        if (!ksystemp (sysx))
        {
            continue;
        }
            
        sys = (struct kyu_system *)sysx;

        for (mc = sys->modules; consp (mc); mc = cdr (mc))
        {
            ma = car (mc);

            if (!kmodulep (ma))
            {
                continue;
            }

            mod = (struct kyu_module *)ma;
 
            if (truep (sx_set_memberp (mod->schedulerflags, sym_enabled)))
            {
                for (using = reschedule_get_disable
                        (compile_list (mod->requires));
                     consp (using); using = cdr (using))
                {
                    ua = car (using);
                    uv = cdr (ua);
                    ua = car (ua);

                    if (falsep (equalp (tsysname, uv)))
                    {
                        continue;
                    }

                    if (kservicep (ua))
                    {
                        svc = (struct kyu_service *)ua;

                        mlx = svc->modules;
                    }
                    else if (kmodulep (ua))
                    {
                        mlx = cons (ua, sx_end_of_list);
                    }

                    if (!eolp (sx_set_intersect (mlx, modlist)))
                    {
                        /* if the intersection of the list of modules used by
                         * this atom and the list of modules described as the
                         * parameter we're checking, then the module whose deps
                         * we just pulled is usign us... */
                        user_list = sx_set_add (user_list, ma);

                        goto next_module;
                    }
                }
            }

          next_module:;
        }
    }

    if (eolp (user_list))
    {
        return sx_true;
    }

    return user_list;
}

/* sort using before/after ordering; "after" takes precedence while sorting */
static sexpr sort_modules_and_services (sexpr a, sexpr b)
{
    sexpr names_a  = sx_end_of_list, names_b  = sx_end_of_list,
          svcn_a   = sx_end_of_list, svcn_b   = sx_end_of_list,
          after_a  = sx_end_of_list, after_b  = sx_end_of_list,
          before_a = sx_end_of_list, before_b = sx_end_of_list,
          system_a = sx_end_of_list, system_b = sx_end_of_list,
          c, ca;

    if (truep (equalp (a, b)))
    {
        return sx_false;
    }

    if (consp (a))
    {
        system_a = sx_set_add (system_a, cdr (a));
        a        = car (a);
    }

    if (consp (b))
    {
        system_b = sx_set_add (system_b, cdr (b));
        b        = car (b);
    }

    if (kmodulep (a))
    {
        struct kyu_module *m = (struct kyu_module *)a;

        names_a  = sx_set_add   (names_a, m->name);
        svcn_a   = m->provides;
        after_a  = sx_set_merge (m->after, m->requires);
        before_a = m->before;
    }
    else if (kservicep (a))
    {
        struct kyu_service *s = (struct kyu_service *)a;

        for (c = s->modules; consp (c); c = cdr (c))
        {
            struct kyu_module *m = (struct kyu_module *)car(c);

            names_a  = sx_set_add   (names_a,  m->name);
            svcn_a   = sx_set_merge (svcn_a,   m->provides);
            after_a  = sx_set_merge (after_a,  m->after);
            after_a  = sx_set_merge (after_a,  m->requires);
            before_a = sx_set_merge (before_a, m->before);
        }
    }

    if (kmodulep (b))
    {
        struct kyu_module *m = (struct kyu_module *)b;

        names_b  = sx_set_add   (names_b, m->name);
        svcn_b   = m->provides;
        after_b  = sx_set_merge (m->after, m->requires);
        before_b = m->before;
    }
    else if (kservicep (b))
    {
        struct kyu_service *s = (struct kyu_service *)b;

        for (c = s->modules; consp (c); c = cdr (c))
        {
            struct kyu_module *m = (struct kyu_module *)car(c);

            names_b  = sx_set_add   (names_b,  m->name);
            svcn_b   = sx_set_merge (svcn_b,   m->provides);
            after_b  = sx_set_merge (after_b,  m->after);
            after_b  = sx_set_merge (after_b,  m->requires);
            before_b = sx_set_merge (before_b, m->before);
        }
    }

    for (c = after_b; consp (c); c = cdr (c))
    {
        ca = car (c);

      retry_a_b:
        if (symbolp (ca))
        {
            if (truep (sx_set_rx_memberp (names_a, ca)))
            {
                return sx_false;
            }
        }
        else if (stringp (ca))
        {
            if (truep (sx_set_rx_memberp (svcn_a, ca)))
            {
                return sx_false;
            }
        }
        else if (consp (ca) && truep (sx_set_rx_memberp (system_a, car(ca))))
        {
          goto retry_a_b;
            ca = cdr (ca);
        }
    }

    for (c = after_a; consp (c); c = cdr (c))
    {
        ca = car (c);

      retry_a_a:
        if (symbolp (ca))
        {
            if (truep (sx_set_rx_memberp (names_b, ca)))
            {
                return sx_true;
            }
        }
        else if (stringp (ca))
        {
            if (truep (sx_set_rx_memberp (svcn_b, ca)))
            {
                return sx_true;
            }
        }
        else if (consp (ca) && truep (sx_set_rx_memberp (system_b, car(ca))))
        {
          goto retry_a_a;
            ca = cdr (ca);
        }
    }

    for (c = before_a; consp (c); c = cdr (c))
    {
        ca = car (c);

      retry_b_a:
        if (symbolp (ca))
        {
            if (truep (sx_set_rx_memberp (names_b, ca)))
            {
                return sx_false;
            }
        }
        else if (stringp (ca))
        {
            if (truep (sx_set_rx_memberp (svcn_b, ca)))
            {
                return sx_false;
            }
        }
        else if (consp (ca) && truep (sx_set_rx_memberp (system_b, car(ca))))
        {
          goto retry_b_a;
            ca = cdr (ca);
        }
    }

    for (c = before_b; consp (c); c = cdr (c))
    {
        ca = car (c);

      retry_b_b:
        if (symbolp (ca))
        {
            if (truep (sx_set_rx_memberp (names_a, ca)))
            {
                return sx_true;
            }
        }
        else if (stringp (ca))
        {
            if (truep (sx_set_rx_memberp (svcn_a, ca)))
            {
                return sx_true;
            }
        }
        else if (consp (ca) && truep (sx_set_rx_memberp (system_a, car(ca))))
        {
          goto retry_b_b;
            ca = cdr (ca);
        }
    }

    return sx_false;
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
 *
 * returns #t when it did do something or if there's still something to do
 * before the current mode is reached, otherwise it'll return #f to indicate
 * that the target mode has been reached (and that no rescheduling needs to be
 * done or can be done anymore with this mode data).
 */
static sexpr reschedule ( void )
{
    sexpr unresolved = sx_end_of_list, c, a, ca, aa, r, rv = sx_false,
          to_enable = reschedule_get_enable
              (target_mode_enable, &unresolved),
          seen = to_enable,
          to_disable = reschedule_get_disable
              (target_mode_disable);
    int pc;

    c = to_enable;

    while (consp (c))
    {
        a = car (c);

        r = requirements (a);

        if (falsep (r)) /* does not need to be enabled, so we ditch it */
        {
            to_enable = sx_set_remove (to_enable, a);
        }
        else
        {
            if (truep(r))
            {
                rv = sx_true;
            }
            else if (consp (r)) /* cannot enable (yet) */
            {
                to_enable = sx_set_remove (to_enable, a);

                if (nexp (car (r))) /* unresolved requirements */
                {
                    unresolved = sx_set_merge (unresolved, cdr (r));
                }
                else /* resolved requirements */
                {
                    rv = sx_true;
                
                    for (ca = r; consp (ca); ca = cdr (ca))
                    {
                        aa = car (ca);

                        if (falsep (sx_set_memberp (seen, aa)))
                        {
                            to_enable = sx_set_add (to_enable, aa);
                        }
                    }

                    seen = sx_set_merge (seen, r);

                    c = to_enable;

                    continue;
                }
            }
        }

        c = cdr (c);
    }

    pc = 0;

    for (c = to_enable; consp (c); c = cdr (c))
    {
        a = car (c);

        if (truep (flagp (a, sym_enabling)))
        {
            to_enable = sx_set_remove (to_enable, a);
            pc++;
        }
    }

    seen = sx_set_sort_merge (seen, sort_modules_and_services);

    /* intersecting sx_enable with the sorted version of seen indirectly sorts
     * to_enable as well, since sx_set_intersect will just go over the second
     * argument and add any elements that appear in the first. the intersection
     * does, however, indirectly reverse the order of the second argument, thus
     * the sx_reverse. */
    to_enable = sx_reverse (sx_set_intersect (to_enable, seen));

    r = to_enable;

    /* remove all things in the list of stuff to enable that should be scheduled
     * after previous stuff in the list... */
    for (c = to_enable; consp (c); c = cdr (c))
    {
        ca = car (c);

        for (a = seen; consp (a); a = cdr (a))
        {
            aa = car (a);

            if (truep (sort_modules_and_services (ca, aa)) &&
                falsep (flagp (aa, sym_enabled)) &&
                falsep (flagp (aa, sym_blocked)))
            {
                to_enable = sx_set_remove (to_enable, ca);

                break;
            }
        }
    }

    if (eolp (to_enable) && (pc == 0))
    {
        /* okay, seems like we have a pretty twocked scheduling situation...
         * looks like someone was overzealous with their before/after specs...
         */

        to_enable = r;

        for (c = to_enable; consp (c); c = cdr (c))
        {
            ca = car (c);

            for (a = to_enable; consp (a); a = cdr (a))
            {
                aa = car (a);

                if (truep (sort_modules_and_services (ca, aa)) &&
                    falsep (flagp (aa, sym_enabled)) &&
                    falsep (flagp (aa, sym_blocked)))
                {
                    to_enable = sx_set_remove (to_enable, ca);

                    break;
                }
            }
        }

        if (eolp (to_enable))
        {
            /* ... WAY overzealous... */

            to_enable = r;
        }
    }

    /* now for disabling things... */

    c = seen = to_disable;

    while (consp (c))
    {
        a = car (c);

        r = users (a);

        if (falsep (r)) /* does not need to be disabled, so we ditch it */
        {
            to_disable = sx_set_remove (to_disable, a);
        }
        else
        {
            rv = sx_true;

            if (consp (r)) /* cannot disable yet, still in use */
            {
                to_disable = sx_set_remove (to_disable, a);

                for (ca = r; consp (ca); ca = cdr (ca))
                {
                    aa = car (ca);

                    if (falsep (sx_set_memberp (seen, aa)))
                    {
                        to_disable = sx_set_add (to_disable, aa);
                    }
                }

                seen = sx_set_merge (seen, r);

                c = to_disable;

                continue;
            }
        }

        c = cdr (c);
    }

    pc = 0;

    for (c = to_disable; consp (c); c = cdr (c))
    {
        a = car (c);

        if (truep (flagp (a, sym_disabling)))
        {
            to_disable = sx_set_remove (to_disable, a);
            pc++;
        }
    }

    /* sorting to disable is in reverse order of enabling */
    seen = sx_reverse(sx_set_sort_merge (seen, sort_modules_and_services));
    to_disable = sx_reverse (sx_set_intersect (to_disable, seen));

    r = to_disable;
    
    for (c = to_disable; consp (c); c = cdr (c))
    {
        ca = car (c);

        for (a = seen; consp (a); a = cdr (a))
        {
            aa = car (a);

            if (falsep (sort_modules_and_services (ca, aa)) &&
                truep (flagp (aa, sym_enabled)))
            {
                to_disable = sx_set_remove (to_disable, ca);

                break;
            }
        }
    }

    if (eolp (to_disable) && (pc == 0))
    {
        to_disable = r;

        for (c = to_disable; consp (c); c = cdr (c))
        {
            ca = car (c);

            for (a = to_disable; consp (a); a = cdr (a))
            {
                aa = car (a);

                if (falsep (sort_modules_and_services (ca, aa)) &&
                    truep (flagp (aa, sym_enabled)))
                {
                    to_disable = sx_set_remove (to_disable, ca);

                    break;
                }
            }
        }

        if (eolp (to_disable))
        {
           to_disable = r;
        }
    }

    /* end disabling, now actually go and do shit */

    if (!eolp (unresolved))
    {
        kyu_command (cons (sym_unresolved, cons (unresolved, sx_end_of_list)));
        rv = sx_true;
    }

    /*! \todo conflict specifications are currently ignored */
    /*! \todo scheduler hints are currently ignored */

    while (consp (to_enable))
    {
        a = car (to_enable);
        aa = car (a);

        if (falsep(statusp(aa)))
        {
            if (kservicep (aa))
            {
                system_service_action (cdr (a), aa, sym_enable);
            }
            else if (kmodulep (aa))
            {
                system_module_action (cdr (a), aa, sym_enable);
            }
        }

        to_enable = cdr (to_enable);
    }

    while (consp (to_disable))
    {
        a = car (to_disable);
        aa = car (a);

        if (falsep(statusp(aa)))
        {
            if (kservicep (aa))
            {
                system_service_action (cdr (a), aa, sym_disable);
            }
            else if (kmodulep (aa))
            {
                system_module_action (cdr (a), aa, sym_disable);
            }
        }

        to_disable = cdr (to_disable);
    }

    return rv;
}

/* calculate affinity for a given module */
static int get_affinity (sexpr sx)
{
    int rv = 0, t;
    struct kyu_module *m;
    sexpr c;

    if (kmodulep (sx))
    {
        m = (struct kyu_module *)sx;

#warning get_affinity() still ignores the global affinity spec list

        if (ia_m_in_mode != (int)0)
        {
#warning get_affinity() still ignores current mode data for implicit affinities
        }

        if (ia_m_num_of_provides != (int)0)
        {
            for (t = 0, c = m->provides; consp (c) ; c = cdr (c))
            {
                t++;
            }

            rv += (t * ia_m_num_of_provides);
        }

        if (ia_m_num_of_requirements != (int)0)
        {
            for (t = 0, c = m->requires; consp (c) ; c = cdr (c))
            {
                t++;
            }

            rv -= (t * ia_m_num_of_requirements);
        }

        if (ia_m_deprecated != (int)0)
        {
            if (truep (sx_set_memberp (m->schedulerflags, sym_deprecated)))
            {
                rv -= ia_m_deprecated;
            }
        }

        if (ia_m_stable != (int)0)
        {
            if (truep (sx_set_memberp (m->schedulerflags, sym_stable)))
            {
                rv += ia_m_stable;
            }
        }

        if (ia_m_experimental != (int)0)
        {
            if (truep (sx_set_memberp (m->schedulerflags, sym_experimental)))
            {
                rv += ia_m_experimental;
            }
        }
        
        if (ia_m_problematic != (int)0)
        {
            if (truep (sx_set_memberp (m->schedulerflags, sym_problematic)))
            {
                rv -= ia_m_problematic;
            }
        }
    }

    kyu_command (cons (sym_affinity, cons (sx, cons (make_integer (rv),
                 sx_end_of_list))));

    return rv;
}

static sexpr affinity_gtp (sexpr a, sexpr b)
{
    int aa = get_affinity (a), ab = get_affinity (b);

    if (aa < ab)
    {
        return sx_true;
    }
    else if (aa == ab)
    {
        if (kmodulep (a) && kmodulep (b))
        {
            struct kyu_module *ka = (struct kyu_module *)a,
                              *kb = (struct kyu_module *)b;
            const char *sa = sx_symbol (ka->name),
                       *sb = sx_symbol (kb->name);

            while ((*sa != (const char)0) && (*sa == *sb))
            {
                sa++;
                sb++;
            }

            return (*sa < *sb) ? sx_true : sx_false;
        }
    }

    return sx_false;
}

static void sort_modules_by_affinity ( void )
{
    sexpr c = lx_environment_alist (system_data);

    system_data = lx_make_environment (sx_end_of_list);

    while (consp (c))
    {
        sexpr a = car (c), sysname = car (a), sys = cdr (a);

        if (ksystemp (sys))
        {
            struct kyu_system *s = (struct kyu_system *)sys;
            sexpr d = s->services, nserv = sx_end_of_list;

            while (consp (d))
            {
                sexpr serv = car (d);

                if (kservicep (serv))
                {
                    struct kyu_service *se = (struct kyu_service *)serv;

                    nserv = sx_set_add
                        (nserv,
                         kyu_make_service
                           (se->name, se->description,
                            se->schedulerflags,
                            sx_set_sort_merge (se->modules,
                                               affinity_gtp)));
                }

                d = cdr (d);
            }

            kyu_command (cons (sym_update, cons (sysname, nserv)));

            system_data =
                lx_environment_bind
                    (system_data, sysname,
                     kyu_make_system (s->name, s->description, s->location,
                                      s->schedulerflags, s->modules, nserv));
        }

        c = cdr (c);
    }
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
                                  mods = sv->modules;

                            if (falsep (equalp (desc, m->description)))
                            {
                                desc = sx_join (desc, sym_c_s, m->description);
                            }

                            flags = sx_set_merge (flags, m->schedulerflags);

                            if (truep (sx_set_memberp (flags, sym_blocked)) &&
                                (falsep (sx_set_memberp (m->schedulerflags,
                                                         sym_blocked)) ||
                                 falsep (sx_set_memberp (sv->schedulerflags,
                                                         sym_blocked))))
                            {
                                /* make sure that the blocked flag is only going
                                 * to be on a service, if all providing modules
                                 * are blocked. */
                                flags = sx_set_remove (flags, sym_blocked);
                            }

                            mods = cons (s_mo, mods);

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

    sort_modules_by_affinity ();
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
            target_mode_enable  = sx_set_merge (target_mode_enable, cdr (a));
        }
        else if (truep (equalp (aa, sym_disable)))
        {
            target_mode_disable = sx_set_merge (target_mode_disable, cdr (a));
        }
        else
        {
            target_mode_ipc     = sx_set_add (target_mode_ipc, a);
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

        if (symbolp (la))
        {
            r = cons (cons (rx_compile_sx (sym_any_rx), la), r);
        }
        else if (stringp (la))
        {
            r = cons (cons (rx_compile_sx (sym_any_rx),
                            cons (la, rx_compile_sx (la))),
                      r);
        }
        else if (consp (la))
        {
            sexpr d = cdr (la);

            r = cons (cons (rx_compile_sx (car(la)),
                            cons (la, rx_compile_sx(d))),
                      r);
        }

        l = cdr (l);
    }

    return r;
}

/* look up the target mode and merge all enable/disable/ipc data */
static sexpr rebuild_target_data ( void )
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
        return cons (sym_error, cons (sym_missing_mode_data,
                     cons (target_mode, sx_end_of_list)));
    }

    target_mode_enable  = compile_list (target_mode_enable);
    target_mode_disable = compile_list (target_mode_disable);

    return sx_true;
}

static void reevaluate_target_data ( void )
{
    sexpr t;

    if (nilp (target_mode))
    {
        target_mode_ipc     = sx_end_of_list;
        target_mode_enable  = compile_list (target_extra_enable);
        target_mode_disable = compile_list (target_extra_disable);

        if (falsep (reschedule()))
        {
            target_extra_enable  = sx_end_of_list;
            target_extra_disable = sx_end_of_list;
        }
    }
    else
    {
        if (!truep(t = rebuild_target_data ()))
        {
            kyu_command (t);
            return;
        }

        if (falsep (reschedule ()))
        {
            if (falsep(equalp(current_mode, target_mode)))
            {
                current_mode = target_mode;
                kyu_command (cons (sym_mode, cons (target_mode, sx_end_of_list)));

                for (t = target_mode_ipc; consp (t); t = cdr (t))
                {
                    kyu_command (car(t));
                }

                target_mode = sx_nil;
                
                reevaluate_target_data ();
            }
        }
    }
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
                          ts_schedulerflags, ts_modules, ts_services, c,
                          flagdelta = sx_end_of_list;

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
                        else
                        {
                            flagdelta = sx_set_difference
                                (((struct kyu_module *)a)->schedulerflags,
                                 ((struct kyu_module *)m)->schedulerflags);
                        }

                        c = cdr (c);
                    }

                    ts = kyu_make_system
                            (ts_name, ts_description, ts_location,
                             ts_schedulerflags, ts_modules, ts_services);

                    system_data = lx_environment_bind
                            (system_data, target_system, ts);

                    if (!eolp (flagdelta))
                    {
                        update_services();
                        reevaluate_target_data ();
                    }
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
                else if (truep (equalp (a, sym_scheduler)))
                {
                    sx = car(cdr (sx));
                    if (environmentp (sx))
                    {
                        a = lx_environment_lookup (a, sym_affinity);

                        if (!nexp (a))
                        {
                            affinities = a;
                        }

                        a = lx_environment_lookup
                            (a, sym_implicit_affinity_multipliers);

                        if (!nexp (a))
                        {
                            sx = car (a);

                            ia_m_in_mode =
                                (integerp (sx) ? sx_integer (sx) : 0);

                            a  = cdr (a);
                            sx = car (a);

                            ia_m_num_of_provides =
                                (integerp (sx) ? sx_integer (sx) : 0);

                            a  = cdr (a);
                            sx = car (a);

                            ia_m_num_of_requirements =
                                (integerp (sx) ? sx_integer (sx) : 0);

                            a  = cdr (a);
                            sx = car (a);

                            ia_m_deprecated =
                                (integerp (sx) ? sx_integer (sx) : 0);

                            a  = cdr (a);
                            sx = car (a);

                            ia_m_stable =
                                (integerp (sx) ? sx_integer (sx) : 0);

                            a  = cdr (a);
                            sx = car (a);

                            ia_m_experimental =
                                (integerp (sx) ? sx_integer (sx) : 0);

                            a  = cdr (a);
                            sx = car (a);

                            ia_m_problematic =
                                (integerp (sx) ? sx_integer (sx) : 0);
                        }
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
        else if (truep (equalp (a, sym_enable)))
        {
            sx = cdr (sx);

            target_extra_enable
                    = sx_set_merge  (target_extra_enable,  sx);

            while (consp (sx))
            {
                target_extra_disable
                    = sx_set_remove (target_extra_disable, car (sx));
                sx = cdr (sx);
            }

            kyu_command (cons (sym_extra_scheduler_data,
                               cons (target_extra_enable,
                                     cons (target_extra_disable,
                                           sx_end_of_list))));

            reevaluate_target_data();
        }
        else if (truep (equalp (a, sym_disable)))
        {
            sx = cdr (sx);

            target_extra_disable
                    = sx_set_merge  (target_extra_disable, sx);

            while (consp (sx))
            {
                target_extra_enable
                    = sx_set_remove (target_extra_enable, car (sx));
                sx = cdr (sx);
            }

            kyu_command (cons (sym_extra_scheduler_data,
                               cons (target_extra_enable,
                                     cons (target_extra_disable,
                                           sx_end_of_list))));

            reevaluate_target_data ();
        }
    }
}

static void read_configuration ()
{
        kyu_command (cons (sym_request,
                           cons (sym_configuration,
                                 cons (sym_modes, sx_end_of_list))));
        kyu_command (cons (sym_request,
                           cons (sym_configuration,
                                 cons (sym_scheduler, sx_end_of_list))));
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
