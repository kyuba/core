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

#ifndef KYUBA_IPC_H
#define KYUBA_IPC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <curie/sexpr.h>
#include <duat/9p-client.h>

define_symbol (sym_power_down,  "power-down");
define_symbol (sym_power_reset, "power-reset");
define_symbol (sym_error,       "error");
define_symbol (sym_disconnect,  "disconnect");

void multiplex_kyu                ();

void multiplex_add_kyu_sexpr      (struct sexpr_io *,
                                   void (*on_event)(sexpr, void *), void *aux);
void multiplex_add_kyu_stdio      (void (*on_event)(sexpr, void *), void *aux);

void kyu_command                  (sexpr command);
void kyu_disconnect               ();

#ifdef __cplusplus
}
#endif

#endif