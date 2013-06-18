/*
    pidfile.c - interact with pidfiles
    Copyright (c) 1995  Martin Schulze <Martin.Schulze@Linux.DE>

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

/*
 * Sat Aug 19 13:24:33 MET DST 1995: Martin Schulze
 *	First version (v0.2) released
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#ifdef __sun
#include <fcntl.h>
#endif

#include "srUtils.h"

/* read_pid
 *
 * Reads the specified pidfile and returns the read pid.
 * 0 is returned if either there's no pidfile, it's empty
 * or no pid can be read.
 */
int read_pid (char *pidfile)
{
  FILE *f;
  int pid;

  if (!(f=fopen(pidfile,"r")))
    return 0;
  if(fscanf(f,"%d", &pid) != 1)
  	pid = 0;
  fclose(f);
  return pid;
}

/* check_pid
 *
 * Reads the pid using read_pid and looks up the pid in the process
 * table (using /proc) to determine if the process already exists. If
 * so 1 is returned, otherwise 0.
 */
int check_pid (char *pidfile)
{
  int pid = read_pid(pidfile);

  /* Amazing ! _I_ am already holding the pid file... */
  if ((!pid) || (pid == getpid ()))
    return 0;

  /*
   * The 'standard' method of doing this is to try and do a 'fake' kill
   * of the process.  If an ESRCH error is returned the process cannot
   * be found -- GW
   */
  /* But... errno is usually changed only on error.. */
  if (kill(pid, 0) && errno == ESRCH)
	  return(0);

  return pid;
}

/* write_pid
 *
 * Writes the pid to the specified file. If that fails 0 is
 * returned, otherwise the pid.
 */
int write_pid (char *pidfile)
{
  FILE *f;
  int fd;
  int pid;

  if ( ((fd = open(pidfile, O_RDWR|O_CREAT|O_CLOEXEC, 0644)) == -1)
       || ((f = fdopen(fd, "r+")) == NULL) ) {
      fprintf(stderr, "Can't open or create %s.\n", pidfile);
      return 0;
  }

 /* It seems to be acceptable that we do not lock the pid file
  * if we run under Solaris. In any case, it is highly unlikely
  * that two instances try to access this file. And flock is really
  * causing me grief on my initial steps on Solaris. Some time later,
  * we might re-enable it (or use some alternate method).
  * 2006-02-16 rgerhards
  */

#if HAVE_FLOCK
  if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
      if(fscanf(f, "%d", &pid) != 1)
      	pid = 0;
      fclose(f);
      printf("Can't lock, lock is held by pid %d.\n", pid);
      return 0;
  }
#endif

  pid = getpid();
  if (!fprintf(f,"%d\n", pid)) {
      char errStr[1024];
      rs_strerror_r(errno, errStr, sizeof(errStr));
      printf("Can't write pid , %s.\n", errStr);
      fclose(f);
      return 0;
  }
  fflush(f);

#if HAVE_FLOCK
  if (flock(fd, LOCK_UN) == -1) {
      char errStr[1024];
      rs_strerror_r(errno, errStr, sizeof(errStr));
      printf("Can't unlock pidfile %s, %s.\n", pidfile, errStr);
      fclose(f);
      return 0;
  }
#endif
  fclose(f);

  return pid;
}

/* remove_pid
 *
 * Remove the the specified file. The result from unlink(2)
 * is returned
 */
int remove_pid (char *pidfile)
{
  return unlink (pidfile);
}
  
