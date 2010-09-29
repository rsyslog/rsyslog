/* A very primitive testing tool that just emits a number of
 * messages to the system log socket. Currently sufficient, but
 * obviously room for improvement. 
 *
 * It is highly suggested NOT to "base" any derivative work
 * on this tool ;)
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2010 Rainer Gerhards and Adiscon GmbH.
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
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
	int i;
	int sev = 0;
	if(argc != 2) {
		fprintf(stderr, "usage: syslog_caller num-messages\n");
		exit(1);
	}

	int msgs = atoi(argv[1]);

	for(i = 0 ; i < msgs ; ++i) {
		syslog(sev % 8, "test message nbr %d, severity=%d", i, sev % 8);
		sev++;
	}
}
