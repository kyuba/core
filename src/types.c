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

#include <kyuba/types.h>
#include <curie/tree.h>
#include <curie/memory.h>
#include <curie/gc.h>

static struct tree module_tree         = TREE_INITIALISER;
static struct tree service_tree        = TREE_INITIALISER;
static struct tree system_tree         = TREE_INITIALISER;

static sexpr module_serialise           (sexpr sx);
static sexpr module_unserialise         (sexpr sx);
static void  module_tag                 (sexpr sx);
static void  module_destroy             (sexpr sx);
static void  module_call                ( void );

static sexpr service_serialise          (sexpr sx);
static sexpr service_unserialise        (sexpr sx);
static void  service_tag                (sexpr sx);
static void  service_destroy            (sexpr sx);
static void  service_call               ( void );

static sexpr system_serialise           (sexpr sx);
static sexpr system_unserialise         (sexpr sx);
static void  system_tag                 (sexpr sx);
static void  system_destroy             (sexpr sx);
static void  system_call                ( void );

void initialise_kyu_types ( void )
{
    static char initialised = 0;

    if (initialised == (char)0)
    {
        sx_register_type
                (kyu_module_type_identifier,
                 module_serialise, module_unserialise,
                 module_tag, module_destroy,
                 module_call, (void *)0);

        sx_register_type
                (kyu_service_type_identifier,
                 service_serialise, service_unserialise,
                 service_tag, service_destroy,
                 service_call, (void *)0);

        sx_register_type
                (kyu_system_type_identifier,
                 system_serialise, system_unserialise,
                 system_tag, system_destroy,
                 system_call, (void *)0);

        initialised = (char)1;
    }
}

static void lx_sx_map_call (struct tree_node *node, void *u)
{
    sexpr sx = (sexpr)node_get_value(node);

    gc_call (sx);
}

static sexpr module_serialise (sexpr sx)
{
    struct kyu_module *m = (struct kyu_module *)sx;
    sx = cons (m->name, cons (m->description, cons (m->provides, cons
            (m->requires, cons (m->before, cons (m->after, cons (m->conflicts,
            cons (m->schedulerflags, cons (m->functions,sx_end_of_list)))))))));

    return sx;
}

static sexpr module_unserialise (sexpr sx)
{
    sexpr a  = car (sx);
    sexpr ad = cdr (sx);
    sexpr b  = car (ad);
    sexpr bd = cdr (ad);
    sexpr c  = car (bd);
    sexpr cd = cdr (bd);
    sexpr d  = car (cd);
    sexpr dd = cdr (cd);
    sexpr e  = car (dd);
    sexpr ed = cdr (dd);
    sexpr f  = car (ed);
    sexpr fd = cdr (ed);
    sexpr g  = car (fd);
    sexpr gd = cdr (fd);
    sexpr h  = car (gd);
    sexpr hd = cdr (gd);
    sexpr i  = car (hd);

    return kyu_make_module (a, b, c, d, e, f, g, h, i);
}

static void module_tag (sexpr sx)
{
    sexpr tn = module_serialise (sx);

    gc_tag (tn);
}

static void module_destroy (sexpr sx)
{
    sexpr tn = module_serialise (sx);

    free_pool_mem ((void *)sx);

    tree_remove_node(&module_tree, (int_pointer)tn);
}

static void module_call ( void )
{
    tree_map (&module_tree, lx_sx_map_call, (void *)0);
}

sexpr kyu_make_module
        (sexpr name, sexpr description, sexpr provides, sexpr requires,
         sexpr before, sexpr after, sexpr conflicts, sexpr schedulerflags,
         sexpr functions)
{
    static struct memory_pool pool
            = MEMORY_POOL_INITIALISER (sizeof (struct kyu_module));
    struct kyu_module *rv;
    struct tree_node *n;
    sexpr sx = cons (name, cons (description, cons (provides, cons
            (requires, cons (before, cons (after, cons (conflicts, cons
            (schedulerflags, cons (functions, sx_end_of_list)))))))));

    if ((n = tree_get_node (&module_tree, (int_pointer)sx)))
    {
        return (sexpr)node_get_value (n);
    }

    rv = get_pool_mem (&pool);

    if (rv == (struct kyu_module *)0)
    {
        return sx_nonexistent;
    }

    rv->type           = kyu_module_type_identifier;
    rv->name           = name;
    rv->description    = description;
    rv->provides       = provides;
    rv->requires       = requires;
    rv->before         = before;
    rv->after          = after;
    rv->conflicts      = conflicts;
    rv->schedulerflags = schedulerflags;
    rv->functions      = functions;

    tree_add_node_value (&module_tree, (int_pointer)sx, (void *)rv);

    return (sexpr)rv;
}

static sexpr service_serialise (sexpr sx)
{
    struct kyu_service *m = (struct kyu_service *)sx;
    sx = cons (m->name, cons (m->description, cons (m->schedulerflags, cons
            (m->modules,sx_end_of_list))));

    return sx;
}

static sexpr service_unserialise (sexpr sx)
{
    sexpr a  = car (sx);
    sexpr ad = cdr (sx);
    sexpr b  = car (ad);
    sexpr bd = cdr (ad);
    sexpr c  = car (bd);
    sexpr cd = cdr (bd);
    sexpr d  = car (cd);

    return kyu_make_service (a, b, c, d);
}

static void service_tag (sexpr sx)
{
    sexpr tn = service_serialise (sx);

    gc_tag (tn);
}

static void service_destroy (sexpr sx)
{
    sexpr tn = service_serialise (sx);

    free_pool_mem ((void *)sx);

    tree_remove_node(&service_tree, (int_pointer)tn);
}

static void service_call ( void )
{
    tree_map (&service_tree, lx_sx_map_call, (void *)0);
}

sexpr kyu_make_service
        (sexpr name, sexpr description, sexpr schedulerflags, sexpr modules)
{
    static struct memory_pool pool
            = MEMORY_POOL_INITIALISER (sizeof (struct kyu_module));
    struct kyu_service *rv;
    struct tree_node *n;
    sexpr sx = cons (name, cons (description, cons (schedulerflags, cons
            (modules, sx_end_of_list))));

    if ((n = tree_get_node (&service_tree, (int_pointer)sx)))
    {
        return (sexpr)node_get_value (n);
    }

    rv = get_pool_mem (&pool);

    if (rv == (struct kyu_service *)0)
    {
        return sx_nonexistent;
    }

    rv->type           = kyu_service_type_identifier;
    rv->name           = name;
    rv->description    = description;
    rv->schedulerflags = schedulerflags;
    rv->modules        = modules;

    tree_add_node_value (&service_tree, (int_pointer)sx, (void *)rv);

    return (sexpr)rv;
}

static sexpr system_serialise (sexpr sx)
{
    struct kyu_system *m = (struct kyu_system *)sx;
    sx = cons (m->name, cons (m->description, cons (m->location,
               cons (m->schedulerflags, cons (m->modules, cons (m->services,
               sx_end_of_list))))));

    return sx;
}

static sexpr system_unserialise (sexpr sx)
{
    sexpr a  = car (sx);
    sexpr ad = cdr (sx);
    sexpr b  = car (ad);
    sexpr bd = cdr (ad);
    sexpr c  = car (bd);
    sexpr cd = cdr (bd);
    sexpr d  = car (cd);
    sexpr dd = cdr (cd);
    sexpr e  = car (dd);
    sexpr ed = cdr (dd);
    sexpr f  = car (ed);

    return kyu_make_system (a, b, c, d, e, f);
}

static void system_tag (sexpr sx)
{
    sexpr tn = system_serialise (sx);

    gc_tag (tn);
}

static void system_destroy (sexpr sx)
{
    sexpr tn = system_serialise (sx);

    free_pool_mem ((void *)sx);

    tree_remove_node(&system_tree, (int_pointer)tn);
}

static void system_call ( void )
{
    tree_map (&system_tree, lx_sx_map_call, (void *)0);
}

sexpr kyu_make_system
        (sexpr name, sexpr description, sexpr location, sexpr schedulerflags,
         sexpr modules, sexpr services)
{
    static struct memory_pool pool
            = MEMORY_POOL_INITIALISER (sizeof (struct kyu_module));
    struct kyu_system *rv;
    struct tree_node *n;
    sexpr sx = cons (name, cons (description, cons (location, cons
            (schedulerflags, cons (modules, cons (services,sx_end_of_list))))));

    if ((n = tree_get_node (&system_tree, (int_pointer)sx)))
    {
        return (sexpr)node_get_value (n);
    }

    rv = get_pool_mem (&pool);

    if (rv == (struct kyu_system *)0)
    {
        return sx_nonexistent;
    }

    rv->type           = kyu_system_type_identifier;
    rv->name           = name;
    rv->description    = description;
    rv->location       = location;
    rv->schedulerflags = schedulerflags;
    rv->modules        = modules;
    rv->services       = services;

    tree_add_node_value (&system_tree, (int_pointer)sx, (void *)rv);

    return (sexpr)rv;
}
