/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2007-2010, Kyuba Project Members
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
#include <syscall/syscall.h>
#include <linux/reboot.h>

// let's be serious, /bin is gonna exist
#define LRTMPPATH "/bin"

#define CONSOLE "/dev/console"

#define LRTMP_LOOP LRTMPPATH "/dev/loop/x"
#define REAL_LOOP "/dev/loop/x"

#define MAX_RETRIES 5
#define MAX_PID ((0x1000 * 8)+1)

#define BUFFERSIZE 0x2000 /* 8kb */
/* seems unlikely that your /proc/mounts is gonna be bigger, and if it is, it's
   gonna run a couple times anyway, so it won't matter */

#define MSG_INTRO " >> Project Kyuba | last rites <<\n"\
     "####################################################################"\
     "###########\n"
#define MSG_PRUNE "pruning file descriptors..."
#define MSG_REOPEN " [ ok ]\n"
#define MSG_NO_KEXEC "no support for kexec?\n"
#define MSG_KEXEC_FAILED "whoops, looks like the kexec failed!\n"
#define MSG_CANT_SHUT_DOWN "can't shut down?\n"
#define MSG_CANT_REBOOT "can't reboot?\n"
#define MSG_EXITING "exiting... this is bad.\n"
#define MSG_MOUNT_PROC_FAILED \
     "couldn't mount another 'proc' at '" LRTMPPATH "/proc'"
#define MSG_MOUNT_TMPFS_FAILED "couldn't mount my tmpfs at " LRTMPPATH
#define MSG_PIVOT_ROOT_FAILED\
     "couldn't pivot_root('" LRTMPPATH "', '" LRTMPPATH "/old')"
#define MSG_UNMOUNTED " [ ok ]\n"
#define MSG_NOT_UNMOUNTED " [ .. ]\n"
#define MSG_READONLY " [ ro ]"
#define MSG_KILLING "Killing everything..."
#define MSG_UNMOUNTING "Unmounting everything...\n"
#define MSG_CLOSING_LOOPS "Disassociating loop filesystems..."
#define MSG_DONE " [ ok ]\n"

static int out = 2;

#define whitespacep(c)\
(((c) == ' ')  || ((c) == '\t') || ((c) == '\v') ||\
 ((c) == '\n') || ((c) == '\r'))

static void trim (char *s)
{
    long l = 0;
    int i = 0, offset = 0;;
    for (; s[l]; l++);

    for (; i < l; i++) {
        if (whitespacep(s[i])) offset++;
        else {
            if (offset) {
                for (char *d = s; i < l; i++, d++)
                {
                    *d = *(d + offset);
                }
                i = offset;
            }
            break;
        }
    }

    if (i == l) {
        s[0] = 0;
        return;
    }

    l -= offset + 1;

    for (i = l; i >= 0; i--) {
        if (whitespacep(s[i]))
            s[i] = 0;
        else
            break;
    }

}

static void sleep(int n)
{
    sys_poll ((void *)0, 0, n * 1000);
}

static int unmount_everything()
{
    int errors    = 0,
        positives = 0,
        fd;

    sys_write (out, MSG_UNMOUNTING, sizeof(MSG_UNMOUNTING)-1);

    if ((fd = sys_open ("/proc/mounts", 0, 0)) >= 0) {
        char cbuffer[BUFFERSIZE];
        char *ccbuffer = cbuffer;
        long c = 0;
        if ((c = sys_read (fd, cbuffer, BUFFERSIZE)) > 0)
        {
            char *ce = cbuffer + c;
            char *rbuffer;

            do {
                char *buffer = (char *)0;
                rbuffer = ccbuffer;

                for (; rbuffer < ce; rbuffer++)
                {
                    if ((*rbuffer) == '\n')
                    {
                        (*rbuffer) = 0;
                        buffer = ccbuffer;
                        ccbuffer = rbuffer + 1;

                        break;
                    }
                }

                if ((buffer != (char *)0) && (buffer[0] != '#')) {
                    trim(buffer);

                    if (buffer[0]) {
                        char *cur = buffer;
                        char *scur = cur;
                        long icur = 0;

                        char *fs_spec    = (char *)0;
                        char *fs_file    = (char *)0;
                        char *fs_vfstype = (char *)0;
                        char *fs_mntops  = (char *)0;

                        trim(cur);
                        for (; *cur; cur++) {
                            if (whitespacep(*cur)) {
                                *cur = 0;
                                icur++;
                                switch (icur) {
                                    case 1:
                                        fs_spec = scur;
                                        break;
                                    case 2:
                                        fs_file = scur;
                                        break;
                                    case 3:
                                        fs_vfstype = scur;
                                        break;
                                    case 4:
                                        fs_mntops = scur;
                                        break;
                                    case 5:
                                    case 6:
                                    /* don't care about fs_freq and fs_passno */
                                        break;
                                }
                                scur = cur + 1;
                                trim(scur);
                            }
                        }
                        if (cur != scur) {
                            icur++;
                            switch (icur) {
                                case 1:
                                    fs_spec = scur;
                                    break;
                                case 2:
                                    fs_file = scur;
                                    break;
                                case 3:
                                    fs_vfstype = scur;
                                    break;
                                case 4:
                                    fs_mntops = scur;
                                    break;
                                case 5:
                                case 6:
                                    /* don't care about fs_freq and fs_passno */
                                    break;
                            }
                        }

                        if (fs_spec && fs_file && (fs_file[0] == '/') &&
                            (fs_file[1] == 'o') && (fs_file[2] == 'l') &&
                            (fs_file[3] == 'd'))
                        {
                            int ixl;
                            long rt;
                            /* still mounted */

                            for (ixl = 0; fs_spec[ixl]; ixl++);
                            sys_write (out, fs_spec, ixl);

                            if (
#if defined(have_sys_umount)
                                sys_umount(fs_file) &&
#else
                                sys_umount2(fs_file, 0) &&
#endif
                                (rt = sys_umount2(fs_file, 1 /* MNT_FORCE */)))
                            {
                                char sx[] = " [ .. ]";
                                /* file failed to unmount */
                                errors++;

                                if (rt < 0) rt *= -1;
                                sx[4] = ('0' + (rt % 10));
                                rt /= 10;
                                sx[3] = ('0' + (rt % 10));

                                sys_write (out, sx, 7);

                                if (fs_vfstype) {
                                    /* try to remount read-only */
                                    if ((rt = sys_mount
                                             (fs_spec, fs_file, fs_vfstype,
                                              0x21 /* MS_REMOUNT|MS_RDONLY */,
                                              (void *)0)))
                                    {
                                        if (rt < 0) rt *= -1;
                                        sx[4] = ('0' + (rt % 10));
                                        rt /= 10;
                                        sx[3] = ('0' + (rt % 10));
                                        sys_write (out, sx, 7);
                                    }
                                    else
                                    {
                                        sys_write (out, MSG_READONLY,
                                                   sizeof(MSG_READONLY)-1);
                                    }
                                }
                                /* ... else there's been some bad data trying to
                                   parse /proc/mounts */

                                /* can't hurt to try this one  */
                                sys_umount2(fs_file, 4 /* MNT_EXPIRE */);
                                sys_umount2(fs_file, 4 /* MNT_EXPIRE */);

                                sys_write (out, MSG_NOT_UNMOUNTED,
                                           sizeof(MSG_NOT_UNMOUNTED)-1);
                            } else {
                                positives = 1; /* unmounted successfully */

                                sys_write (out, MSG_UNMOUNTED,
                                           sizeof(MSG_UNMOUNTED)-1);
                            }
                        }
                    }
                }
            } while (rbuffer < ce);
        }

        sys_close(fd);
    }

    if (positives) {
        sys_sync();
        return unmount_everything();
    }

    return errors;
}

static void kill_everything()
{
    int mypid = sys_getpid();

    sys_write (out, MSG_KILLING, sizeof(MSG_KILLING)-1);

    for (int i = 2; i < MAX_PID; i++)
    {
        if (i != mypid) sys_kill (i, 9 /* SIGKILL */);
    }

    sys_write (out, MSG_DONE, sizeof(MSG_DONE)-1);
}

static void close_all_loops()
{
    char tmppath[] = REAL_LOOP;

    sys_write (out, MSG_CLOSING_LOOPS, sizeof(MSG_CLOSING_LOOPS)-1);

    for (unsigned char i = 0; i < 9; i++)
    {
        int loopfd;
        tmppath[(sizeof(REAL_LOOP)-2)] = ('0' + (i));

        if ((loopfd = sys_open(tmppath, 0 /* O_RDONLY */, 0)) >= 0)
        {
            sys_ioctl(loopfd, 0x4C01 /* LOOP_CLR_FD */, 0);

            sys_close(loopfd);
        }
    }

    sys_write (out, MSG_DONE, sizeof(MSG_DONE)-1);
}

static void prune_file_descriptors()
{
    sys_write (out, MSG_PRUNE, sizeof(MSG_PRUNE)-1);

    for (int i = 0; i < 4096 * 8; i++) {
        sys_close (i);
    }
}

static void reopen_stdout()
{
    out = sys_open (CONSOLE, 1 /* O_WRONLY */, 0); /* open stdout */

    sys_write (out, MSG_REOPEN, sizeof(MSG_REOPEN)-1);
}

static void prune ()
{
    prune_file_descriptors();
    reopen_stdout();
}

static void create_loops ()
{
    char tmppath[] = LRTMP_LOOP;

    for (unsigned char i = 0; i < 9; i++) {
        tmppath[(sizeof(LRTMP_LOOP)-2)] = ('0' + (i));

        sys_mknod (tmppath, 0x6000 /* S_IFBLK */, (((7) << 8) | (i)));
    }
}

static void lastrites()
{
    if (sys_mount("lastrites", LRTMPPATH, "tmpfs", 0, "")) {
        sys_write (out, MSG_MOUNT_TMPFS_FAILED, sizeof(MSG_MOUNT_TMPFS_FAILED)-1);
    }

    sys_mkdir(LRTMPPATH "/old", 0777);
    sys_mkdir(LRTMPPATH "/proc", 0777);
    sys_mkdir(LRTMPPATH "/dev", 0777);
    sys_mkdir(LRTMPPATH "/dev/loop", 0777);
    sys_mkdir(LRTMPPATH "/tmp", 0777);

    create_loops ();

    sys_mknod (LRTMPPATH "/dev/console", 0x2000 /* S_IFCHR */, ((5 << 8) | 1));

    if (sys_mount("lastrites-proc", LRTMPPATH "/proc", "proc", 0, ""))
    {
        sys_write (out, MSG_MOUNT_PROC_FAILED, sizeof(MSG_MOUNT_PROC_FAILED)-1);
    }

    sys_chdir(LRTMPPATH "/old");

    if (sys_pivot_root(LRTMPPATH, LRTMPPATH "/old"))
    {
        sys_write (out, MSG_PIVOT_ROOT_FAILED, sizeof(MSG_PIVOT_ROOT_FAILED)-1);
    }

    sys_chdir("/");

    char max_retries = MAX_RETRIES;

    prune ();

    do {
        max_retries--;

        sys_sync();
        kill_everything();
        sys_sync();
        close_all_loops();
        sys_sync();
        sleep (1);
    } while (unmount_everything() && max_retries);
}

static void ignore_signals()
{
    unsigned long long mask = (long)~0;
    /* the kernel is using 64-bit integers for this. in c99, unsigned long longs
       are required to be at least 64-bit last i checked. */

    /* seems like getting to SIG_BLOCK isn't quite that easy, but upon examining
       the kernel headers, i noticed that SIGBLOCK is always either 0 or 1, and
       SIG_UNBLOCK is always either 1 or 2, so calling sigprocmask with 1 first
       and then 0 will either unblock everything and then block everything, or
       block everything and then get rejected as invalid. it's a bitch. */
#if defined(have_sys_sigprocmask)
    sys_sigprocmask (1, &mask, (void *)0);
    sys_sigprocmask (0, &mask, (void *)0);
#else
    sys_rt_sigprocmask (1, &mask, (void *)0, 8 /* 64 bit */);
    sys_rt_sigprocmask (0, &mask, (void *)0, 8 /* 64 bit */);
#endif
}

int cmain ()
{
    char action = curie_argv[1] ? curie_argv[1][0] : '?';

#if defined(have_sys_setsid)
    sys_setsid();
#endif

    ignore_signals();

    sys_write (out, MSG_INTRO, sizeof(MSG_INTRO) -1);

    prune ();

    lastrites();

    switch (action) {
        case 'k':
#if defined(LINUX_REBOOT_MAGIC1) && defined (LINUX_REBOOT_MAGIC2) && defined (LINUX_REBOOT_CMD_KEXEC) && defined (__NR_reboot)
            sys_reboot (LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                        LINUX_REBOOT_CMD_KEXEC, 0);
            sys_write (out, MSG_KEXEC_FAILED, sizeof(MSG_KEXEC_FAILED)-1);
#else
            sys_write (out, MSG_NO_KEXEC, sizeof(MSG_NO_KEXEC)-1);
#endif

        case 'r':
            sys_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                       LINUX_REBOOT_CMD_RESTART, 0);
            sys_write (out, MSG_CANT_REBOOT, sizeof(MSG_CANT_REBOOT)-1);

        case 'd':
            sys_reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
                       LINUX_REBOOT_CMD_POWER_OFF, 0);
            sys_write (out, MSG_CANT_SHUT_DOWN, sizeof(MSG_CANT_SHUT_DOWN)-1);

        default:
            sys_write (out, MSG_EXITING, sizeof(MSG_EXITING)-1);
    }

    return 0;
}
