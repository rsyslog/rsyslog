/* Checks if a file consists of line of strictly monotonically
 * increasing numbers. An expected start and end number may
 * be set.
 *
 * Params
 * -f<filename> MUST be given!
 * -s<starting number> -e<ending number>
 * default for s is 0. -e should be given (else it is also 0)
 * -d may be specified, in which case duplicate messages are permitted.
 *
 * Part of the testbench for rsyslog.
 *
 * Copyright 2009 Rainer Gerhards and Adiscon GmbH.
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
#include <getopt.h>

int main(int argc, char *argv[])
{
	FILE *fp;
	int val;
	int i;
	int ret = 0;
	int dupsPermitted = 0;
	int start = 0, end = 0;
	int opt;
	int nDups = 0;
	char *file = NULL;

	while((opt = getopt(argc, argv, "e:f:ds:")) != EOF) {
		switch((char)opt) {
		case 'f':
			file = optarg;
			break;
                case 'd':
			dupsPermitted = 1;
			break;
                case 'e':
			end = atoi(optarg);
			break;
                case 's':
			start = atoi(optarg);
			break;
		default:printf("Invalid call of chkseq\n");
			printf("Usage: chkseq file -sstart -eend -d\n");
			exit(1);
		}
	}

	if(file == NULL) {
		printf("file must be given!\n");
		exit(1);
	}

	if(start > end) {
		printf("start must be less than or equal end!\n");
		exit(1);
	}

	/* read file */
	fp = fopen(file, "r");
	if(fp == NULL) {
		printf("error opening file '%s'\n", file);
		perror(file);
		exit(1);
	}

	for(i = start ; i < end+1 ; ++i) {
		if(fscanf(fp, "%d\n", &val) != 1) {
			printf("scanf error in index i=%d\n", i);
			exit(1);
		}
		if(val != i) {
			if(val == i - 1 && dupsPermitted) {
				--i;
				++nDups;
			} else {
				printf("read value %d, but expected value %d\n", val, i);
				exit(1);
			}
		}
	}

	if(nDups != 0)
		printf("info: had %d duplicates (this is no error)\n", nDups);

	if(i - 1 != end) {
		printf("only %d records in file, expected %d\n", i - 1, end);
		exit(1);
	}

	exit(ret);
}
