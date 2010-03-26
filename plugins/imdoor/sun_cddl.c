#define MAXLINE 4096
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/* Portions Copyright 2010 by Rainer Gerhards and Adiscon
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 *      Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *      All Rights Reserved
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */
#include <unistd.h>
#include <note.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <libscf.h>
#include <netconfig.h>
#include <netdir.h>
#include <pwd.h>
#include <sys/socket.h>
#include <tiuser.h>
#include <utmpx.h>
#include <limits.h>
#include <pthread.h>
#include <fcntl.h>
#include <stropts.h>
#include <assert.h>
#include <sys/statvfs.h>

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/syslog.h>
#include <sys/strlog.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/note.h>
#include <door.h>
#include <sys/door.h>

#include "rsyslog.h"
#include "debug.h"

#define	DOORFILE		"/var/run/syslog_door"
#define	RELATIVE_DOORFILE	"../var/run/syslog_door"
#define	OLD_DOORFILE		"/etc/.syslog_door"

static int		DoorFd = -1;
static int		DoorCreated = 0;
static char		*DoorFileName = DOORFILE;

/* for managing door server threads */
static pthread_mutex_t door_server_cnt_lock = PTHREAD_MUTEX_INITIALIZER;
static uint_t door_server_cnt = 0;
static pthread_attr_t door_thr_attr;

/*
 * the 'server' function that we export via the door. It does
 * nothing but return.
 */
/*ARGSUSED*/
static void
server(void *cookie, char *argp, size_t arg_size,
    door_desc_t *dp, uint_t n)
{
	(void) door_return(NULL, 0, NULL, 0);
	/* NOTREACHED */
}

/*ARGSUSED*/
static void *
create_door_thr(void *arg)
{
	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	(void) door_return(NULL, 0, NULL, 0);

	/*
	 * If there is an error in door_return(), it will return here and
	 * the thread will exit. Hence we need to decrement door_server_cnt.
	 */
	(void) pthread_mutex_lock(&door_server_cnt_lock);
	door_server_cnt--;
	(void) pthread_mutex_unlock(&door_server_cnt_lock);
	return (NULL);
}

/*
 * Max number of door server threads for syslogd. Since door is used
 * to check the health of syslogd, we don't need large number of
 * server threads.
 */
#define	MAX_DOOR_SERVER_THR	3

/*
 * Manage door server thread pool.
 */
/*ARGSUSED*/
static void
door_server_pool(door_info_t *dip)
{
	(void) pthread_mutex_lock(&door_server_cnt_lock);
	if (door_server_cnt <= MAX_DOOR_SERVER_THR &&
	    pthread_create(NULL, &door_thr_attr, create_door_thr, NULL) == 0) {
		door_server_cnt++;
		(void) pthread_mutex_unlock(&door_server_cnt_lock);
		return;
	}

	(void) pthread_mutex_unlock(&door_server_cnt_lock);
}

static void
delete_doorfiles(void)
{
	pthread_t mythreadno;
	struct stat sb;
	int err;
	char line[MAXLINE+1];

	if (Debug) {
		mythreadno = pthread_self();
	}


	if (lstat(DoorFileName, &sb) == 0 && !S_ISDIR(sb.st_mode)) {
		if (unlink(DoorFileName) < 0) {
			err = errno;
			(void) snprintf(line, sizeof (line),
			    "unlink() of %s failed - fatal", DoorFileName);
			errno = err;
			DBGPRINTF("%s", line);//logerror(line);
			DBGPRINTF("delete_doorfiles(%u): error: %s, "
			    "errno=%d\n", mythreadno, line, err);
			exit(1);
		}

		DBGPRINTF("delete_doorfiles(%u): deleted %s\n",
		    mythreadno, DoorFileName);
	}

	if (strcmp(DoorFileName, DOORFILE) == 0) {
		if (lstat(OLD_DOORFILE, &sb) == 0 && !S_ISDIR(sb.st_mode)) {
			if (unlink(OLD_DOORFILE) < 0) {
				err = errno;
				(void) snprintf(line, sizeof (line),
				    "unlink() of %s failed", OLD_DOORFILE);
				DBGPRINTF("delete_doorfiles(%u): %s\n",
				    mythreadno, line);

				if (err != EROFS) {
					errno = err;
					(void) strlcat(line, " - fatal",
					    sizeof (line));
					logerror(line);
					DBGPRINTF("delete_doorfiles(%u): "
					    "error: %s, errno=%d\n",
					    mythreadno, line, err);
					exit(1);
				}

				DBGPRINTF("delete_doorfiles(%u): unlink() "
				    "failure OK on RO file system\n",
				    mythreadno);
			}

			DBGPRINTF("delete_doorfiles(%u): deleted %s\n",
			    mythreadno, OLD_DOORFILE);
		}
	}

#if 0
	if (lstat(PidFileName, &sb) == 0 && !S_ISDIR(sb.st_mode)) {
		if (unlink(PidFileName) < 0) {
			err = errno;
			(void) snprintf(line, sizeof (line),
			    "unlink() of %s failed - fatal", PidFileName);
			errno = err;
			logerror(line);
			DBGPRINTF("delete_doorfiles(%u): error: %s, "
			    "errno=%d\n", mythreadno, line, err);
			exit(1);
		}

		DBGPRINTF("delete_doorfiles(%u): deleted %s\n", mythreadno,
		    PidFileName);
	}

	if (strcmp(PidFileName, PIDFILE) == 0) {
		if (lstat(OLD_PIDFILE, &sb) == 0 && !S_ISDIR(sb.st_mode)) {
			if (unlink(OLD_PIDFILE) < 0) {
				err = errno;
				(void) snprintf(line, sizeof (line),
				    "unlink() of %s failed", OLD_PIDFILE);
				DBGPRINTF(5, "delete_doorfiles(%u): %s, \n",
				    mythreadno, line);

				if (err != EROFS) {
					errno = err;
					(void) strlcat(line, " - fatal",
					    sizeof (line));
					logerror(line);
					DBGPRINTF(1, "delete_doorfiles(%u): "
					    "error: %s, errno=%d\n",
					    mythreadno, line, err);
					exit(1);
				}

				DBGPRINTF(5, "delete_doorfiles(%u): unlink "
				    "failure OK on RO file system\n",
				    mythreadno);
			}

			DBGPRINTF(5, "delete_doorfiles(%u): deleted %s\n",
			    mythreadno, OLD_PIDFILE);
		}
	}
#endif

	if (DoorFd != -1) {
		(void) door_revoke(DoorFd);
	}

	DBGPRINTF("delete_doorfiles(%u): revoked door: DoorFd=%d\n",
	    mythreadno, DoorFd);
}


/*
 * Create the door file and the pid file in /var/run.  If the filesystem
 * containing /etc is writable, create symlinks /etc/.syslog_door and
 * /etc/syslog.pid to them.  On systems that do not support /var/run, create
 * /etc/.syslog_door and /etc/syslog.pid directly.
 *
 * Note: it is not considered fatal to fail to create the pid file or its
 * symlink.  Attempts to use them in the usual way will fail, of course, but
 * syslogd will function nicely without it (not so for the door file).
 */

static void
sun_open_door(void)
{
	struct stat buf;
	door_info_t info;
	char line[MAXLINE+1];
	pthread_t mythreadno;
	int err;

	if (Debug) {
		mythreadno = pthread_self();
	}

	/*
	 * first see if another syslogd is running by trying
	 * a door call - if it succeeds, there is already
	 * a syslogd process active
	 */

	if (!DoorCreated) {
		int door;

		if ((door = open(DoorFileName, O_RDONLY)) >= 0) {
			DBGPRINTF("open_door(%u): %s opened "
			    "successfully\n", mythreadno, DoorFileName);

			if (door_info(door, &info) >= 0) {
				DBGPRINTF("open_door(%u): "
				    "door_info:info.di_target = %ld\n",
				    mythreadno, info.di_target);

				if (info.di_target > 0) {
					(void) sprintf(line, "syslogd pid %ld"
					    " already running. Cannot "
					    "start another syslogd pid %ld",
					    info.di_target, getpid());
					DBGPRINTF("open_door(%u): error: "
					    "%s\n", mythreadno, line);
					errno = 0;
					//logerror(line);
					exit(1);
				}
			}

			(void) close(door);
		} else {
			if (lstat(DoorFileName, &buf) < 0) {
				err = errno;

				DBGPRINTF("open_door(%u): lstat() of %s "
				    "failed, errno=%d\n",
				    mythreadno, DoorFileName, err);

				if ((door = creat(DoorFileName, 0644)) < 0) {
					err = errno;
					(void) snprintf(line, sizeof (line),
					    "creat() of %s failed - fatal",
					    DoorFileName);
					DBGPRINTF("open_door(%u): error: %s, "
					    "errno=%d\n", mythreadno, line,
					    err);
					errno = err;
					//logerror(line);
					delete_doorfiles();
					exit(1);
				}

				(void) fchmod(door,
				    S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

				DBGPRINTF("open_door(%u): creat() of %s "
				    "succeeded\n", mythreadno,
				    DoorFileName);

				(void) close(door);
			}
		}

		if (strcmp(DoorFileName, DOORFILE) == 0) {
			if (lstat(OLD_DOORFILE, &buf) == 0) {
				DBGPRINTF("open_door(%u): lstat() of %s "
				    "succeeded\n", mythreadno,
				    OLD_DOORFILE);

				if (S_ISDIR(buf.st_mode)) {
					(void) snprintf(line, sizeof (line),
					    "%s is a directory - fatal",
					    OLD_DOORFILE);
					DBGPRINTF("open_door(%u): error: "
					    "%s\n", mythreadno, line);
					errno = 0;
					//logerror(line);
					delete_doorfiles();
					exit(1);
				}

				DBGPRINTF("open_door(%u): %s is not a "
				    "directory\n",
				    mythreadno, OLD_DOORFILE);

				if (unlink(OLD_DOORFILE) < 0) {
					err = errno;
					(void) snprintf(line, sizeof (line),
					    "unlink() of %s failed",
					    OLD_DOORFILE);
					DBGPRINTF("open_door(%u): %s\n",
					    mythreadno, line);

					if (err != EROFS) {
						DBGPRINTF("open_door(%u): "
						    "error: %s, "
						    "errno=%d\n",
						    mythreadno, line, err);
						(void) strcat(line, " - fatal");
						errno = err;
						//logerror(line);
						delete_doorfiles();
						exit(1);
					}

					DBGPRINTF("open_door(%u): unlink "
					    "failure OK on RO file "
					    "system\n", mythreadno);
				}
			} else {
				DBGPRINTF("open_door(%u): file %s doesn't "
				    "exist\n", mythreadno, OLD_DOORFILE);
			}

			if (symlink(RELATIVE_DOORFILE, OLD_DOORFILE) < 0) {
				err = errno;
				(void) snprintf(line, sizeof (line),
				    "symlink %s -> %s failed", OLD_DOORFILE,
				    RELATIVE_DOORFILE);
				DBGPRINTF("open_door(%u): %s\n", mythreadno,
				    line);

				if (err != EROFS) {
					DBGPRINTF("open_door(%u): error: %s, "
					    "errno=%d\n", mythreadno, line,
					    err);
					errno = err;
					(void) strcat(line, " - fatal");
					//logerror(line);
					delete_doorfiles();
					exit(1);
				}

				DBGPRINTF("open_door(%u): symlink failure OK "
				    "on RO file system\n", mythreadno);
			} else {
				DBGPRINTF("open_door(%u): symlink %s -> %s "
				    "succeeded\n", mythreadno,
				    OLD_DOORFILE, RELATIVE_DOORFILE);
			}
		}

		if ((DoorFd = door_create(server, 0,
		    DOOR_REFUSE_DESC)) < 0) {
		    //???? DOOR_NO_CANEL requires newer libs??? DOOR_REFUSE_DESC | DOOR_NO_CANCEL)) < 0) {
			err = errno;
			(void) sprintf(line, "door_create() failed - fatal");
			DBGPRINTF("open_door(%u): error: %s, errno=%d\n",
			    mythreadno, line, err);
			errno = err;
			//logerror(line);
			delete_doorfiles();
			exit(1);
		}
		//???? (void) door_setparam(DoorFd, DOOR_PARAM_DATA_MAX, 0);
		DBGPRINTF("open_door(%u): door_create() succeeded, "
		    "DoorFd=%d\n", mythreadno, DoorFd);

		DoorCreated = 1;
	}

	(void) fdetach(DoorFileName);	/* just in case... */

	(void) door_server_create(door_server_pool);

	if (fattach(DoorFd, DoorFileName) < 0) {
		err = errno;
		(void) snprintf(line, sizeof (line), "fattach() of fd"
		    " %d to %s failed - fatal", DoorFd, DoorFileName);
		DBGPRINTF("open_door(%u): error: %s, errno=%d\n", mythreadno,
		    line, err);
		errno = err;
		//logerror(line);
		delete_doorfiles();
		exit(1);
	}

	DBGPRINTF("open_door(%u): attached server() to %s\n", mythreadno,
	    DoorFileName);

#if 0
	/*
	 * create pidfile anyway, so those using it to control
	 * syslogd (with kill `cat /etc/syslog.pid` perhaps)
	 * don't get broken.
	 */

	if (!PidfileCreated) {
		int pidfd;

		PidfileCreated = 1;

		if ((pidfd = open(PidFileName, O_RDWR|O_CREAT|O_TRUNC, 0644))
		    < 0) {
			err = errno;
			(void) snprintf(line, sizeof (line),
			    "open() of %s failed", PidFileName);
			DBGPRINTF(1, "open_door(%u): warning: %s, errno=%d\n",
			    mythreadno, line, err);
			errno = err;
			//logerror(line);
			return;
		}

		(void) fchmod(pidfd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		(void) sprintf(line, "%ld\n", getpid());

		if (write(pidfd, line, strlen(line)) < 0) {
			err = errno;
			(void) snprintf(line, sizeof (line),
			    "write to %s on fd %d failed", PidFileName, pidfd);
			DBGPRINTF(1, "open_door(%u): warning: %s, errno=%d\n",
			    mythreadno, line, err);
			errno = err;
			//logerror(line);
			return;
		}

		(void) close(pidfd);

		DBGPRINTF("open_door(%u): %s created\n",
		    mythreadno, PidFileName);

		if (strcmp(PidFileName, PIDFILE) == 0) {
			if (lstat(OLD_PIDFILE, &buf) == 0) {
				DBGPRINTF("open_door(%u): lstat() of %s "
				    "succeded\n", mythreadno, OLD_PIDFILE);

				if (S_ISDIR(buf.st_mode)) {
					(void) snprintf(line, sizeof (line),
					    "file %s is a directory",
					    OLD_PIDFILE);
					DBGPRINTF("open_door(%u): warning: "
					    "%s\n", mythreadno, line);
					errno = 0;
					//logerror(line);
					return;
				}

				if (unlink(OLD_PIDFILE) < 0) {
					err = errno;
					(void) snprintf(line, sizeof (line),
					    "unlink() of %s failed",
					    OLD_PIDFILE);
					DBGPRINTF("open_door(%u): %s\n",
					    mythreadno, line);

					if (err != EROFS) {
						DBGPRINTF(1, "open_door (%u): "
						    "warning: %s, "
						    "errno=%d\n",
						    mythreadno, line, err);
						errno = err;
						//logerror(line);
						return;
					}

					DBGPRINTF(5, "open_door(%u): unlink "
					    "failure OK on RO file "
					    "system\n", mythreadno);
				}
			} else {
				DBGPRINTF("open_door(%u): file %s doesn't "
				    "exist\n", mythreadno, OLD_PIDFILE);
			}

			if (symlink(RELATIVE_PIDFILE, OLD_PIDFILE) < 0) {
				err = errno;
				(void) snprintf(line, sizeof (line),
				    "symlink %s -> %s failed", OLD_PIDFILE,
				    RELATIVE_PIDFILE);
				DBGPRINTF("open_door(%u): %s\n", mythreadno,
				    line);

				if (err != EROFS) {
					DBGPRINTF(1, "open_door(%u): warning: "
					    "%s, errno=%d\n", mythreadno,
					    line, err);
					errno = err;
					//logerror(line);
					return;
				}

				DBGPRINTF(5, "open_door(%u): symlink failure OK "
				    "on RO file system\n", mythreadno);
				return;
			}

			DBGPRINTF(5, "open_door(%u): symlink %s -> %s "
			    "succeeded\n", mythreadno, OLD_PIDFILE,
			    RELATIVE_PIDFILE);
		}
	}
#endif
}


