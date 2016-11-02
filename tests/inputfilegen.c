/* generate an input file suitable for use by the testbench
 * Copyright (C) 2011 by Rainer Gerhards and Adiscon GmbH.
 * usage: ./inputfilegen num-lines > file
 * Part of rsyslog, licensed under ASL 2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
	int extradata = 0;
	char *pdata = "";
	int nmsgs;
	int i;

	if(argc < 2 && argc > 3) {
		fprintf(stderr, "usage: inputfilegen num-messages [extra-data]\n");
		return 1;
	}

	nmsgs = atoi(argv[1]);
	if(argc == 3) {
		extradata = atoi(argv[2]);
		if((pdata = malloc(extradata+1)) == NULL) {
			perror("malloc extradata");
			return 2;
		}
		memset(pdata, 'X', extradata);
		pdata[extradata] = '\0';
	}
	for(i = 0 ; i < nmsgs ; ++i) {
		printf("msgnum:%8.8d:%s\n", i, pdata);
	}
	if(argc == 3)
		free(pdata);
	return 0;
}
