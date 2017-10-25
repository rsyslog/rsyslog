/* A testing tool that just emits a single message to
 * the journal. Very basic, up to the task in need.
 * The tool is needed on systems where logger by default does
 * not log to the journal, but where the journal is present.
 * messages to the system log socket.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2017 Rainer Gerhards and Adiscon GmbH.
 *
 * This file is part of rsyslog.
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
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <systemd/sd-journal.h>

int main(int argc, char *argv[])
{
	if(argc != 2) {
		fprintf(stderr, "usage: journal_print \"message\"\n");
		exit(1);
	}

	int r = sd_journal_print(LOG_INFO, "%s", argv[1]);
	if(r < 0) {
		errno = -r;
		perror("writing to the journal");
	} else {
		printf("Written to journal: \"%s\"\n", argv[1]);
	}

	return(0);
}
