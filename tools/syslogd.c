/**
 * main rsyslog file with GPLv3 content.
 *
 * *********************** NOTE ************************
 * * Do no longer patch this file. If there is hard    *
 * * need to, talk to Rainer as to how we can make any *
 * * patch be licensed under ASL 2.0.                  *
 * * THIS FILE WILL GO AWAY. The new main file is      *
 * * rsyslogd.c.                                       *
 * *****************************************************
 *
 * Please visit the rsyslog project at
 * https://www.rsyslog.com
 * to learn more about it and discuss any questions you may have.
 *
 * rsyslog had initially been forked from the sysklogd project.
 * I would like to express my thanks to the developers of the sysklogd
 * package - without it, I would have had a much harder start...
 *
 * Please note that while rsyslog started from the sysklogd code base,
 * it nowadays has almost nothing left in common with it. Allmost all
 * parts of the code have been rewritten.
 *
 * This Project was intiated and is maintained by
 * Rainer Gerhards <rgerhards@hq.adiscon.com>.
 *
 * rsyslog - An Enhanced syslogd Replacement.
 * Copyright 2003-2016 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
 *
 * Rsyslog is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rsyslog is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Rsyslog.  If not, see <http://www.gnu.org/licenses/>.
 *
 * A copy of the GPL can be found in the file "COPYING" in this distribution.
 */
#include "config.h"
#include "rsyslog.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <errno.h>

#ifdef OS_SOLARIS
    #include <fcntl.h>
    #include <stropts.h>
    #include <sys/termios.h>
    #include <sys/types.h>
#else
    #include <libgen.h>
#endif

#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <grp.h>

#ifdef HAVE_SYS_TIMESPEC_H
    #include <sys/timespec.h>
#endif

#ifdef HAVE_SYS_STAT_H
    #include <sys/stat.h>
#endif

#include <signal.h>

#ifdef HAVE_PATHS_H
    #include <paths.h>
#endif

#include "srUtils.h"
#include "stringbuf.h"
#include "syslogd-types.h"
#include "template.h"
#include "outchannel.h"
#include "syslogd.h"

#include "msg.h"
#include "iminternal.h"
#include "threads.h"
#include "parser.h"
#include "unicode-helper.h"
#include "dnscache.h"
#include "ratelimit.h"

#ifndef HAVE_SETSID
/* stems back to sysklogd in whole */
void untty(void) {
    int i;
    pid_t pid;

    if (!Debug) {
        /* Peng Haitao <penght@cn.fujitsu.com> contribution */
        pid = getpid();
        if (setpgid(pid, pid) < 0) {
            perror("setpgid");
            exit(1);
        }
        /* end Peng Haitao <penght@cn.fujitsu.com> contribution */

        i = open(_PATH_TTY, O_RDWR | O_CLOEXEC);
        if (i >= 0) {
    #if !defined(__hpux)
            (void)ioctl(i, (int)TIOCNOTTY, NULL);
    #else
                /* TODO: we need to implement something for HP UX! -- rgerhards, 2008-03-04 */
                /* actually, HP UX should have setsid, so the code directly above should
                 * trigger. So the actual question is why it doesn't do that...
                 */
    #endif
            close(i);
        }
    }
}
#endif
