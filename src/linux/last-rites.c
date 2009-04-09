/*
 * This file is part of the kyuba.org Kyuba project.
 * See the appropriate repository at http://git.kyuba.org/ for exact file
 * modification records.
*/

/*
 * Copyright (c) 2007, 2008, 2009, Kyuba Project Members
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
#define MAX_PID (4096*8 + 1)

#define BUFFERSIZE 0x2000 /* 8kb */
/* seems unlikely that your /proc/mounts is gonna be bigger, and if it is, it's
   gonna run a couple times anyway, so it won't matter */

#define MSG_INTRO " >> Project Kyuba | last rites <<\n"\
     "####################################################################"\
     "###########\n"
#define MSG_PRUNE "pruning all file descriptors...\n"
#define MSG_REOPEN "stdio reopened\n"
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

static int unmount_everything()
{
    int errors    = 0,
        positives = 0,
        fd;

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
                            /* still mounted */

                            if (
#if defined(have_sys_umount)
                                sys_umount(fs_file) &&
#else
                                sys_umount2(fs_file, 0) &&
#endif
                                sys_umount2(fs_file, 1 /* MNT_FORCE */))
                            {
                                /* file failed to unmount */
                                errors++;

                                if (fs_spec && fs_file && fs_vfstype) {
                                    /* try to remount read-only */
                                    sys_mount (fs_spec, fs_file, fs_vfstype,
                                               0x21 /* MS_REMOUNT|MS_RDONLY */,
                                               "");
                                }
                                /* ... else there's been some bad data trying to
                                   parse /proc/mounts */

                                /*
                                 * can't hurt to try this one 
                                 */
                                sys_umount2(fs_file, 4 /* MNT_EXPIRE */);
                                sys_umount2(fs_file, 4 /* MNT_EXPIRE */);
                            } else {
                                positives = 1; /* unmounted successfully */
                            }
                        }
                    }
                }
            } while (rbuffer < ce);
        }

        sys_close(fd);
    }

    if (positives) {
        return unmount_everything();
    }

    return errors;
}

static void kill_everything()
{
    for (int pid = 2; pid < MAX_PID; pid++) { /* don't kill 1 (us) */
        sys_kill(pid, 9 /* SIGKILL */);
    }
}

static void close_all_loops()
{
    char tmppath[] = REAL_LOOP;

    for (unsigned char i = 0; i < 9; i++)
    {
        int loopfd;
        tmppath[(sizeof(LRTMP_LOOP)-2)] = ('0' + (i));

        if ((loopfd = sys_open(tmppath, 2 /* O_RDWR */, 0)) >= 0)
        {
            sys_ioctl(loopfd, 0x4C01 /* LOOP_CLR_FD */, 0);

            sys_close(loopfd);
        }
    }
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
        int ldev = (((7) << 8) | (i));
        tmppath[(sizeof(LRTMP_LOOP)-2)] = ('0' + (i));

        sys_mknod (tmppath, 0x6000 /* S_IFBLK */, ldev);
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

    int ldev = (5 << 8) | 1;
    sys_mknod(LRTMPPATH "/dev/console", 0x2000 /* S_IFCHR */, ldev);
//    ldev = (4 << 8) | 1;
//    sys_mknod(LRTMPPATH "/dev/tty1", 0x2000 /* S_IFCHR */, ldev);
//    ldev = (1 << 8) | 3;
//    sys_mknod(LRTMPPATH "/dev/null", 0x2000 /* S_IFCHR */, ldev);

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

        kill_everything();
        sys_sync();
        close_all_loops();
        sys_sync();
    } while (unmount_everything() && max_retries);
}

int cmain ()
{
    char action = curie_argv[1] ? curie_argv[1][0] : '?';

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