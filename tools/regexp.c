/* A simple regular expression checker for rsyslog test and debug.
 * Regular expressions have shown to turn out to be a hot support topic. 
 * While I have done an online tool at http://www.rsyslog.com/tool-regex
 * there are still some situations where one wants to check against the
 * actual clib api calls. This is what this small test program does,
 * it takes its command line arguments (re first, then sample data) and
 * pushes them into the API and then shows the result. This should be
 * considered the ultimate reference for any questions arising.
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
#include <string.h>
#include <sys/types.h>
#include <regex.h>

int main(int argc, char *argv[])
{
	regex_t preg;
	size_t nmatch = 10;
	regmatch_t pmatch[10];
	char *pstr;
	int i;

	if(argc != 3) {
		fprintf(stderr, "usage: regex regexp sample-data\n");
		exit(1);
	}

	pstr = strdup(argv[2]); /* get working copy */

	i = regcomp(&preg, argv[1], REG_EXTENDED);
	printf("regcomp returns %d\n", i);
	i = regexec(&preg, pstr, nmatch, pmatch, 0);
	printf("regexec returns %d\n", i);
	if(i == REG_NOMATCH) {
		printf("found no match!\n");
		return 1;
	}

	printf("returned substrings:\n");
	for(i = 0 ; i < 10 ; i++) {
		printf("%d: so %d, eo %d", i, pmatch[i].rm_so, pmatch[i].rm_eo);
		if(pmatch[i].rm_so != -1) {
			int j;
			printf(", text: '");
			for(j = pmatch[i].rm_so ; j < pmatch[i].rm_eo ; ++j)
				putchar(pstr[j]);
			putchar('\'');
		}
		putchar('\n');
	}
	return 0;
}
