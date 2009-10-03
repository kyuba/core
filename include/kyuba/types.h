/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2008, 2009, Kyuba Project Members
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

#ifndef KYUBA_TYPES_H
#define KYUBA_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#define kyu_module_type_identifier  0x5de /* מ */
#define kyu_service_type_identifier 0x5e2 /* ע */
#define kyu_system_type_identifier  0x5e6 /* צ */

#define kmodulep(sx)  sx_customp(sx,kyu_module_type_identifier)
#define kservicep(sx) sx_customp(sx,kyu_service_type_identifier)
#define ksystemp(sx)  sx_customp(sx,kyu_system_type_identifier)

struct kyu_module
{
    unsigned int type;
    sexpr name;
    sexpr description;
    sexpr provides;
    sexpr requires;
    sexpr before;
    sexpr after;
    sexpr conflicts;
    sexpr schedulerflags;
    sexpr functions;
};

struct kyu_service
{
    unsigned int type;
    sexpr name;
    sexpr description;
    sexpr schedulerflags;
    sexpr modules;
};

struct kyu_system
{
    unsigned int type;
    sexpr name;
    sexpr description;
    sexpr location;
    sexpr schedulerflags;
    sexpr modules;
    sexpr services;
};

void initialise_kyu_types ( void );

#ifdef __cplusplus
}
#endif

#endif
