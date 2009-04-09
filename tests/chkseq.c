/* Checks if a file consists of line of strictly monotonically
 * increasing numbers. An expected start and end number may
 * be set.
 *
 * Params
 * argv[1]	file to check
 * argv[2]	start number
 * argv[3]	end number
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

int main(int argc, char *argv[])
{
	FILE *fp;
	int val;
	int i;
	int ret = 0;
	int start, end;

	if(argc != 4) {
		printf("Invalid call of chkseq\n");
		printf("Usage: chkseq file start end\n");
		exit(1);
	}
	
	start = atoi(argv[2]);
	end = atoi(argv[3]);

	if(start > end) {
		printf("start must be less than or equal end!\n");
		exit(1);
	}

	/* read file */
	fp = fopen(argv[1], "r");
	if(fp == NULL) {
		perror(argv[1]);
		exit(1);
	}

	for(i = start ; i < end ; ++i) {
		if(fscanf(fp, "%d\n", &val) != 1) {
			printf("scanf error in index i=%d\n", i);
			exit(1);
		}
		if(val != i) {
			printf("read value %d, but expected value %d\n", val, i);
			exit(1);
		}
	}

	exit(ret);
}
