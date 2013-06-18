/* msggen - a small diagnostic utility that does very quick
 * syslog() calls.
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
#include <syslog.h>

int main(int __attribute__((unused)) argc, char __attribute__((unused)) *argv[])
{
	int i;

	openlog("msggen", 0 , LOG_LOCAL0);

	for(i = 0 ; i < 10 ; ++i)
		syslog(LOG_NOTICE, "This is message number %d", i);

	closelog();
	return 0;
}
