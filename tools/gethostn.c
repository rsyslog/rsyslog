/* gethostn - a small diagnostic utility to show what the
 * gethostname() API returns. Of course, this tool duplicates
 * functionality already found in other tools. But the point is
 * that the API shall be called by a program that is compiled like
 * rsyslogd and does exactly what rsyslog does.
 *
 * Copyright 2008 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int __attribute__((unused)) argc, char __attribute__((unused)) *argv[])
{
	char hostname[4096]; /* this should always be sufficient ;) */
	int err;

	err = gethostname(hostname, sizeof(hostname));

	if(err) {
		perror("gethostname failed");
		exit(1);
	}

	printf("hostname of this system is '%s'.\n", hostname);

	return 0;
}
