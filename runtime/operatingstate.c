/* OperatingStateFile Handler.
 *
 * Copyright 2018 Adiscon GmbH.
 *
 * This file is part of the rsyslog runtime library.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *       -or-
 *       see COPYING.ASL20 in the source distribution
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "rsyslog.h"
#include "errmsg.h"
#include "operatingstate.h"
#include "rsconf.h"

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#ifndef HAVE_LSEEK64
#  define lseek64(fd, offset, whence) lseek(fd, offset, whence)
#endif

/* some important standard states */
#define STATE_INITIALIZING  "INITIALIZING"
#define STATE_CLEAN_CLOSE   "CLEAN CLOSE"

static int fd_osf = -1;

/* check if old osf points to a problem and, if so, report */
static void
osf_checkOnStartup(void)
{
    int do_rename = 1;
    const char *fn_osf = (const char*) glblGetOperatingStateFile(loadConf);
    char iobuf[sizeof(STATE_CLEAN_CLOSE)];
    const int len_clean_close = sizeof(STATE_CLEAN_CLOSE) - 1;
    assert(fn_osf != NULL);

    const int fd = open(fn_osf, O_RDONLY|O_LARGEFILE|O_CLOEXEC, 0);
    if(fd == -1) {
        if(errno != ENOENT) {
            LogError(errno, RS_RET_ERR, "error opening existing operatingStateFile '%s' - "
                "this may be an indication of a problem; ignoring", fn_osf);
        }
        do_rename = 0;
        goto done;
    }
    assert(fd != -1);
    int offs = lseek64(fd, -(len_clean_close + 1), SEEK_END);
    if(offs == -1){
        LogError(errno, RS_RET_IO_ERROR, "error seeking to end of existing operatingStateFile "
            "'%s' - this may be an indication of a problem, e.g. empty file", fn_osf);
        goto done;
    }
    int rd = read(fd, iobuf, len_clean_close);
    if(rd == -1) {
        LogError(errno, RS_RET_IO_ERROR, "error reading existing operatingStateFile "
            "'%s' - this probably indicates an improper shutdown", fn_osf);
        goto done;
    } else {
        assert(rd <= len_clean_close);
        iobuf[rd] = '\0';
        if(rd != len_clean_close || strcmp(iobuf, STATE_CLEAN_CLOSE) != 0) {
            LogError(errno, RS_RET_IO_ERROR, "existing operatingStateFile '%s' does not end "
                "with '%s, instead it has '%s' - this probably indicates an "
                "improper shutdown", fn_osf, STATE_CLEAN_CLOSE, iobuf);
            goto done;
        }
    }

    /* all ok! */
    do_rename = 0;
done:
    if(fd != -1) {
        close(fd);
    }
    if(do_rename) {
        char newname[MAXFNAME];
        snprintf(newname, sizeof(newname)-1, "%s.previous", fn_osf);
        newname[MAXFNAME-1] = '\0';
        if(rename(fn_osf, newname) != 0) {
            LogError(errno, RS_RET_IO_ERROR, "could not rename existing operatingStateFile "
                "'%s' to '%s' - ignored, but will probably be overwritten now",
                fn_osf, newname);
        } else {
            LogMsg(errno, RS_RET_OK, LOG_INFO, "existing state file '%s' renamed "
                "to '%s' - you may want to review it", fn_osf, newname);
        }
    }
    return;
}

void
osf_open(void)
{
    assert(fd_osf == -1);
    const char *fn_osf = (const char*) glblGetOperatingStateFile(loadConf);
    assert(fn_osf != NULL);

    osf_checkOnStartup();

    fd_osf = open(fn_osf, O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
        S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if(fd_osf == -1) {
        LogError(errno, RS_RET_ERR, "error opening operatingStateFile '%s' for write - "
            "ignoring it", fn_osf);
        goto done;
    }
    assert(fd_osf != -1);

    osf_write(OSF_TAG_STATE, STATE_INITIALIZING " " VERSION);
done:
    return;
}


void ATTR_NONNULL()
osf_write(const char *const tag, const char *const line)
{
    char buf[1024]; /* intentionally small */
    time_t tt;
    ssize_t wr;
    size_t len;
    struct tm tm;

    DBGPRINTF("osf: %s %s: ", tag, line); /* ensure everything is inside the debug log */

    if(fd_osf == -1)
        return;

    time(&tt);
    localtime_r(&tt, &tm);
    len = snprintf(buf, sizeof(buf)-1, "%d%2.2d%2.2d-%2.2d%2.2d%2.2d: %-5.5s %s\n",
        tm.tm_year+1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
        tag, line);
    if(len > sizeof(buf)-1) {
        len = sizeof(buf)-1; /* overflow, truncate */
    }
    wr = write(fd_osf, buf, len);
    // TODO: handle EINTR
    if(wr != (ssize_t) len) {
        LogError(errno, RS_RET_IO_ERROR, "error writing operating state file - line lost");
    }
}


void
osf_close(void)
{
    if(fd_osf == -1)
        return;
    osf_write(OSF_TAG_STATE, STATE_CLEAN_CLOSE);
    close(fd_osf);
}
