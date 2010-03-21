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

#include <curie/filesystem.h>
#include <kyuba/system.h>

int kyu_test_pid (int pid)
{
    char file[] = "/proc/NNNNN";

    if (pid >= 10000)
    {
        file[10] = '0' + (pid % 10);
        pid /= 10;
    }
    else
    {
        file [10] = 0;
    }

    if (pid >= 1000)
    {
        file[9] = '0' + (pid % 10);
        pid /= 10;
    }
    else
    {
        file [9] = 0;
    }

    if (pid >= 100)
    {
        file[8] = '0' + (pid % 10);
        pid /= 10;
    }
    else
    {
        file [8] = 0;
    }

    if (pid >= 10)
    {
        file[7] = '0' + (pid % 10);
        pid /= 10;
    }
    else
    {
        file [7] = 0;
    }

    file[6] = '0' + (pid % 10);
    pid /= 10;

    return truep (filep (make_string (file)));
}
