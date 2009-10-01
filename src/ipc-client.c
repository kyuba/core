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

#include <curie/main.h>
#include <curie/memory.h>
#include <curie/sexpr.h>
#include <curie/multiplex.h>
#include <curie/network.h>
#include <kyuba/ipc.h>

struct sexpr_io *stdio        = (struct sexpr_io *)0;
static char o_send_commands   = (char)0;

static int print_help ()
{
#define help "kyu (-H|-D|-R|-i)\n\n\
Options:\n\
  -H, -D  shut down the computer\n\
  -R      re-boot the computer\n\
  -i      enter interactive mode\n"
    struct io *out = io_open (1);
    if (out != (struct io *)0)
    {
        io_write (out, help, (sizeof (help) -1));
    }

    return 1;
}

static void on_read_stdio (sexpr e, struct sexpr_io *io, void *aux)
{
    if (o_send_commands == (char)1)
    {
        kyu_command (e);
    }
}

static void on_event (sexpr event, void *aux)
{
    sx_write (stdio, event);

    if (consp (event))
    {
        sexpr c = car(event);

        if (truep(equalp(c, sym_disconnect)))
        {
            cexit (24);
        }
        else if (truep(equalp(c, sym_event)))
        {
            c = car (cdr (event));

            if (truep(equalp(c, sym_power_down)))
            {
                cexit (0);
            }
            else if (truep(equalp(c, sym_power_reset)))
            {
                cexit (0);
            }
        }
    }
}

int cmain()
{
    struct sexpr_io *io;
    char cmd = (char)0;

    terminate_on_allocation_errors();

    multiplex_sexpr       ();
    stdio = sx_open_stdio ();
    multiplex_add_sexpr   (stdio, on_read_stdio, (void *)0);

    for (int i = 1; curie_argv[i] != (char *)0; i++)
    {
        char *t = curie_argv[i];

        if (t[0] == '-')
        {
            for (int j = 1; t[j] != (char)0; j++)
            {
                switch (t[j])
                {
                    case 'H':
                    case 'D':
                        kyu_command (cons (sym_event, cons (sym_power_down, sx_end_of_list)));
                        cmd = (char)1;
                        break;
                    case 'R':
                        kyu_command (cons (sym_event, cons (sym_power_reset, sx_end_of_list)));
                        cmd = (char)1;
                        break;
                    case 'i':
                        o_send_commands = (char)1;
                        cmd = (char)1;
                        break;
                    default:
                        cexit (print_help ());
                        break;
                }
            }
        }
    }

    if (cmd == (char)0)
    {
        cexit (print_help ());
    }

    multiplex_kyu ();

    io = sx_open_socket (KYU_IPC_SOCKET);

    multiplex_add_kyu_sexpr (io, on_event, (void *)0);

    while (multiplex() == mx_ok);

    return 0;
}
