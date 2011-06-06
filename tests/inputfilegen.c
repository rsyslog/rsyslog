/* generate an input file suitable for use by the testbench
 * Copyright (C) 2011 by Rainer Gerhards and Adiscon GmbH.
 * usage: ./inputfilegen num-lines > file
 * Part of rsyslog, licensed under GPLv3
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	int nmsgs;
	int i;

	if(argc != 2) {
		fprintf(stderr, "usage: inputfilegen num-messages\n");
		return 1;
	}

	nmsgs = atoi(argv[1]);
	for(i = 0 ; i < nmsgs ; ++i) {
		printf("msgnum:%8.8d:\n", i);
	}
	return 0;
}
