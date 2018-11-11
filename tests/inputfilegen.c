/* generate an input file suitable for use by the testbench
 * Copyright (C) 2018 by Rainer Gerhards and Adiscon GmbH.
 * Copyright (C) 2016-2018 by Pascal Withopf and Adiscon GmbH.
 *
 * usage: ./inputfilegen num-lines > file
 * -m number of messages
 * -i start number of message
 * -d extra data to add (all 'X' after the msgnum)
 * -s size of file to generate
 *  cannot be used together with -m
 *  size may be slightly smaller in order to be able to write
 *  the last complete line.
 * -M "message count file" - contains number of messages to be written
 *  This is especially useful with -s, as the testbench otherwise does
 *  not know how to do a seq_check. To keep things flexible, can also be
 *  used with -m (this may aid testbench framework generalization).
 * Part of rsyslog, licensed under ASL 2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#if defined(_AIX)
	#include  <unistd.h>
#else
	#include <getopt.h>
#endif

#define DEFMSGS 5
#define NOEXTRADATA -1

int main(int argc, char* argv[])
{
	int c, i;
	long long nmsgs = DEFMSGS;
	long long nmsgstart = 0;
	int nchars = NOEXTRADATA;
	int errflg = 0;
	long long filesize = -1;
	char *extradata = NULL;
	const char *msgcntfile = NULL;

	while((c=getopt(argc, argv, "m:M:i:d:s:")) != -1) {
		switch(c) {
		case 'm':
			nmsgs = atoi(optarg);
			break;
		case 'M':
			msgcntfile = optarg;
			break;
		case 'i':
			nmsgstart = atoi(optarg);
			break;
		case 'd':
			nchars = atoi(optarg);
			break;
		case 's':
			filesize = atoll(optarg);
			break;
		case ':':
			fprintf(stderr, "Option -%c requires an operand\n", optopt);
			errflg++;
			break;
		case '?':
			fprintf(stderr, "Unrecognized option: -%c\n", optopt);
			errflg++;
			break;
		}
	}
	if(errflg) {
		fprintf(stderr, "invalid call\n");
		exit(2);
	}

	if(filesize != -1) {
		const int linesize = (17 + nchars); /* 17 is std line size! */
		nmsgs = filesize / linesize;
		fprintf(stderr, "file size requested %lld, actual %lld with "
			"%lld lines, %lld bytes less\n",
			filesize, nmsgs * linesize, nmsgs, filesize - nmsgs * linesize);
		if(nmsgs > 100000000) {
			fprintf(stderr, "number of lines exhaust 8-digit line numbers "
				"which are standard inside the testbench.\n"
				"Use -d switch to add extra data (e.g. -d111 for "
				"128 byte lines or -d47 for 64 byte lines)\n");
			exit(1);
		}
	}

	if(msgcntfile != NULL) {
		FILE *const fh = fopen(msgcntfile, "w");
		if(fh == NULL) {
			perror(msgcntfile);
			exit(1);
		}
		fprintf(fh, "%lld", nmsgs);
		fclose(fh);
	}

	if(nchars != NOEXTRADATA) {
		extradata = (char *)malloc(nchars + 1);
		memset(extradata, 'X', nchars);
		extradata[nchars] = '\0';
	}
	for(i = nmsgstart; i < (nmsgs+nmsgstart); ++i) {
		printf("msgnum:%8.8d:", i);
		if(nchars != NOEXTRADATA) {
			printf("%s", extradata);
		}
		printf("\n");
	}
	free(extradata);
	return 0;
}
